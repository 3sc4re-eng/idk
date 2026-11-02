#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "native_joint_version_guards.h"

namespace native_joints {
using Address = std::uint64_t;
using HostClock = std::chrono::steady_clock;

struct ReadRequest {
    Address address = 0;
    void* destination = nullptr;
    std::uint32_t size = 0;
    std::uint32_t transferred = 0;
    bool valid = false;
};

// An adapter must issue a new uncached acquisition for each call. A
// FrameCache/page-cache replay does not satisfy this interface's contract.
class FreshSource {
public:
    virtual ~FreshSource() = default;
    virtual bool supports_independent_uncached_reads() const noexcept = 0;
    // Host attach generation: nonzero, stable while attached; changes on every
    // target/session/module binding replacement. This is NOT native pose age.
    virtual std::uint64_t binding_epoch() const noexcept = 0;
    virtual bool read_fresh(std::span<ReadRequest> requests) = 0;
};

enum class Joint : std::uint8_t { Head = 0, Neck = 1 };
using Demand = std::uint8_t;
inline constexpr Demand head = 1, neck = 2, supported_demand = head | neck;

enum class Failure {
    None, NoDemand, UnsupportedDemand, UnsupportedReadContract, NotBound,
    WrongVersion, BindingChanged, InvalidAddress, ReadFailed, BudgetExceeded,
    IdentityMismatch, InvalidScene, InvalidModel, InvalidCount, InvalidName,
    MissingName, AmbiguousName, NotHitbox, Locked, CacheNotReady, MetadataChanged, Inconsistent,
    NonFiniteTransform
};

struct Vec3 { float x = 0, y = 0, z = 0; };
struct JointSample {
    bool valid = false;
    std::int32_t model_index = -1;
    Vec3 world{};
};
struct Sample {
    Failure failure = Failure::NotBound;
    Demand requested = 0;
    std::array<JointSample, 2> joints{};
    // Only populated after a completely accepted acquisition. No native frame
    // timestamp, pose age or render-frame freshness is represented here.
    HostClock::time_point host_acquisition_started_at{}, host_acquired_at{};
    std::uint64_t bytes_requested = 0;
    std::uint64_t read_requests = 0;
    bool valid(Joint joint) const noexcept {
        const auto index = static_cast<std::size_t>(joint);
        return index < joints.size() && joints[index].valid;
    }
};

// Existing backend identity contract: entity+m_pEntity -> record, whose serial
// handle is +0x10 and rejection flags are +0x30. No new serial offset is guessed.
struct Target {
    Address entity = 0;
    Address expected_identity = 0;
    std::uint32_t expected_handle = 0;
};

struct Limits {
    // Operational ceilings, not claims about native model maxima.
    std::uint32_t max_bones = 1024;
    std::uint64_t max_bytes_per_acquisition = 512 * 1024;
    std::uint64_t max_requests_per_acquisition = 8192;
    std::uint32_t max_requests_per_burst = 256;
};

namespace detail {
constexpr bool invalidates_metadata(Failure failure) noexcept {
    switch (failure) {
    case Failure::NoDemand:
    case Failure::UnsupportedDemand:
    case Failure::Locked:
    case Failure::CacheNotReady:
    case Failure::Inconsistent:
    case Failure::NonFiniteTransform:
    case Failure::ReadFailed:
    case Failure::BudgetExceeded:
        return false;
    default:
        return true;
    }
}
inline constexpr Address minimum_user_address = 0x10000;
inline constexpr Address maximum_user_address = 0x00007fffffffffffULL;
inline constexpr std::uint32_t name_capacity = 96;
inline constexpr std::uint32_t entity_identity_offset = 0x10;
inline constexpr std::uint32_t entity_scene_offset = 0x330;
inline constexpr std::uint32_t identity_handle_offset = 0x10;
inline constexpr std::uint32_t identity_flags_offset = 0x30;
inline constexpr std::uint32_t rejected_identity_flags = 0x717;
inline constexpr std::uint32_t scene_owner_offset = 0x30;
inline constexpr std::uint32_t scene_lock_offset = 0x114;
inline constexpr std::uint32_t pose_pointer_offset = 0x1c0;
inline constexpr std::uint32_t bitmap_pointer_offset = 0x1c8;
inline constexpr std::uint32_t pose_count_offset = 0x1d4;
inline constexpr std::uint32_t model_record_offset = 0x1e0;
inline constexpr std::uint32_t condition_mask_offset = 0x210;
inline constexpr std::uint32_t condition_pointer_offset = 0x260;
inline constexpr std::uint32_t computed_mask_offset = 0x468;
inline constexpr std::uint32_t names_pointer_offset = 0x168;
inline constexpr std::uint32_t model_count_offset = 0x178;
inline constexpr std::uint32_t flags_pointer_offset = 0x1b0;
inline constexpr std::uint32_t condition_object_pointer_rva = 0x3e889d0;
inline constexpr std::uint32_t condition_dword_rva = 0x3332150;
inline constexpr std::uint32_t condition_byte_rva = 0x3a29b5c;
inline constexpr std::uint32_t scene_vtable_rva = 0x2c15650;

constexpr bool valid_span(Address address, std::uint64_t size) noexcept {
    return address >= minimum_user_address && size != 0 &&
        size <= maximum_user_address && address <= maximum_user_address - (size - 1);
}
constexpr bool finite(float value) noexcept {
    return (std::bit_cast<std::uint32_t>(value) & 0x7f800000U) != 0x7f800000U;
}
constexpr std::uint32_t base_request(std::uint32_t flags) noexcept {
    return (flags & 0x100U) != 0 ? 0x100U : 0xfffffU;
}
constexpr std::uint32_t effective_request(std::uint32_t request,
    Address scene_condition_pointer, std::uint32_t scene_condition_mask,
    std::uint8_t object_byte, std::int32_t condition_dword,
    std::uint8_t condition_byte) noexcept {
    const auto r = request & 0xfffffU;
    const bool add8 = scene_condition_pointer != 0 && (scene_condition_mask & r) != 0 &&
        (object_byte == 0 || (condition_dword == -1 && condition_byte != 1));
    return r | (add8 ? 8U : 0U);
}
constexpr bool mask_ready(std::uint32_t mask, std::uint32_t request) noexcept {
    return (mask & request) == request;
}
constexpr bool valid_count(std::int32_t count, std::uint32_t maximum) noexcept {
    return count > 0 && static_cast<std::uint32_t>(count) <= maximum && count <= 65535;
}
struct NativePose {
    float x, y, z, scale, qx, qy, qz, qw;
};
static_assert(sizeof(NativePose) == 32);
inline bool finite_pose(const NativePose& p) noexcept {
    return finite(p.x) && finite(p.y) && finite(p.z) && finite(p.scale) &&
        finite(p.qx) && finite(p.qy) && finite(p.qz) && finite(p.qw);
}
struct Lock {
    std::uint32_t state = 0;
    std::uint16_t depth = 0;
    std::uint16_t padding = 0;
    std::uint32_t owner = 0;
    bool quiet() const noexcept { return state == 0 && depth == 0 && owner == 0; }
};
static_assert(sizeof(Lock) == 12);
struct Snapshot {
    bool actor_context_captured = false;
    bool metadata_captured = false; // Observer bookkeeping, never a native field.
    Address identity = 0, scene = 0, vtable = 0, owner = 0;
    std::uint32_t handle = 0, identity_flags = 0;
    Lock lock{};
    Address model_record = 0, model = 0, poses = 0, bitmap = 0;
    std::int32_t pose_count = 0, model_count = 0;
    Address names = 0, flags = 0, condition_pointer = 0;
    std::uint32_t computed_mask = 0, condition_mask = 0;
    Address condition_object = 0;
    std::uint8_t condition_object_byte = 0, condition_byte = 0;
    std::int32_t condition_dword = 0;
    std::uint32_t request() const noexcept {
        return effective_request(0x100, condition_pointer, condition_mask,
            condition_object_byte, condition_dword, condition_byte);
    }
};
inline bool same_snapshot(const Snapshot& a, const Snapshot& b) noexcept {
    return a.identity == b.identity && a.scene == b.scene && a.vtable == b.vtable &&
        a.owner == b.owner && a.handle == b.handle && a.identity_flags == b.identity_flags &&
        a.lock.state == b.lock.state && a.lock.depth == b.lock.depth && a.lock.owner == b.lock.owner &&
        a.model_record == b.model_record && a.model == b.model && a.poses == b.poses &&
        a.bitmap == b.bitmap && a.pose_count == b.pose_count && a.model_count == b.model_count &&
        a.names == b.names && a.flags == b.flags && a.condition_pointer == b.condition_pointer &&
        a.computed_mask == b.computed_mask && a.condition_mask == b.condition_mask &&
        a.condition_object == b.condition_object &&
        a.condition_object_byte == b.condition_object_byte && a.condition_byte == b.condition_byte &&
        a.condition_dword == b.condition_dword;
}
struct Descriptor {
    std::int32_t index = -1;
    Address name = 0;
    std::uint32_t flags = 0;
    std::string exact_name;
    std::uint32_t matches = 0;
};
struct MetadataKey {
    Address identity = 0, scene = 0, record = 0, model = 0, names = 0, flags = 0;
    std::uint32_t handle = 0;
    std::int32_t count = 0;
    bool operator==(const MetadataKey&) const = default;
};
inline MetadataKey key(const Snapshot& s) noexcept {
    return {s.identity, s.scene, s.model_record, s.model, s.names, s.flags, s.handle, s.model_count};
}
struct SelectedRead {
    Address name = 0;
    std::uint32_t flags = 0, bitmap_word = 0;
    NativePose pose{};
    std::array<char, name_capacity> name_bytes{};
    std::string name_string;
};
template<class T> ReadRequest request(Address at, T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(sizeof(T) <= std::numeric_limits<std::uint32_t>::max());
    return {at, &value, static_cast<std::uint32_t>(sizeof(T)), 0, false};
}
inline ReadRequest bytes(Address at, void* destination, std::size_t size) {
    return {at, destination, static_cast<std::uint32_t>(size), 0, false};
}
inline bool parse_name(const std::array<char, name_capacity>& bytes, std::string& out) {
    const auto end = std::find(bytes.begin(), bytes.end(), '\0');
    if (end == bytes.end() || end == bytes.begin()) return false;
    out.assign(bytes.begin(), end);
    return true;
}
} // namespace detail

// One instance owns one actor's small semantic metadata cache. Pose samples are
// never cached here. The class is deliberately not thread-safe; use on the
// backend reader thread and publish accepted Sample values through its snapshot.
class Reader {
public:
    explicit Reader(FreshSource& source, Limits limits = {}) : source_(source), limits_(limits) {}

    void reset() noexcept {
        bound_ = false; module_ = 0; epoch_ = 0; metadata_valid_ = false;
        descriptors_ = {}; metadata_key_ = {};
    }
    bool bind(Address client_module) {
        reset(); failure_ = Failure::None; bytes_ = requests_ = 0;
        if (!source_.supports_independent_uncached_reads()) return reject(Failure::UnsupportedReadContract);
        epoch_ = source_.binding_epoch();
        if (epoch_ == 0) return reject(Failure::BindingChanged);
        if (!detail::valid_span(client_module, version::image_size)) return reject(Failure::InvalidAddress);
        module_ = client_module;
        if (!verify_module()) { module_ = 0; epoch_ = 0; return false; }
        bound_ = true;
        return true;
    }
    Failure last_failure() const noexcept { return failure_; }
    bool metadata_cached() const noexcept { return metadata_valid_; } // Cached mapping, not current sample approval.

    Sample sample(const Target& target, Demand demand) {
        Sample out; out.requested = demand; bytes_ = requests_ = 0; failure_ = Failure::None;
        auto fail = [&](Failure failure) {
            failure_ = failure;
            if (detail::invalidates_metadata(failure)) metadata_valid_ = false;
            out.failure = failure; out.bytes_requested = bytes_; out.read_requests = requests_;
            return out;
        };
        if (demand == 0) return fail(Failure::NoDemand);
        if ((demand & ~supported_demand) != 0) return fail(Failure::UnsupportedDemand);
        if (!source_.supports_independent_uncached_reads()) return fail(Failure::UnsupportedReadContract);
        if (!bound_) return fail(Failure::NotBound);
        if (!same_epoch()) { reset(); return fail(Failure::BindingChanged); }
        if (!detail::valid_span(target.entity, detail::entity_scene_offset + 8) ||
            !detail::valid_span(target.expected_identity, detail::identity_flags_offset + 4))
            return fail(Failure::InvalidAddress);
        const auto started_at = HostClock::now();
        detail::Snapshot before, after;
        const bool before_ok = capture(target, before);
        invalidate_changed_metadata_key(before);
        if (!before_ok) return fail(failure_);
        if (!metadata_valid_ || !(metadata_key_ == detail::key(before))) {
            metadata_valid_ = false;
            if (!build_metadata(before)) return fail(failure_);
        }
        for (std::size_t j = 0; j != 2; ++j) if ((demand & (1U << j)) != 0) {
            if (descriptors_[j].matches == 0) return fail(Failure::MissingName);
            if (descriptors_[j].matches != 1) return fail(Failure::AmbiguousName);
            if ((descriptors_[j].flags & 0x100U) == 0) return fail(Failure::NotHitbox);
            if (descriptors_[j].index < 0 || descriptors_[j].index >= before.pose_count)
                return fail(Failure::InvalidCount);
        }
        std::array<detail::SelectedRead, 2> first{}, second{};
        if (!read_selected(before, demand, first) || !read_selected(before, demand, second))
            return fail(failure_);
        const bool after_ok = capture(target, after);
        invalidate_changed_metadata_key(after);
        if (!after_ok) return fail(failure_);
        if (!detail::same_snapshot(before, after)) {
            if (!(detail::key(before) == detail::key(after))) return fail(Failure::MetadataChanged);
            return fail(Failure::Inconsistent);
        }
        for (std::size_t j = 0; j != 2; ++j) if ((demand & (1U << j)) != 0) {
            const auto& a = first[j]; const auto& b = second[j];
            if (a.name != b.name || a.flags != b.flags || a.name_string != b.name_string)
                return fail(Failure::MetadataChanged);
            if (a.bitmap_word != b.bitmap_word || std::memcmp(&a.pose, &b.pose, sizeof(a.pose)) != 0)
                return fail(Failure::Inconsistent);
        }
        if (!same_epoch()) { reset(); return fail(Failure::BindingChanged); }
        for (std::size_t j = 0; j != 2; ++j) if ((demand & (1U << j)) != 0) {
            const auto& p = second[j].pose;
            out.joints[j] = {true, descriptors_[j].index, {p.x, p.y, p.z}};
        }
        out.failure = Failure::None; out.host_acquisition_started_at = started_at;
        out.host_acquired_at = HostClock::now();
        out.bytes_requested = bytes_; out.read_requests = requests_;
        return out;
    }

private:
    FreshSource& source_;
    Limits limits_;
    Address module_ = 0;
    std::uint64_t epoch_ = 0, bytes_ = 0, requests_ = 0;
    bool bound_ = false, metadata_valid_ = false;
    Failure failure_ = Failure::NotBound;
    detail::MetadataKey metadata_key_{};
    std::array<detail::Descriptor, 2> descriptors_{};

    bool reject(Failure failure) noexcept { failure_ = failure; return false; }
    void invalidate_changed_metadata_key(const detail::Snapshot& snapshot) noexcept {
        if (metadata_valid_ && snapshot.actor_context_captured &&
            (metadata_key_.identity != snapshot.identity || metadata_key_.handle != snapshot.handle ||
             metadata_key_.scene != snapshot.scene)) metadata_valid_ = false;
        if (metadata_valid_ && snapshot.metadata_captured && !(metadata_key_ == detail::key(snapshot)))
            metadata_valid_ = false;
    }
    bool same_epoch() const noexcept { return epoch_ != 0 && source_.binding_epoch() == epoch_; }
    bool batch(std::span<ReadRequest> reads) {
        if (reads.empty()) return true;
        if (!same_epoch()) return reject(Failure::BindingChanged);
        if (limits_.max_requests_per_burst == 0 || limits_.max_requests_per_burst > 512)
            return reject(Failure::BudgetExceeded);
        std::uint64_t added = 0;
        for (auto& r : reads) {
            r.valid = false; r.transferred = 0;
            if (!r.destination || !detail::valid_span(r.address, r.size)) return reject(Failure::InvalidAddress);
            added += r.size;
        }
        if (bytes_ > limits_.max_bytes_per_acquisition ||
            added > limits_.max_bytes_per_acquisition - bytes_ ||
            requests_ > limits_.max_requests_per_acquisition ||
            reads.size() > limits_.max_requests_per_acquisition - requests_)
            return reject(Failure::BudgetExceeded);
        for (std::size_t begin = 0; begin < reads.size(); begin += limits_.max_requests_per_burst) {
            const auto count = std::min<std::size_t>(limits_.max_requests_per_burst, reads.size() - begin);
            auto part = reads.subspan(begin, count);
            if (!same_epoch()) return reject(Failure::BindingChanged);
            for (const auto& r : part) { bytes_ += r.size; ++requests_; }
            if (!source_.read_fresh(part)) return reject(Failure::ReadFailed);
            if (!same_epoch()) return reject(Failure::BindingChanged);
            for (const auto& r : part) if (!r.valid || r.transferred != r.size) return reject(Failure::ReadFailed);
        }
        return true;
    }
    bool batch(std::initializer_list<ReadRequest> reads) {
        std::vector<ReadRequest> owned(reads);
        return batch(std::span<ReadRequest>(owned));
    }
    template<class T> bool read(Address at, T& value) {
        auto r = detail::request(at, value); return batch(std::span<ReadRequest>(&r, 1));
    }
    bool verify_module() {
        std::uint16_t mz = 0; std::uint32_t pe_offset = 0;
        if (!batch({detail::request(module_, mz), detail::request(module_ + 0x3c, pe_offset)})) return false;
        if (mz != 0x5a4d || pe_offset < 0x40 || pe_offset > 0x100000) return reject(Failure::WrongVersion);
        std::uint32_t pe = 0, timestamp = 0, image_size = 0;
        std::uint16_t machine = 0, magic = 0;
        if (!batch({detail::request(module_ + pe_offset, pe),
            detail::request(module_ + pe_offset + 4, machine),
            detail::request(module_ + pe_offset + 8, timestamp),
            detail::request(module_ + pe_offset + 24, magic),
            detail::request(module_ + pe_offset + 24 + 56, image_size)})) return false;
        if (pe != 0x4550 || machine != 0x8664 || magic != 0x20b ||
            timestamp != version::timestamp || image_size != version::image_size)
            return reject(Failure::WrongVersion);
        for (const auto& guard : version::guards) {
            if ((guard.hex.size() & 1U) != 0 || guard.hex.empty()) return reject(Failure::WrongVersion);
            const auto count = guard.hex.size() / 2;
            if (guard.rva >= image_size || count > image_size - guard.rva) return reject(Failure::WrongVersion);
            std::vector<std::uint8_t> observed(count), expected(count);
            auto digit = [](char c) -> unsigned { return c <= '9' ? unsigned(c - '0') : unsigned(c - 'a' + 10); };
            for (std::size_t i = 0; i != count; ++i)
                expected[i] = static_cast<std::uint8_t>((digit(guard.hex[2*i]) << 4) | digit(guard.hex[2*i+1]));
            auto r = detail::bytes(module_ + guard.rva, observed.data(), count);
            if (!batch(std::span<ReadRequest>(&r, 1))) return false;
            if (observed != expected) return reject(Failure::WrongVersion);
        }
        return true;
    }
    bool capture(const Target& target, detail::Snapshot& s) {
        if (!batch({detail::request(target.entity + detail::entity_identity_offset, s.identity),
            detail::request(target.entity + detail::entity_scene_offset, s.scene)})) return false;
        if (s.identity != target.expected_identity) return reject(Failure::IdentityMismatch);
        if (!detail::valid_span(s.scene, detail::computed_mask_offset + 4)) return reject(Failure::InvalidScene);
        if (!batch({detail::request(s.identity + detail::identity_handle_offset, s.handle),
            detail::request(s.identity + detail::identity_flags_offset, s.identity_flags),
            detail::request(s.scene, s.vtable), detail::request(s.scene + detail::scene_owner_offset, s.owner),
            detail::request(s.scene + detail::scene_lock_offset, s.lock),
            detail::request(s.scene + detail::model_record_offset, s.model_record),
            detail::request(s.scene + detail::pose_pointer_offset, s.poses),
            detail::request(s.scene + detail::bitmap_pointer_offset, s.bitmap),
            detail::request(s.scene + detail::pose_count_offset, s.pose_count),
            detail::request(s.scene + detail::computed_mask_offset, s.computed_mask),
            detail::request(s.scene + detail::condition_pointer_offset, s.condition_pointer),
            detail::request(s.scene + detail::condition_mask_offset, s.condition_mask)})) return false;
        if (s.handle != target.expected_handle || (s.identity_flags & detail::rejected_identity_flags) != 0)
            return reject(Failure::IdentityMismatch);
        if (s.vtable != module_ + detail::scene_vtable_rva || s.owner != target.entity)
            return reject(Failure::InvalidScene);
        s.actor_context_captured = true;
        if (!s.lock.quiet()) return reject(Failure::Locked);
        if (!detail::valid_count(s.pose_count, limits_.max_bones)) return reject(Failure::InvalidCount);
        if (!detail::valid_span(s.model_record, 8) ||
            !detail::valid_span(s.poses, std::uint64_t(s.pose_count) * 32) ||
            !detail::valid_span(s.bitmap, ((std::uint64_t(s.pose_count) + 31) / 32) * 4))
            return reject(Failure::InvalidModel);
        // The verified native getter uses MOVAPS for cache elements.
        if ((s.poses & 15U) != 0) return reject(Failure::InvalidModel);
        if (!read(s.model_record, s.model)) return false;
        if (!detail::valid_span(s.model, detail::flags_pointer_offset + 8)) return reject(Failure::InvalidModel);
        if (!batch({detail::request(s.model + detail::model_count_offset, s.model_count),
            detail::request(s.model + detail::names_pointer_offset, s.names),
            detail::request(s.model + detail::flags_pointer_offset, s.flags)})) return false;
        if (!detail::valid_count(s.model_count, limits_.max_bones)) return reject(Failure::InvalidCount);
        if (!detail::valid_span(s.names, std::uint64_t(s.model_count) * 8) ||
            !detail::valid_span(s.flags, std::uint64_t(s.model_count) * 4)) return reject(Failure::InvalidModel);
        s.metadata_captured = true;
        if (s.condition_pointer != 0 && (s.condition_mask & 0x100U) != 0) {
            if (!read(module_ + detail::condition_object_pointer_rva, s.condition_object)) return false;
            if (!detail::valid_span(s.condition_object, 0x59)) return reject(Failure::InvalidModel);
            if (!read(s.condition_object + 0x58, s.condition_object_byte)) return false;
            if (s.condition_object_byte != 0) {
                if (!read(module_ + detail::condition_dword_rva, s.condition_dword)) return false;
                if (s.condition_dword == -1 &&
                    !read(module_ + detail::condition_byte_rva, s.condition_byte)) return false;
            }
        }
        if (!detail::mask_ready(s.computed_mask, s.request())) return reject(Failure::CacheNotReady);
        return true;
    }
    bool build_metadata(const detail::Snapshot& s) {
        descriptors_ = {};
        std::vector<Address> pointers(static_cast<std::size_t>(s.model_count));
        std::vector<std::uint32_t> flags(static_cast<std::size_t>(s.model_count));
        std::vector<std::array<char, detail::name_capacity>> names(static_cast<std::size_t>(s.model_count));
        if (!batch({detail::bytes(s.names, pointers.data(), pointers.size() * sizeof(Address)),
            detail::bytes(s.flags, flags.data(), flags.size() * sizeof(std::uint32_t))})) return false;
        std::vector<ReadRequest> reads; reads.reserve(pointers.size());
        for (std::size_t i = 0; i != pointers.size(); ++i)
            reads.push_back(detail::bytes(pointers[i], names[i].data(), names[i].size()));
        if (!batch(std::span<ReadRequest>(reads))) return false;
        constexpr std::array<std::string_view, 2> exact = {"head", "neck_0"};
        for (std::size_t i = 0; i != names.size(); ++i) {
            std::string name;
            if (!detail::parse_name(names[i], name)) return reject(Failure::InvalidName);
            for (std::size_t j = 0; j != exact.size(); ++j) if (name == exact[j]) {
                auto& d = descriptors_[j]; ++d.matches;
                d.index = static_cast<std::int32_t>(i); d.name = pointers[i]; d.flags = flags[i]; d.exact_name = name;
            }
        }
        metadata_key_ = detail::key(s); metadata_valid_ = true;
        return true;
    }
    bool read_selected(const detail::Snapshot& s, Demand demand,
        std::array<detail::SelectedRead, 2>& data) {
        std::vector<ReadRequest> reads;
        for (std::size_t j = 0; j != 2; ++j) if ((demand & (1U << j)) != 0) {
            const auto& d = descriptors_[j]; const auto i = static_cast<std::uint32_t>(d.index);
            if (d.index < 0 || d.index >= s.model_count || d.index >= s.pose_count)
                return reject(Failure::InvalidCount);
            auto& r = data[j];
            reads.push_back(detail::request(s.names + std::uint64_t(i) * 8, r.name));
            reads.push_back(detail::request(s.flags + std::uint64_t(i) * 4, r.flags));
            reads.push_back(detail::request(s.bitmap + std::uint64_t(i >> 5) * 4, r.bitmap_word));
            reads.push_back(detail::request(s.poses + std::uint64_t(i) * 32, r.pose));
        }
        if (!batch(std::span<ReadRequest>(reads))) return false;
        reads.clear();
        for (std::size_t j = 0; j != 2; ++j) if ((demand & (1U << j)) != 0) {
            auto& r = data[j]; const auto& d = descriptors_[j];
            if (r.name != d.name || r.flags != d.flags) return reject(Failure::MetadataChanged);
            if ((r.flags & 0x100U) == 0) return reject(Failure::NotHitbox);
            if ((r.bitmap_word & (1U << (static_cast<std::uint32_t>(d.index) & 31U))) == 0)
                return reject(Failure::CacheNotReady);
            if (!detail::finite_pose(r.pose)) return reject(Failure::NonFiniteTransform);
            reads.push_back(detail::bytes(r.name, r.name_bytes.data(), r.name_bytes.size()));
        }
        if (!batch(std::span<ReadRequest>(reads))) return false;
        for (std::size_t j = 0; j != 2; ++j) if ((demand & (1U << j)) != 0) {
            auto& r = data[j];
            if (!detail::parse_name(r.name_bytes, r.name_string)) return reject(Failure::InvalidName);
            if (r.name_string != descriptors_[j].exact_name) return reject(Failure::MetadataChanged);
        }
        return true;
    }
};
} // namespace native_joints
