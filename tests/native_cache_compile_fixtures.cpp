#include "native_joint_reader.h"
#include <cassert>
#include <functional>
#include <map>

using namespace native_joints;
namespace d = native_joints::detail;

// These checks are evaluated by the compiler; they do not run a test binary.
static_assert(d::base_request(0x100) == 0x100);
static_assert(d::base_request(0) == 0xfffff);
static_assert(d::effective_request(0x100, 0, 0x100, 0, -1, 0) == 0x100);
static_assert(d::effective_request(0x100, 0x50000, 0, 0, -1, 0) == 0x100);
static_assert(d::effective_request(0x100, 0x50000, 0x100, 0, 0, 1) == 0x108);
static_assert(d::effective_request(0x100, 0x50000, 0x100, 1, 0, 0) == 0x100);
static_assert(d::effective_request(0x100, 0x50000, 0x100, 1, -1, 0) == 0x108);
static_assert(d::effective_request(0x100, 0x50000, 0x100, 1, -1, 1) == 0x100);
static_assert(d::effective_request(0x100, 0x50000, 0x100, 1, -1, 2) == 0x108);
static_assert(d::effective_request(0xfffff, 0x50000, 0x100, 0, -1, 0) == 0xfffff);
static_assert(d::mask_ready(0x108, 0x100));
static_assert(!d::mask_ready(0x100, 0x108));
static_assert(d::valid_count(65535, 65535));
static_assert(!d::valid_count(65536, 100000));
static_assert(!d::valid_count(0x10001, 1024));
static_assert(!d::valid_count(-1, 1024));
static_assert(!d::valid_count(0, 1024));
static_assert(d::valid_span(0x10000, 1));
static_assert(!d::valid_span(0xffff, 1));
static_assert(d::valid_span(d::maximum_user_address, 1));
static_assert(!d::valid_span(d::maximum_user_address, 2));
static_assert(!d::valid_span(UINT64_MAX, 32));
static_assert(d::finite(0.f));
static_assert(d::finite(std::bit_cast<float>(0x80000000U)));
static_assert(!d::finite(std::bit_cast<float>(0x7f800000U)));
static_assert(!d::finite(std::bit_cast<float>(0x7fc00000U)));
static_assert(!d::invalidates_metadata(Failure::NoDemand));
static_assert(!d::invalidates_metadata(Failure::Locked));
static_assert(!d::invalidates_metadata(Failure::CacheNotReady));
static_assert(!d::invalidates_metadata(Failure::Inconsistent));
static_assert(!d::invalidates_metadata(Failure::ReadFailed));
static_assert(d::invalidates_metadata(Failure::IdentityMismatch));
static_assert(d::invalidates_metadata(Failure::InvalidModel));
static_assert(d::invalidates_metadata(Failure::InvalidCount));
static_assert(d::invalidates_metadata(Failure::MetadataChanged));
static_assert(d::invalidates_metadata(Failure::InvalidName));

// Complete synthetic fresh-memory fixture. The functions below are compiled but
// deliberately not executed in this task. They are available for later permitted
// host-only execution, with no game, hardware, DLL or target-process dependency.
class SyntheticSource final : public FreshSource {
public:
    std::map<Address, std::uint8_t> memory;
    std::uint64_t epoch = 1, calls = 0;
    bool uncached = true;
    Address partial_address = 0;
    std::function<void(SyntheticSource&, std::span<ReadRequest>)> before_read;
    bool supports_independent_uncached_reads() const noexcept override { return uncached; }
    std::uint64_t binding_epoch() const noexcept override { return epoch; }
    bool read_fresh(std::span<ReadRequest> requests) override {
        ++calls;
        if (before_read) before_read(*this, requests);
        for (auto& r : requests) {
            r.valid = false; r.transferred = 0;
            auto* destination = static_cast<std::uint8_t*>(r.destination);
            for (std::uint32_t i = 0; i != r.size; ++i) {
                if (r.address == partial_address && i + 1 == r.size) break;
                const auto found = memory.find(r.address + i);
                if (found == memory.end()) break;
                destination[i] = found->second; ++r.transferred;
            }
            r.valid = r.transferred == r.size;
        }
        return true;
    }
    template<class T> void put(Address address, const T& value) {
        const auto* p = reinterpret_cast<const std::uint8_t*>(&value);
        for (std::size_t i = 0; i != sizeof(T); ++i) memory[address+i] = p[i];
    }
    void put_name(Address address, std::string_view name) {
        std::array<char, d::name_capacity> bytes{};
        assert(name.size() < bytes.size());
        std::copy(name.begin(), name.end(), bytes.begin()); put(address, bytes);
    }
    void erase_span(Address address, std::size_t size) {
        for (std::size_t i = 0; i != size; ++i) memory.erase(address+i);
    }
};

struct Fixture {
    static constexpr Address module = 0x180000000ULL, entity = 0x100000,
        identity = 0x110000, scene = 0x120000, record = 0x130000,
        model = 0x140000, names = 0x150000, flags = 0x160000,
        poses = 0x170000, bitmap = 0x180000, condition_object = 0x190000;
    static constexpr Address head_name = 0x200000, neck_name = 0x210000, root_name = 0x220000;
    static constexpr std::uint32_t handle = 0x81234567;
    SyntheticSource memory;
    Target target{entity, identity, handle};
    Fixture() {
        memory.put<std::uint16_t>(module, 0x5a4d);
        memory.put<std::uint32_t>(module+0x3c, 0x100);
        memory.put<std::uint32_t>(module+0x100, 0x4550);
        memory.put<std::uint16_t>(module+0x104, 0x8664);
        memory.put<std::uint32_t>(module+0x108, version::timestamp);
        memory.put<std::uint16_t>(module+0x118, 0x20b);
        memory.put<std::uint32_t>(module+0x150, version::image_size);
        for (const auto& g : version::guards) {
            auto digit = [](char c) -> unsigned { return c <= '9' ? unsigned(c-'0') : unsigned(c-'a'+10); };
            for (std::size_t i = 0; i != g.hex.size()/2; ++i)
                memory.memory[module+g.rva+i] = static_cast<std::uint8_t>((digit(g.hex[2*i])<<4)|digit(g.hex[2*i+1]));
        }
        memory.put(entity+d::entity_identity_offset, identity);
        memory.put(entity+d::entity_scene_offset, scene);
        memory.put(identity+d::identity_handle_offset, handle);
        memory.put<std::uint32_t>(identity+d::identity_flags_offset, 0);
        memory.put(scene, module+d::scene_vtable_rva);
        memory.put(scene+d::scene_owner_offset, entity);
        memory.put(scene+d::scene_lock_offset, d::Lock{});
        memory.put(scene+d::model_record_offset, record);
        memory.put(record, model);
        memory.put(scene+d::pose_pointer_offset, poses);
        memory.put(scene+d::bitmap_pointer_offset, bitmap);
        memory.put<std::int32_t>(scene+d::pose_count_offset, 3);
        memory.put<std::uint32_t>(scene+d::computed_mask_offset, 0x100);
        memory.put<Address>(scene+d::condition_pointer_offset, 0);
        memory.put<std::uint32_t>(scene+d::condition_mask_offset, 0);
        memory.put<std::int32_t>(model+d::model_count_offset, 3);
        memory.put(model+d::names_pointer_offset, names);
        memory.put(model+d::flags_pointer_offset, flags);
        // Deliberately non-universal order: neck index0, root index1, head index2.
        memory.put(names, neck_name); memory.put(names+8, root_name); memory.put(names+16, head_name);
        memory.put_name(neck_name, "neck_0"); memory.put_name(root_name, "root"); memory.put_name(head_name, "head");
        memory.put<std::uint32_t>(flags, 0x100); memory.put<std::uint32_t>(flags+4, 0); memory.put<std::uint32_t>(flags+8, 0x100);
        memory.put<std::uint32_t>(bitmap, 0x5);
        memory.put(poses, d::NativePose{10,20,30,1,0,0,0,1});
        memory.put(poses+32, d::NativePose{1,2,3,1,0,0,0,1});
        memory.put(poses+64, d::NativePose{40,50,60,1,0,0,0,1});
    }
    void optional8(bool add, std::uint8_t byte = 0) {
        memory.put<Address>(scene+d::condition_pointer_offset, 0x300000);
        memory.put<std::uint32_t>(scene+d::condition_mask_offset, 0x100);
        memory.put(module+d::condition_object_pointer_rva, condition_object);
        memory.put<std::uint8_t>(condition_object+0x58, add ? 0 : 1);
        memory.put<std::int32_t>(module+d::condition_dword_rva, -1);
        memory.put<std::uint8_t>(module+d::condition_byte_rva, byte);
    }
};

[[maybe_unused]] void compile_only_functional_boundary_cases() {
    {
        Fixture f; native_joints::Reader reader(f.memory); assert(reader.bind(Fixture::module));
        const auto sample = reader.sample(f.target, head|neck);
        assert(sample.failure == Failure::None && sample.valid(Joint::Head) && sample.valid(Joint::Neck));
        assert(sample.joints[0].model_index == 2 && sample.joints[1].model_index == 0);
        assert(sample.joints[0].world.x == 40 && sample.joints[0].world.z == 60);
        assert(sample.host_acquisition_started_at != HostClock::time_point{});
        assert(sample.host_acquisition_started_at <= sample.host_acquired_at);
        assert(sample.host_acquired_at != HostClock::time_point{});
        const auto reads = f.memory.calls; assert(reader.sample(f.target, 0).failure == Failure::NoDemand);
        assert(f.memory.calls == reads);
    }
    {
        Fixture f; f.memory.put<std::uint32_t>(Fixture::module+0x108, version::timestamp+1);
        native_joints::Reader reader(f.memory); assert(!reader.bind(Fixture::module));
        assert(reader.last_failure() == Failure::WrongVersion);
    }
    {
        Fixture f; f.memory.memory[Fixture::module+version::guards[0].rva] ^= 1;
        native_joints::Reader reader(f.memory); assert(!reader.bind(Fixture::module));
        assert(reader.last_failure() == Failure::WrongVersion);
    }
    {
        Fixture f; f.memory.uncached = false; native_joints::Reader reader(f.memory);
        assert(!reader.bind(Fixture::module)); assert(reader.last_failure() == Failure::UnsupportedReadContract);
    }
    for (const auto failure : {Failure::IdentityMismatch, Failure::InvalidScene, Failure::InvalidCount,
        Failure::Locked, Failure::CacheNotReady, Failure::NotHitbox, Failure::MissingName,
        Failure::AmbiguousName, Failure::InvalidName, Failure::NonFiniteTransform, Failure::ReadFailed}) {
        Fixture f; native_joints::Reader reader(f.memory); assert(reader.bind(Fixture::module));
        switch (failure) {
        case Failure::IdentityMismatch: f.memory.put<std::uint32_t>(Fixture::identity+d::identity_handle_offset, Fixture::handle+1); break;
        case Failure::InvalidScene: f.memory.put<Address>(Fixture::scene, Fixture::module+d::scene_vtable_rva+8); break;
        case Failure::InvalidCount: f.memory.put<std::int32_t>(Fixture::scene+d::pose_count_offset, 0x10003); break;
        case Failure::Locked: f.memory.put(Fixture::scene+d::scene_lock_offset, d::Lock{3,1,0,12}); break;
        case Failure::CacheNotReady: f.memory.put<std::uint32_t>(Fixture::bitmap, 0x1); break;
        case Failure::NotHitbox: f.memory.put<std::uint32_t>(Fixture::flags+8, 0); break;
        case Failure::MissingName: f.memory.put_name(Fixture::head_name, "unknown_head"); break;
        case Failure::AmbiguousName: f.memory.put_name(Fixture::root_name, "head"); break;
        case Failure::InvalidName: { std::array<char,d::name_capacity> bytes; bytes.fill('x'); f.memory.put(Fixture::head_name,bytes); break; }
        case Failure::NonFiniteTransform: f.memory.put(Fixture::poses+64,d::NativePose{40,50,std::bit_cast<float>(0x7fc00000U),1,0,0,0,1}); break;
        case Failure::ReadFailed: f.memory.partial_address = Fixture::poses+64; break;
        default: break;
        }
        const auto sample = reader.sample(f.target, head|neck);
        assert(sample.failure == failure && !sample.valid(Joint::Head) && !sample.valid(Joint::Neck));
        assert(sample.host_acquisition_started_at == HostClock::time_point{});
        assert(sample.host_acquired_at == HostClock::time_point{});
    }
    {
        Fixture f; f.optional8(true); native_joints::Reader reader(f.memory); assert(reader.bind(Fixture::module));
        assert(reader.sample(f.target,head).failure == Failure::CacheNotReady);
        f.memory.put<std::uint32_t>(Fixture::scene+d::computed_mask_offset,0x108);
        assert(reader.sample(f.target,head).failure == Failure::None);
    }
    {
        Fixture f; f.optional8(false,1); native_joints::Reader reader(f.memory); assert(reader.bind(Fixture::module));
        assert(reader.sample(f.target,head).failure == Failure::None);
    }
    {
        Fixture f; native_joints::Reader reader(f.memory); assert(reader.bind(Fixture::module));
        assert(reader.sample(f.target,head).failure == Failure::None);
        // Selected semantic bytes must be checked even with unchanged pointer/count keys.
        f.memory.put_name(Fixture::head_name,"root");
        assert(reader.sample(f.target,head).failure == Failure::MetadataChanged);
        assert(reader.sample(f.target,head).failure == Failure::MissingName);
    }
    {
        Fixture f; native_joints::Reader reader(f.memory); assert(reader.bind(Fixture::module));
        unsigned pose_reads = 0;
        f.memory.before_read = [&](SyntheticSource& memory, std::span<ReadRequest> requests) {
            for (const auto& r:requests) if (r.address==Fixture::poses+64 && ++pose_reads==2)
                memory.put(Fixture::poses+64,d::NativePose{41,50,60,1,0,0,0,1});
        };
        assert(reader.sample(f.target,head).failure == Failure::Inconsistent);
    }
    {
        Fixture f; native_joints::Reader reader(f.memory); assert(reader.bind(Fixture::module));
        unsigned scene_reads = 0;
        f.memory.before_read = [&](SyntheticSource& memory, std::span<ReadRequest> requests) {
            for (const auto& r:requests) if(r.address==Fixture::entity+d::entity_scene_offset && ++scene_reads==2)
                memory.put<std::uint32_t>(Fixture::scene+d::condition_mask_offset,0x100);
        };
        assert(reader.sample(f.target,head).failure == Failure::Inconsistent);
    }
    {
        Fixture f; native_joints::Reader reader(f.memory); assert(reader.bind(Fixture::module));
        ++f.memory.epoch;
        assert(reader.sample(f.target,head).failure == Failure::BindingChanged);
        assert(reader.sample(f.target,head).failure == Failure::NotBound);
    }
    {
        Fixture f; Limits limits; limits.max_bytes_per_acquisition = 1;
        native_joints::Reader reader(f.memory,limits); assert(!reader.bind(Fixture::module));
        assert(reader.last_failure() == Failure::BudgetExceeded);
    }
    for (const auto temporary : {Failure::NoDemand,Failure::Locked,Failure::CacheNotReady,
        Failure::Inconsistent,Failure::NonFiniteTransform,Failure::ReadFailed}) {
        Fixture f; native_joints::Reader reader(f.memory); assert(reader.bind(Fixture::module));
        assert(reader.sample(f.target,head).failure == Failure::None);
        // A full-name rebuild would now fail on an unrelated bone. Selected
        // head metadata remains complete and must be checked on recovery.
        f.memory.erase_span(Fixture::root_name,d::name_capacity);
        unsigned pose_reads = 0;
        switch (temporary) {
        case Failure::Locked: f.memory.put(Fixture::scene+d::scene_lock_offset,d::Lock{3,1,0,12}); break;
        case Failure::CacheNotReady: f.memory.put<std::uint32_t>(Fixture::scene+d::computed_mask_offset,0); break;
        case Failure::Inconsistent:
            f.memory.before_read = [&](SyntheticSource& memory,std::span<ReadRequest> requests) {
                for (const auto& r:requests) if(r.address==Fixture::poses+64 && ++pose_reads==2)
                    memory.put(Fixture::poses+64,d::NativePose{41,50,60,1,0,0,0,1});
            }; break;
        case Failure::NonFiniteTransform:
            f.memory.put(Fixture::poses+64,d::NativePose{40,50,std::bit_cast<float>(0x7f800000U),1,0,0,0,1}); break;
        case Failure::ReadFailed: f.memory.partial_address = Fixture::poses+64; break;
        default: break;
        }
        const auto failed = reader.sample(f.target,temporary==Failure::NoDemand ? 0 : head);
        assert(failed.failure==temporary && !failed.valid(Joint::Head));
        assert(failed.host_acquisition_started_at==HostClock::time_point{} && failed.host_acquired_at==HostClock::time_point{});
        f.memory.put(Fixture::scene+d::scene_lock_offset,d::Lock{});
        f.memory.put<std::uint32_t>(Fixture::scene+d::computed_mask_offset,0x100);
        f.memory.put(Fixture::poses+64,d::NativePose{40,50,60,1,0,0,0,1});
        f.memory.partial_address=0; f.memory.before_read={};
        assert(reader.sample(f.target,head).failure==Failure::None);
    }
    {
        Fixture f; f.optional8(true);
        f.memory.put<std::uint32_t>(Fixture::scene+d::computed_mask_offset,0x108);
        // Object byte zero short-circuits both remaining global reads.
        f.memory.erase_span(Fixture::module+d::condition_dword_rva,4);
        f.memory.erase_span(Fixture::module+d::condition_byte_rva,1);
        native_joints::Reader reader(f.memory); assert(reader.bind(Fixture::module));
        assert(reader.sample(f.target,head).failure==Failure::None);
    }
    {
        Fixture f; f.optional8(false);
        f.memory.put<std::int32_t>(Fixture::module+d::condition_dword_rva,0);
        // Nonnegative DWORD means the callee's byte is not a dependency.
        f.memory.erase_span(Fixture::module+d::condition_byte_rva,1);
        native_joints::Reader reader(f.memory); assert(reader.bind(Fixture::module));
        assert(reader.sample(f.target,head).failure==Failure::None);
    }
}
