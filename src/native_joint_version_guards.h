#pragma once
#include <array>
#include <cstdint>
#include <string_view>
namespace native_joints::version {
// Generated from the two reviewed static evidence JSON files; RVAs only.
inline constexpr std::uint32_t timestamp = 1793470523U;
inline constexpr std::uint32_t image_size = 71352320U;
inline constexpr std::uint32_t schema_revision = 11193905U;
inline constexpr std::string_view static_file_sha256 = "929a0455e510ad8bb76c26737a9c84a58ed496267b89b243f94b0699bb10855a";
struct Guard { std::uint32_t rva; std::string_view hex; };
inline constexpr std::array<Guard, 12> guards = {{
    {0x1895cb9U, "488b8bc00100004885c9751b90388b100100007408488bcbe84a4ef2ff0f2843100f284b20eb10488bc648c1e0050f2804080f284c0810488b5c2430488bc7488b7424380f29070f294f104883c4205f"}, // bone_world_transform_copy
    {0x1895bf0U, "40534883ec20488bd9e842edfeff486383d401000048c1e005480383c00100004883c4205bc3"}, // bone_pose_end_pointer_count
    {0x22da440U, "8b8178010000c3"}, // model_bone_count
    {0x22dadd0U, "85d278253b91780100007d1d488b81680100004863d2488b0cd0488d0539a637004885c9480f45c1c3488d0590a0a800c3"}, // model_bone_name
    {0x22dae10U, "85d278173b91780100007d0f488b81800100004863d20fbf0450c3b8ffffffffc3"}, // model_bone_parent
    {0x22dadb0U, "85d278163b91780100007d0e488b81b00100004863d28b0490c333c0c3"}, // model_bone_flags
    {0x1884c72U, "908b86680400004123c7413bc70f84480b0000"}, // cache_mask_fast_path
    {0x18ab000U, "40534883ec20488bd9c7816804000000000000488b89c80100004885c9741e8b83d401000083c01fc1f80585c07e0e4c63c033d249c1e002e82381d100488bcb"}, // cache_mask_invalidation
    {0x1895790U, "48895c2420555657415441554881ecd00000004963304d8be0488bea488bd985f60f84820000004c8ba9e001000033ff4d85ed74064d8b6d00eb034c8bef"}, // named_hitbox_transforms_entry
    {0x1884bfeU, "4181e7ffff0f004883be60020000004489bd5842000074558b861002000023c3a9ffff0f007446488b05a43d60028078580075144183cf08c68568420000014489bd58420000eb2c833d03d5aa01ff751ce8fc2a3bff340188856842000074144183cf084489bd58420000eb07c6856842000000908b86680400004123c7413bc70f84480b0000"}, // effective_request_and_fast_path
    {0xc37750U, "0fb6050524df02c3"}, // effective_request_callee
    {0x1895c9bU, "8bd6e80e51a400ba00010000488bcb85c2b8ffff0f000f44d0e8e7eefeff"}, // bone_getter_request
}};
} // namespace native_joints::version
