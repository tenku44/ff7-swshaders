
#pragma once

#include <cstdint>

namespace rh::input {

inline constexpr uint64_t kButtonA = 1ull << 0;
inline constexpr uint64_t kButtonB = 1ull << 1;
inline constexpr uint64_t kButtonX = 1ull << 2;
inline constexpr uint64_t kButtonY = 1ull << 3;
inline constexpr uint64_t kButtonStickL = 1ull << 4;
inline constexpr uint64_t kButtonStickR = 1ull << 5;
inline constexpr uint64_t kButtonL = 1ull << 6;
inline constexpr uint64_t kButtonR = 1ull << 7;
inline constexpr uint64_t kButtonZL = 1ull << 8;
inline constexpr uint64_t kButtonZR = 1ull << 9;
inline constexpr uint64_t kButtonPlus = 1ull << 10;
inline constexpr uint64_t kButtonMinus = 1ull << 11;
inline constexpr uint64_t kButtonLeft = 1ull << 12;
inline constexpr uint64_t kButtonUp = 1ull << 13;
inline constexpr uint64_t kButtonRight = 1ull << 14;
inline constexpr uint64_t kButtonDown = 1ull << 15;

inline constexpr uint64_t kToggleCombo = kButtonL | kButtonZL; //menu toggle L+Zl
inline constexpr const char* kToggleComboName = "L + ZL";

bool Available();
bool TouchAvailable();

inline constexpr unsigned kFoundHandheld = 1;
inline constexpr unsigned kFoundFullKey = 2;
inline constexpr unsigned kFoundJoyDual = 4;
unsigned FoundFunctions();
void EnableImGuiNavigation();

bool Poll(bool feedImGui, int width, int height);

}  // namespace rh::input
