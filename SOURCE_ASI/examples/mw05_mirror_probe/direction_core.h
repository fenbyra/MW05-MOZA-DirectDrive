#ifndef MW05_DIRECTION_CORE_H
#define MW05_DIRECTION_CORE_H

#include "drive_probe.h"
#include "moza_core.h"

// For the exact executable supplied by the user. The plugin only replaces
// arguments of a call the game was ALREADY making; it never invokes Shift,
// synthesizes a call to the helper, or writes a gear number.
namespace mw05_direction {
namespace p = mw05_probe;
namespace m = mw05_moza;
namespace d = mw05_drive_probe;
constexpr p::Address kHelperRva = 0x0029CEF0u;
constexpr p::Address kHelperReturnRva = 0x0029CFD5u;
constexpr p::Address kFetchCallRva = 0x002A8D89u;
constexpr p::Address kReverseSwapRva = 0x0029CFB6u;
constexpr p::Address kOwnerOffset = 0x48u; // IInput is InputPlayer+48.
constexpr p::Address kOwnerVtableRva = 0x004AC6BCu;
constexpr p::Address kDragOwnerVtableRva = 0x004AC754u;
constexpr p::Address kDragCtorRva = 0x002B8D06u;
constexpr p::Address kDragFetchRva = 0x002A8E70u;
constexpr unsigned char kDragCtor[] = {
    0xC7,0x06,0x54,0xC7,0x8A,0x00,0xC7,0x46,0x08,0x4C,0xC7,0x8A,0x00,
    0xC7,0x46,0x48,0x08,0xC7,0x8A,0x00,0xC7,0x46,0x74,0xF0,0xC6,0x8A,0x00
};
// Drag fetch delegates directly to the same base fetch, hence the same native helper.
constexpr unsigned char kDragFetch[] = {0xE9,0xFB,0xF9,0xFF,0xFF};
constexpr p::Address kSetGasRva = 0x0029CB90u;
constexpr p::Address kSetBrakeRva = 0x0029CBA0u;
constexpr unsigned char kHelperEntry[] = {
    0x51,0x57,0x8B,0xF9,0x8B,0x47,0x7C,0x85,0xC0,
    0x0F,0x84,0xD4,0x00,0x00,0x00
};
constexpr unsigned char kHelperReturn[] = {0xC2,0x08,0x00};
constexpr unsigned char kFetchCall[] = {0xE8,0x62,0x41,0xFF,0xFF};
constexpr unsigned char kReverseSwap[] = {
    0x8B,0x44,0x24,0x14,0x8B,0x57,0x48,0x8D,0x77,0x48,0x50,0x8B,
    0xCE,0xFF,0x52,0x18,0x8B,0x44,0x24,0x10,0x8B,0x16,0x50,0x8B,
    0xCE,0xFF,0x52,0x1C
};
constexpr unsigned char kSetGasCode[] = {
    0x8B,0x44,0x24,0x04,0x89,0x41,0x1C,0xC2,0x04,0x00
};
constexpr unsigned char kSetBrakeCode[] = {
    0x8B,0x44,0x24,0x04,0x89,0x41,0x20,0xC2,0x04,0x00
};

inline bool settersMatch(p::Address base, p::ReadMemory read, p::Address inputVtable=0) {
    if(inputVtable==0)inputVtable=base+p::kPlayerInputVtableRva;
    p::Address gas = 0, brake = 0;
    return base == p::kImageBase && read && p::supportedInputVtable(base,inputVtable) &&
        d::readValue(read, inputVtable + 6u*4, gas) &&
        d::readValue(read, inputVtable + 7u*4, brake) &&
        gas == base + kSetGasRva && brake == base + kSetBrakeRva &&
        d::matches(read, gas, kSetGasCode) &&
        d::matches(read, brake, kSetBrakeCode);
}
inline bool dragFingerprint(p::Address base, p::ReadMemory read) {
    p::Address getter=0,steer=0;
    return base==p::kImageBase && read &&
        d::matches(read,base+kDragCtorRva,kDragCtor) &&
        d::matches(read,base+kDragFetchRva,kDragFetch) &&
        d::readValue(read,base+p::kDragInputVtableRva+2u*4,getter) &&
        getter==base+p::kGetControlsRva &&
        d::readValue(read,base+p::kDragInputVtableRva+5u*4,steer) &&
        steer==0x0069CB80u && settersMatch(base,read,base+p::kDragInputVtableRva);
}
inline bool fingerprint(p::Address base, p::ReadMemory read) {
    return base == p::kImageBase && read &&
        d::matches(read, base + kHelperRva, kHelperEntry) &&
        d::matches(read, base + kHelperReturnRva, kHelperReturn) &&
        d::matches(read, base + kFetchCallRva, kFetchCall) &&
        d::matches(read, base + kReverseSwapRva, kReverseSwap) &&
        settersMatch(base, read);
}
inline bool ownerMatches(p::Address base, const p::Player& player,
                         p::Address owner, p::ReadMemory read) {
    p::Address vtable = 0,inputVtable=0;
    return base == p::kImageBase &&
        player.status == p::ResolveStatus::Unique &&
        p::pointerLike(owner) && owner <= 0xFFFFFE00u &&
        owner + kOwnerOffset == player.input &&
        d::readValue(read, owner, vtable) &&
        d::readValue(read, player.input, inputVtable) &&
        (player.inputVtable==0 || player.inputVtable==inputVtable) &&
        ((vtable==base+kOwnerVtableRva && inputVtable==base+p::kPlayerInputVtableRva) ||
         (vtable==base+kDragOwnerVtableRva && inputVtable==base+p::kDragInputVtableRva));
}
struct EnginePedals {
    bool valid = false, reverse = false;
    float gas = 0, brake = 0;
};
inline EnginePedals map(const m::Axes& logical, int actualGear) {
    if (actualGear < 0 || actualGear > 9 ||
        !std::isfinite(logical.gas) || !std::isfinite(logical.brake) ||
        logical.gas < 0 || logical.gas > 1 ||
        logical.brake < 0 || logical.brake > 1) return {};
    // In R the GAME's logical brake action supplies engine gas, while the
    // logical gas action supplies braking until the native helper selects D.
    // Never infer R from speed sign, a request, or a remembered gear.
    const bool reverse = actualGear == 0;
    return {true, reverse, reverse ? logical.brake : logical.gas,
                           reverse ? logical.gas : logical.brake};
}
} // namespace mw05_direction
#endif