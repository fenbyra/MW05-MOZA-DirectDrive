#ifndef MW05_TELEMETRY_CORE_H
#define MW05_TELEMETRY_CORE_H
#include "drive_probe.h"
#include "session_gate.h"
namespace mw05_telemetry {
namespace p=mw05_probe;
namespace d=mw05_drive_probe;
namespace session=mw05_session_gate;
// Exact-EXE candidates: raw values need comparison to HUD before use as RPM.
// No functions are called and no game/device memory is written by this header.
constexpr p::Address kEngineDelta=0x14u; // same root: transmission+4C, engine+60.
constexpr p::Address kEngineVtableRva=0x004AB6C8u;
constexpr p::Address kCurrentRva=0x002A0580u; // transmission slot8 -> [this+34].
constexpr p::Address kMaxRva=0x002A0530u;     // engine slot2 -> [this+30].
constexpr p::Address kLimitRva=0x002A0520u;   // engine slot3 -> [this+34].
constexpr unsigned char kCurrentCode[]={0xD9,0x41,0x34,0xC3};
constexpr unsigned char kMaxCode[]={0xD9,0x41,0x30,0xC3};
constexpr unsigned char kLimitCode[]={0xD9,0x41,0x34,0xC3};
// Constructor sets transmission root+4C and engine root+60 vtables.
constexpr p::Address kCtorLayoutRva=0x002A019Eu;
constexpr unsigned char kCtorLayoutCode[]={0x8D,0x7E,0x4C,0x8D,0x5E,0x54,0x8D,0x6E,0x60};
constexpr p::Address kCtorRva=0x002A01B8u;
constexpr unsigned char kCtorCode[]={0xC7,0x07,0x20,0xB7,0x8A,0x00,
    0xC7,0x03,0xE0,0xB6,0x8A,0x00,0xC7,0x46,0x5C,0xDC,0xB6,0x8A,0x00,
    0xC7,0x45,0x00,0xC8,0xB6,0x8A,0x00};
enum class Status:unsigned {
    NoPlayer=0,Ok=1,DriveUnavailable=2,LayoutMismatch=3,FingerprintMismatch=4,
    ReadFailed=5,ValueRejected=6,ChangedDuringRead=7,ContextBlocked=8
};
struct RawCandidates {
    bool available=false; // Identified object/context stable after reads; NOT usable RPM.
    unsigned rejectedMask=0; // bit1=current, bit2=maximum, bit4=limit.
    float current=0,maximum=0,limit=0;
    p::Address vehicle=0,transmission=0,engine=0;
};
struct Snapshot {
    Status status=Status::NoPlayer;
    p::Address vehicle=0,transmission=0,engine=0;
    float rpm=0,maxRpm=0,limitRpm=0,speed=0;
    int gear=-1;
    RawCandidates raw{};
};
inline Snapshot failure(Status status) {Snapshot s{};s.status=status;return s;}
inline unsigned rejectedMask(float current,float maximum,float limit) {
    unsigned mask=0;
    if(!std::isfinite(current) || current<0 || current>100000) mask|=1u;
    if(!std::isfinite(maximum) || maximum<=0 || maximum>100000) mask|=2u;
    if(!std::isfinite(limit) || limit<=0 || limit>100000) mask|=4u;
    return mask;
}
inline bool candidateValid(float current,float maximum,float limit) {
    return rejectedMask(current,maximum,limit)==0;
}
inline std::uint32_t rawBits(float value) {
    std::uint32_t bits=0;static_assert(sizeof(bits)==sizeof(value),"32-bit float required");
    std::memcpy(&bits,&value,sizeof(bits));return bits;
}
template<class Read> bool fingerprint(p::Address base,Read read) {
    return base==p::kImageBase && d::matches(read,base+kCurrentRva,kCurrentCode)&&
           d::matches(read,base+kMaxRva,kMaxCode)&&d::matches(read,base+kLimitRva,kLimitCode)&&
           d::matches(read,base+kCtorRva,kCtorCode)&&
           d::matches(read,base+kCtorLayoutRva,kCtorLayoutCode);
}
inline Snapshot sample(p::Address base,const p::Player& player,p::ReadMemory read) {
    if(player.status!=p::ResolveStatus::Unique)return Snapshot{};
    if(!read || base!=p::kImageBase)return failure(Status::FingerprintMismatch);
    if(!session::sample(base,read).ready())return failure(Status::ContextBlocked);
    const auto drive=d::sample(base,player,read);
    if(drive.status!=d::Status::Ok)return failure(Status::DriveUnavailable);
    if(!p::pointerLike(drive.transmission) || drive.transmission>0xFFFFFD00u)
        return failure(Status::LayoutMismatch);
    const p::Address engine=drive.transmission+kEngineDelta;
    p::Address vt=0,currentFn=0,maxFn=0,limitFn=0;
    if(!d::readValue(read,engine,vt) ||
       !d::readValue(read,base+d::kTransmissionVtableRva+8u*4u,currentFn))
        return failure(Status::ReadFailed);
    if(vt!=base+kEngineVtableRva)return failure(Status::LayoutMismatch);
    if(!d::readValue(read,vt+2u*4u,maxFn) || !d::readValue(read,vt+3u*4u,limitFn))
        return failure(Status::ReadFailed);
    if(currentFn!=base+kCurrentRva || maxFn!=base+kMaxRva || limitFn!=base+kLimitRva ||
       !fingerprint(base,read))return failure(Status::FingerprintMismatch);
    float rpm=0,maximum=0,limit=0;
    if(!d::readValue(read,drive.transmission+0x34u,rpm) ||
       !d::readValue(read,engine+0x30u,maximum) || !d::readValue(read,engine+0x34u,limit))
        return failure(Status::ReadFailed);
    // Discard changed identity/context; sequential memory reads are not atomic.
    const auto fresh=p::resolvePlayer(base,read);
    p::Address lastTransmission=0,lastTransmissionVt=0,lastEngineVt=0;
    if(fresh.status!=p::ResolveStatus::Unique || fresh.vehicle!=player.vehicle ||
       fresh.input!=player.input || fresh.controls!=player.controls ||
       !d::readValue(read,player.vehicle+d::kTransmissionOffset,lastTransmission) ||
       lastTransmission!=drive.transmission ||
       !d::readValue(read,drive.transmission,lastTransmissionVt) ||
       lastTransmissionVt!=base+d::kTransmissionVtableRva ||
       !d::readValue(read,engine,lastEngineVt) || lastEngineVt!=vt ||
       !session::sample(base,read).ready())return failure(Status::ChangedDuringRead);
    const RawCandidates raw{true,rejectedMask(rpm,maximum,limit),rpm,maximum,limit,
                            player.vehicle,drive.transmission,engine};
    if(raw.rejectedMask) {
        auto result=failure(Status::ValueRejected);
        result.raw=raw; // Diagnostics only: validated telemetry fields remain cleared.
        return result;
    }
    return {Status::Ok,player.vehicle,drive.transmission,engine,rpm,maximum,limit,drive.speed,drive.gear,raw};
}
}
#endif
