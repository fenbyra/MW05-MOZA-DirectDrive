#ifndef MW05_GETCONTROLS_PROBE_CORE_H
#define MW05_GETCONTROLS_PROBE_CORE_H

#include <cstddef>
#include <cstdint>

namespace mw05_probe {

using Address = std::uint32_t;
using ReadMemory = bool (*)(Address, void*, std::size_t);

constexpr Address kImageBase = 0x00400000u;
constexpr Address kVehicleListRva = 0x005352B0u;
constexpr Address kVehicleVtableRva = 0x004AA9D8u;
constexpr Address kPlayerInputVtableRva = 0x004AC670u;
constexpr Address kGetControlsRva = 0x0029CBB0u;
constexpr Address kVehicleInputOffset = 0xE8u;
constexpr Address kControlsOffset = 0x08u;
constexpr Address kGasOffset = 0x14u;
constexpr Address kBrakeOffset = 0x18u;
constexpr unsigned kGetterSlot = 2;
// Counted inline container observed at 009352A0. Static vtable allocator
// returns this+10; fixed capacity 160 (0067A670). Grow/copy at 006749E0
// stores data+4, capacity+8 and iterates count+0C in 8-byte steps.
constexpr Address kVehicleContainerRva = kVehicleListRva - 0x10u;
constexpr Address kVehicleContainerVtableRva = 0x004AA038u;
constexpr unsigned kMaxVehicles = 160;
struct VehicleContainer {
    Address vtable, data;
    std::uint32_t capacity, count;
};
static_assert(sizeof(VehicleContainer)==0x10, "Vehicle container header");
inline bool sameContainer(const VehicleContainer& a, const VehicleContainer& b) {
    return a.vtable==b.vtable && a.data==b.data &&
           a.capacity==b.capacity && a.count==b.count;
}

struct InputControls {
    float banking;
    float steering;
    float steeringVertical;
    float strafeVertical;
    float strafeHorizontal;
    float gas;
    float brake;
    float handBrake;
    std::uint8_t actionButton;
    std::uint8_t nos;
    std::uint8_t padding[2];
};

static_assert(sizeof(InputControls) == 0x24, "InputControls size");
static_assert(offsetof(InputControls, steering) == 0x04, "Steering offset");
static_assert(offsetof(InputControls, gas) == kGasOffset, "Gas offset");
static_assert(offsetof(InputControls, brake) == kBrakeOffset, "Brake offset");
static_assert(kBrakeOffset == kGasOffset + sizeof(float), "Pedal fields are adjacent");
static_assert(offsetof(InputControls, handBrake) == 0x1C, "Handbrake offset");
static_assert(offsetof(InputControls, actionButton) == 0x20, "Action offset");
static_assert(offsetof(InputControls, nos) == 0x21, "NOS offset");

struct VehicleEntry {
    Address vehicle;
    std::uint8_t enabled;
    std::uint8_t padding[3];
};
static_assert(sizeof(VehicleEntry) == 8, "Vehicle list stride");

// Preserve old numeric statuses for existing logs; Unterminated (4) is retired.
enum class ResolveStatus { NoPlayer, Unique, Ambiguous, Unreadable, Unterminated,
                           InvalidContainer, ChangedDuringRead };

struct Player {
    ResolveStatus status = ResolveStatus::NoPlayer;
    Address vehicle = 0;
    Address input = 0;
    Address controls = 0;
};

inline bool pointerLike(Address a) {
    return a >= 0x10000u && a <= 0xFFFFFF00u && (a & 3u) == 0;
}

// Only data reads. No calls through an unverified object or vtable.
inline Player resolvePlayer(Address base, ReadMemory read) {
    Player result;
    if (!read || base!=kImageBase) return {ResolveStatus::InvalidContainer,0,0,0};
    VehicleContainer header{};
    if (!read(base+kVehicleContainerRva,&header,sizeof(header)))
        return {ResolveStatus::Unreadable,0,0,0};
    if (header.vtable!=base+kVehicleContainerVtableRva ||
        header.data!=base+kVehicleListRva || header.capacity!=kMaxVehicles ||
        header.count>header.capacity)
        return {ResolveStatus::InvalidContainer,0,0,0};
    unsigned matches = 0;
    for (unsigned i = 0; i < header.count; ++i) {
        VehicleEntry entry{};
        const Address slot = header.data + i * sizeof(VehicleEntry);
        if (!read(slot, &entry, sizeof(entry)))
            return {ResolveStatus::Unreadable, 0, 0, 0};
        // Active entries have no sentinel. A null active row is inconsistent,
        // not permission to truncate the scan and ignore later candidates.
        if (entry.vehicle == 0)
            return {ResolveStatus::InvalidContainer,0,0,0};
        if (entry.enabled != 1 || !pointerLike(entry.vehicle)) continue;
        Address carVtable = 0, input = 0, inputVtable = 0;
        if (!read(entry.vehicle, &carVtable, sizeof(carVtable)) ||
            carVtable != base + kVehicleVtableRva ||
            !read(entry.vehicle + kVehicleInputOffset, &input, sizeof(input)) ||
            !pointerLike(input) ||
            !read(input, &inputVtable, sizeof(inputVtable)) ||
            inputVtable != base + kPlayerInputVtableRva)
            continue;
        // A transition can leave repeated rows for ONE validated vehicle/input pair.
        // Count identities, not table occurrences; never merge different vehicles
        // sharing an input or different inputs. Scan the entire active count.
        if (matches != 0) {
            if (result.vehicle == entry.vehicle && result.input == input) continue;
            return {ResolveStatus::Ambiguous, 0, 0, 0};
        }
        ++matches;
        result = {ResolveStatus::Unique, entry.vehicle, input, input + kControlsOffset};
    }
    VehicleContainer after{};
    if (!read(base+kVehicleContainerRva,&after,sizeof(after)))
        return {ResolveStatus::Unreadable,0,0,0};
    if (!sameContainer(header,after))
        return {ResolveStatus::ChangedDuringRead,0,0,0};
    return result;
}

inline bool validGetterResult(Address input, Address controls) {
    return pointerLike(input) && controls == input + kControlsOffset;
}

} // namespace mw05_probe
#endif
