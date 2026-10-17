#include "../examples/mw05_mirror_probe/drive_probe.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <map>

namespace p = mw05_probe;
namespace d = mw05_drive_probe;
namespace {
std::map<p::Address, unsigned char> memory;
constexpr p::Address car = 0x03203870, input = 0x0320F5A8;
constexpr p::Address iv = car + d::kVehicleInterfaceOffset, transmission = 0x0321FB3C;
bool changeTransmission = false, removePlayer = false, addSecondPlayer = false;
unsigned reads = 0;
template<class T> void put(p::Address a, const T& value) {
    const auto* b = reinterpret_cast<const unsigned char*>(&value);
    for (std::size_t i = 0; i < sizeof(value); ++i)
        memory[a + static_cast<p::Address>(i)] = b[i];
}
bool read(p::Address a, void* out, std::size_t size) {
    ++reads;
    auto* b = static_cast<unsigned char*>(out);
    for (std::size_t i = 0; i < size; ++i) {
        const auto it = memory.find(a + static_cast<p::Address>(i));
        if (it == memory.end()) return false;
        b[i] = it->second;
    }
    if (a == iv + d::kSpeedOffset) {
        if (changeTransmission) put(car + d::kTransmissionOffset, transmission + 0x100);
        if (removePlayer) put(p::kImageBase + p::kVehicleListRva, p::VehicleEntry{});
        if (addSecondPlayer) {
            put(p::kImageBase+p::kVehicleContainerRva+12,std::uint32_t{2});
            put(p::kImageBase + p::kVehicleListRva + 8,
                p::VehicleEntry{0x04110000, 1, {0,0,0}});
            put(p::kImageBase + p::kVehicleListRva + 16, p::VehicleEntry{});
            put<p::Address>(0x04110000, p::kImageBase + p::kVehicleVtableRva);
            put<p::Address>(0x04110000 + p::kVehicleInputOffset, 0x04120000);
            put<p::Address>(0x04120000, p::kImageBase + p::kPlayerInputVtableRva);
        }
    }
    return true;
}
p::Player player() { return {p::ResolveStatus::Unique, car, input, input + 8}; }
void setup(std::uint32_t gear = 2, float speed = 0) {
    memory.clear(); reads = 0;
    changeTransmission = removePlayer = addSecondPlayer = false;
    put(p::kImageBase+p::kVehicleContainerRva,p::VehicleContainer{p::kImageBase+p::kVehicleContainerVtableRva,p::kImageBase+p::kVehicleListRva,p::kMaxVehicles,1});
    put(p::kImageBase + p::kVehicleListRva, p::VehicleEntry{car, 1, {0,0,0}});
    put(p::kImageBase + p::kVehicleListRva + 8, p::VehicleEntry{});
    put(car, p::kImageBase + p::kVehicleVtableRva);
    put(car + p::kVehicleInputOffset, input);
    put(input, p::kImageBase + p::kPlayerInputVtableRva);
    put(input + d::kInputVehicleOffset, iv);
    put(iv, p::kImageBase + d::kVehicleInterfaceVtableRva);
    put(car + d::kTransmissionOffset, transmission);
    put(transmission, p::kImageBase + d::kTransmissionVtableRva);
    put(p::kImageBase + d::kTransmissionVtableRva + d::kGetGearSlot * 4,
        p::kImageBase + d::kGetGearRva);
    put(p::kImageBase + d::kVehicleInterfaceVtableRva + d::kGetSpeedSlot * 4,
        p::kImageBase + d::kGetSpeedRva);
    put(p::kImageBase + d::kGetGearRva, d::kGetGearCode);
    put(p::kImageBase + d::kGetSpeedRva, d::kGetSpeedCode);
    put(transmission + d::kGearOffset, gear);
    put(iv + d::kSpeedOffset, speed);
}
d::Snapshot sample() { return d::sample(p::kImageBase, player(), &read); }
void failed(d::Status status) {
    const auto s = sample();
    assert(s.status == status && s.gear == -1 && s.speed == 0);
    assert(s.transmission == 0 && s.vehicleInterface == 0);
}
}
int main() {
    unsigned tests = 0;
    setup();
    const auto none = d::sample(p::kImageBase, p::Player{}, &read);
    assert(none.status == d::Status::NoPlayer && reads == 0); ++tests;

    setup();
    const auto good = sample();
    assert(good.status == d::Status::Ok && good.gear == 2 && good.speed == 0);
    assert(good.transmission == transmission && good.vehicleInterface == iv); ++tests;
    for (std::uint32_t gear = 0; gear <= 9; ++gear) {
        setup(gear, 12.5f);
        const auto s = sample();
        assert(s.status == d::Status::Ok && s.gear == static_cast<int>(gear));
    }
    ++tests;
    setup(0, -3.25f);
    assert(sample().status == d::Status::Ok && sample().speed == -3.25f); ++tests;
    for (float value : {std::numeric_limits<float>::quiet_NaN(),
                        std::numeric_limits<float>::infinity(),
                        -std::numeric_limits<float>::infinity(), 1001.0f, -1001.0f}) {
        setup(2, value); failed(d::Status::ValueRejected);
    }
    ++tests;
    for (std::uint32_t value : {10u, 0xFFFFFFFFu}) {
        setup(value); failed(d::Status::ValueRejected);
    }
    ++tests;
    setup(); put(input + d::kInputVehicleOffset, iv + 4);
    failed(d::Status::VehicleInterfaceMismatch); ++tests;
    setup(); put<p::Address>(iv, 0x008AA940);
    failed(d::Status::VehicleInterfaceMismatch); ++tests;
    for (p::Address value : {0u, 0x0321FB3Du, 0xFFFFFFFCu}) {
        setup(); put(car + d::kTransmissionOffset, value);
        failed(d::Status::TransmissionMismatch);
    }
    ++tests;
    setup(); put<p::Address>(transmission, 0x008AB6E0);
    failed(d::Status::TransmissionMismatch); ++tests;
    setup(); put<p::Address>(p::kImageBase + d::kTransmissionVtableRva + 4, 0x006A0500);
    failed(d::Status::FingerprintMismatch); ++tests;
    setup(); put<p::Address>(p::kImageBase + d::kVehicleInterfaceVtableRva + 36*4, 0x00688190);
    failed(d::Status::FingerprintMismatch); ++tests;
    setup(); put<unsigned char>(p::kImageBase + d::kGetGearRva, 0xCC);
    failed(d::Status::FingerprintMismatch); ++tests;
    setup(); put<unsigned char>(p::kImageBase + d::kGetSpeedRva, 0xCC);
    failed(d::Status::FingerprintMismatch); ++tests;
    setup(); memory.erase(transmission + d::kGearOffset);
    failed(d::Status::ReadFailed); ++tests;
    setup(); changeTransmission = true;
    failed(d::Status::ChangedDuringRead); ++tests;
    setup(); removePlayer = true;
    failed(d::Status::ChangedDuringRead); ++tests;
    setup(); addSecondPlayer = true;
    failed(d::Status::ChangedDuringRead); ++tests;
    setup();
    const p::Player high{p::ResolveStatus::Unique, 0xFFFFFF00, input, input+8};
    assert(d::sample(p::kImageBase, high, &read).status == d::Status::IdentityMismatch);
    assert(reads == 0); ++tests;
    setup(); auto wrong = player(); wrong.controls += 4;
    assert(d::sample(p::kImageBase, wrong, &read).status == d::Status::IdentityMismatch);
    assert(reads == 0); ++tests;
    setup();
    assert(d::sample(0x500000, player(), &read).status == d::Status::FingerprintMismatch);
    assert(d::sample(p::kImageBase, player(), nullptr).status == d::Status::FingerprintMismatch);
    assert(reads == 0); ++tests;
    setup(); put<p::Address>(car + p::kVehicleInputOffset, input + 4);
    failed(d::Status::IdentityMismatch); ++tests;
    std::printf("PASS: %u read-only drivetrain diagnostic groups\n", tests);
}
