#ifndef MW05_DRIVE_PROBE_H
#define MW05_DRIVE_PROBE_H

#include "probe_core.h"
#include <cmath>
#include <cstdint>
#include <cstring>

// Read-only diagnostics for the user's SHA256 325304A2... executable.
// No game function calls, gear requests, or game-memory writes are made HERE.
// v0.6.2 used it for logging only. v0.6.3 also requires a valid actual gear for
// pedal mapping; invalid snapshots must cancel, never guess a gear.
namespace mw05_drive_probe {
namespace p = mw05_probe;
constexpr p::Address kVehicleInterfaceOffset = 0xACu;
constexpr p::Address kInputVehicleOffset = 0x34u; // InputPlayer +7C, IInput at +48.
constexpr p::Address kTransmissionOffset = 0xFCu;
constexpr p::Address kVehicleInterfaceVtableRva = 0x004AA828u;
constexpr p::Address kTransmissionVtableRva = 0x004AB720u;
constexpr p::Address kGetGearRva = 0x002A0590u;
constexpr p::Address kGetSpeedRva = 0x002881B0u;
constexpr p::Address kGearOffset = 0x38u;  // mov eax,[ecx+38]; ret.
constexpr p::Address kSpeedOffset = 0x70u; // fld dword ptr [ecx+70]; ret.
constexpr unsigned kGetGearSlot = 1;
constexpr unsigned kGetSpeedSlot = 36;
constexpr unsigned char kGetGearCode[] = {0x8B, 0x41, 0x38, 0xC3};
constexpr unsigned char kGetSpeedCode[] = {0xD9, 0x41, 0x70, 0xC3};

enum class Status : unsigned {
    NoPlayer = 0,
    Ok = 1,
    IdentityMismatch = 2,
    VehicleInterfaceMismatch = 3,
    TransmissionMismatch = 4,
    FingerprintMismatch = 5,
    ValueRejected = 6,
    ChangedDuringRead = 7,
    ReadFailed = 8
};
struct Snapshot {
    Status status = Status::NoPlayer;
    p::Address transmission = 0;
    p::Address vehicleInterface = 0;
    int gear = -1; // 0=R, 1=N, 2=first, 3=second ... 9=eighth.
    float speed = 0; // Native signed GetSpeed value; not converted to HUD units.
};
inline Snapshot failure(Status status) {
    Snapshot result{};
    result.status = status;
    return result;
}
template<class T>
inline bool readValue(p::ReadMemory read, p::Address address, T& value) {
    return read && read(address, &value, sizeof(value));
}
template<std::size_t N>
inline bool matches(p::ReadMemory read, p::Address address,
                    const unsigned char (&expected)[N]) {
    unsigned char actual[N]{};
    return read && read(address, actual, N) &&
           std::memcmp(actual, expected, N) == 0;
}
inline Snapshot sample(p::Address base, const p::Player& player, p::ReadMemory read) {
    if (player.status != p::ResolveStatus::Unique) return Snapshot{};
    if (!read || base != p::kImageBase)
        return failure(Status::FingerprintMismatch);
    // Leave room for every offset below; reject wraparound before reading.
    if (!p::pointerLike(player.vehicle) || !p::pointerLike(player.input) ||
        player.vehicle > 0xFFFFFE00u || player.input > 0xFFFFFE00u ||
        !p::validGetterResult(player.input, player.controls))
        return failure(Status::IdentityMismatch);

    p::Address carVtable = 0, input = 0, inputVtable = 0, iv = 0, ivVtable = 0;
    p::Address transmission = 0, transmissionVtable = 0;
    if (!readValue(read, player.vehicle, carVtable) ||
        !readValue(read, player.vehicle + p::kVehicleInputOffset, input) ||
        !readValue(read, player.input, inputVtable))
        return failure(Status::ReadFailed);
    if (carVtable != base + p::kVehicleVtableRva || input != player.input ||
        inputVtable != base + p::kPlayerInputVtableRva)
        return failure(Status::IdentityMismatch);
    if (!readValue(read, player.input + kInputVehicleOffset, iv))
        return failure(Status::ReadFailed);
    if (iv != player.vehicle + kVehicleInterfaceOffset)
        return failure(Status::VehicleInterfaceMismatch);
    if (!readValue(read, iv, ivVtable))
        return failure(Status::ReadFailed);
    if (ivVtable != base + kVehicleInterfaceVtableRva)
        return failure(Status::VehicleInterfaceMismatch);
    if (!readValue(read, player.vehicle + kTransmissionOffset, transmission))
        return failure(Status::ReadFailed);
    if (!p::pointerLike(transmission))
        return failure(Status::TransmissionMismatch);
    if (!readValue(read, transmission, transmissionVtable))
        return failure(Status::ReadFailed);
    if (transmissionVtable != base + kTransmissionVtableRva)
        return failure(Status::TransmissionMismatch);

    p::Address gearFn = 0, speedFn = 0;
    if (!readValue(read, transmissionVtable + kGetGearSlot * 4, gearFn) ||
        !readValue(read, ivVtable + kGetSpeedSlot * 4, speedFn))
        return failure(Status::ReadFailed);
    if (gearFn != base + kGetGearRva || speedFn != base + kGetSpeedRva ||
        !matches(read, gearFn, kGetGearCode) ||
        !matches(read, speedFn, kGetSpeedCode))
        return failure(Status::FingerprintMismatch);

    std::uint32_t gear = 0;
    float speed = 0;
    if (!readValue(read, transmission + kGearOffset, gear) ||
        !readValue(read, iv + kSpeedOffset, speed))
        return failure(Status::ReadFailed);
    if (gear > 9 || !std::isfinite(speed) || std::fabs(speed) > 1000.0f)
        return failure(Status::ValueRejected);

    // Re-resolve after reading. No stale/ambiguous object is reported as valid.
    // Separate reads are NOT an atomic game snapshot. Callers must also enforce
    // current identity/sample/deadline constraints; this is no real-time guarantee.
    const p::Player fresh = p::resolvePlayer(base, read);
    p::Address lastTransmission = 0, lastIv = 0, lastTransmissionVtable = 0;
    p::Address lastIvVtable = 0;
    if (fresh.status != p::ResolveStatus::Unique ||
        fresh.vehicle != player.vehicle || fresh.input != player.input ||
        !readValue(read, player.vehicle + kTransmissionOffset, lastTransmission) ||
        !readValue(read, player.input + kInputVehicleOffset, lastIv) ||
        !readValue(read, transmission, lastTransmissionVtable) ||
        !readValue(read, iv, lastIvVtable) ||
        lastTransmission != transmission || lastIv != iv ||
        lastTransmissionVtable != transmissionVtable || lastIvVtable != ivVtable)
        return failure(Status::ChangedDuringRead);
    return {Status::Ok, transmission, iv, static_cast<int>(gear), speed};
}
} // namespace mw05_drive_probe
#endif
