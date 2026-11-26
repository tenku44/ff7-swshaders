// nn_hid_min.hpp: the few Nintendo SDK controller declarations the overlay needs.
//
// The SDK's input functions live in the game's SDK module. The functions are declared WEAK: if the
// game's module does not export one of them, its address is null at run time and the overlay simply
// has no controller input (see input.cpp). Games read pads through either the plural
// GetNpadStates (several states at once) or the singular GetNpadState, so both are declared and
// whichever the game's SDK module exports is used. To check, search the SDK module's symbols for
// 'Npad'. The mangled names are
//   _ZN2nn3hid13GetNpadStatesEPNS0_16NpadFullKeyStateEiRKj   (and ..._17NpadHandheldState...)
//   _ZN2nn3hid12GetNpadStateEPNS0_16NpadFullKeyStateERKj     (and ..._17NpadHandheldState...)
//
// The structs follow the SDK's layout (0x28 bytes, the same as libnx's HidNpadFullKeyState).
// Define them only in the file that reads the pads; do not include this header together with
// another definition of the same types.

#pragma once

#include <cstdint>

namespace nn {
namespace hid {

using NpadIdType = uint32_t;

struct AnalogStickState {
    int32_t x;
    int32_t y;
};

struct NpadFullKeyState {
    int64_t samplingNumber;
    uint64_t buttons;
    AnalogStickState analogStickL;
    AnalogStickState analogStickR;
    uint32_t attributes;
    uint32_t reserved;
};
static_assert(sizeof(NpadFullKeyState) == 0x28, "unexpected NpadFullKeyState layout");

struct NpadHandheldState {
    int64_t samplingNumber;
    uint64_t buttons;
    AnalogStickState analogStickL;
    AnalogStickState analogStickR;
    uint32_t attributes;
    uint32_t reserved;
};
static_assert(sizeof(NpadHandheldState) == 0x28, "unexpected NpadHandheldState layout");

// Writes up to 'count' states, newest first, and returns how many were written.
int GetNpadStates(NpadFullKeyState* outStates, int count, const NpadIdType& id) __attribute__((weak));
int GetNpadStates(NpadHandheldState* outStates, int count, const NpadIdType& id) __attribute__((weak));

// Writes the newest state.
void GetNpadState(NpadFullKeyState* outState, const NpadIdType& id) __attribute__((weak));
void GetNpadState(NpadHandheldState* outState, const NpadIdType& id) __attribute__((weak));

}  // namespace hid
}  // namespace nn