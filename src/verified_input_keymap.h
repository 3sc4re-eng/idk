#pragma once
#include <array>
#include <cstdint>
// Derived only from the exact inputsystem.dll static initializer. See verification/input-keymap.json.
// SHA256: ce8a3d9ae3f312ad081248dfe11a371008694e4a355c226b9f3944e4a9530fb3
// Raw table: mouse and side-specific modifier VK entries intentionally remain zero.
namespace verified_input_keymap{
inline constexpr std::uint32_t timestamp=0x6AD7C220, image_size=0x53000;
inline constexpr std::array<int,256> virtual_keys={{
    0, 0, 0, 0, 0, 0, 0, 0, 67, 68, 0, 0, 0, 65, 0, 0,
    80, 84, 82, 79, 69, 0, 0, 0, 0, 0, 0, 71, 0, 0, 0, 0,
    66, 77, 78, 76, 75, 90, 89, 92, 91, 0, 0, 0, 0, 73, 74, 0,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0, 0, 53, 0, 0, 0,
    0, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
    26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 86, 87, 88, 0, 0,
    37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 48, 50, 0, 49, 52, 47,
    93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    70, 72, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 56, 64, 59, 63, 60, 61,
    58, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 54, 62, 55, 57, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
}};
}

static_assert([]{for(int code:verified_input_keymap::virtual_keys)if(code<0||code>=512)return false;return verified_input_keymap::virtual_keys[9]==68&&verified_input_keymap::virtual_keys[65]==11&&verified_input_keymap::virtual_keys[90]==36;}());
