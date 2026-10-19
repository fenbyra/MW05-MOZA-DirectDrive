#ifndef MW05_HUD_TELEMETRY_CORE_H
#define MW05_HUD_TELEMETRY_CORE_H
#include "drive_probe.h"
#include "session_gate.h"
namespace mw05_hud {
namespace p=mw05_probe;
namespace d=mw05_drive_probe;
namespace session=mw05_session_gate;
constexpr p::Address kTickRva=0x0017A6E0u;
constexpr p::Address kVtableRva=0x004A2260u;
constexpr p::Address kSecondaryRva=0x004A2248u;
constexpr unsigned char kEntry[]={0x83,0xEC,0x30,0x56,0x8B,0xF1};
constexpr unsigned char kReturn[]={0x5E,0x83,0xC4,0x30,0xC2,0x04,0x00};
constexpr unsigned char kVtable[]={0xC0,0xEB,0x59,0x00,0xE0,0xA6,0x57,0x00};
constexpr unsigned char kSecondary[]={0xB0,0xEB,0x59,0x00,0x30,0xEB,0x59,0x00,0x40,0xEB,0x59,0x00};
constexpr unsigned char kRatio[]={0xD9,0x46,0x54,0xD8,0x76,0x5C};
constexpr unsigned char kThreshold[]={0xD9,0x46,0x54,0xD8,0x5E,0x58};
constexpr unsigned char kLayout[]={0x8D,0x6E,0x30};
constexpr unsigned char kCtor[]={0xC7,0x06,0x60,0x22,0x8A,0x00,0xC7,0x45,0x00,0x48,0x22,0x8A,0x00};
enum class Source {Normal,Drag};
constexpr p::Address kDragTickRva=0x0017D130u;
constexpr p::Address kDragVtableRva=0x004A261Cu;
constexpr p::Address kDragSecondaryRva=0x004A2630u;
constexpr unsigned char kDragEntry[]={0x83,0xEC,0x2C,0x56,0x8B,0xF1};
constexpr unsigned char kDragReturn[]={0x5E,0x83,0xC4,0x2C,0xC2,0x04,0x00};
constexpr unsigned char kDragVtable[]={0xB0,0x12,0x5A,0x00,0x30,0xD1,0x57,0x00};
constexpr unsigned char kDragSecondary[]={0x90,0x12,0x5A,0x00,0x30,0x12,0x5A,0x00,0x40,0x12,0x5A,0x00};
constexpr unsigned char kDragRatio[]={0xD9,0x46,0x54,0x57,0xD8,0x76,0x5C};
constexpr unsigned char kDragSecondaryCtor[]={0xC7,0x45,0x00,0x30,0x26,0x8A,0x00};
constexpr unsigned char kDragCtor[]={0xC7,0x06,0x1C,0x26,0x8A,0x00,0xC7,0x03,0x18,0x26,0x8A,0x00};
inline bool isDrag(p::Address base,const p::Player& player){return player.inputVtable==base+p::kDragInputVtableRva;}
enum class Status:unsigned {NoSample=0,Ok=1,NoPlayer=2,ContextBlocked=3,LayoutMismatch=4,
    ReadFailed=5,ValueRejected=6,ChangedDuringRead=7,Inactive=8,Stale=9,OwnerChanged=10,
    Ambiguous=11,HookUnavailable=12};
struct Snapshot {
    Status status=Status::NoSample;
    p::Address widget=0,vehicle=0,input=0,inputVtable=0;
    float current=0,threshold=0,scale=0;
    int gear=-1;
    bool active=false;
};
inline Snapshot failure(Status s){Snapshot v{};v.status=s;return v;}
template<class Read> bool fingerprint(p::Address base,Read read,Source source=Source::Normal){
    if(source==Source::Drag)return base==p::kImageBase &&
        d::matches(read,base+kDragTickRva,kDragEntry) &&
        d::matches(read,base+0x0017D2DBu,kDragReturn) &&
        d::matches(read,base+kDragVtableRva,kDragVtable) &&
        d::matches(read,base+kDragSecondaryRva,kDragSecondary) &&
        d::matches(read,base+0x0017D144u,kDragRatio) &&
        d::matches(read,base+0x0017D19Fu,kThreshold) &&
        d::matches(read,base+0x001A0F3Eu,kLayout) &&
        d::matches(read,base+0x001A0F87u,kDragSecondaryCtor) &&
        d::matches(read,base+0x001A0F9Au,kDragCtor);
    return base==p::kImageBase && d::matches(read,base+kTickRva,kEntry) &&
        d::matches(read,base+0x0017A8B7u,kReturn) && d::matches(read,base+0x0017A8F2u,kReturn) &&
        d::matches(read,base+kVtableRva,kVtable) && d::matches(read,base+kSecondaryRva,kSecondary) &&
        d::matches(read,base+0x0017A708u,kRatio) && d::matches(read,base+0x0017A75Bu,kThreshold) &&
        d::matches(read,base+0x0019EA1Eu,kLayout) && d::matches(read,base+0x0019EA41u,kCtor);
}
inline Snapshot sample(p::Address base,const p::Player& player,p::Address widget,p::ReadMemory read,Source source=Source::Normal){
    if(player.status!=p::ResolveStatus::Unique)return failure(Status::NoPlayer);
    if(base!=p::kImageBase || !read)return failure(Status::LayoutMismatch);
    if(!session::sample(base,read).ready())return failure(Status::ContextBlocked);
    if(!p::pointerLike(widget) || widget>0xFFFFFF80u)return failure(Status::LayoutMismatch);
    p::Address vt=0,secondary=0;
    if(!d::readValue(read,widget,vt) || !d::readValue(read,widget+0x30u,secondary))return failure(Status::ReadFailed);
    if(vt!=base+(source==Source::Drag?kDragVtableRva:kVtableRva) ||
       secondary!=base+(source==Source::Drag?kDragSecondaryRva:kSecondaryRva))return failure(Status::LayoutMismatch);
    Snapshot out{};out.widget=widget;out.vehicle=player.vehicle;out.input=player.input;
    std::uint32_t m0=0,m1=0,m2=0,m3=0;
    if(!d::readValue(read,widget+0x54u,out.current) || !d::readValue(read,widget+0x58u,out.threshold) ||
       !d::readValue(read,widget+0x5Cu,out.scale) || !d::readValue(read,widget+0x60u,out.gear) ||
       !d::readValue(read,widget+0x18u,m0) || !d::readValue(read,widget+0x1Cu,m1) ||
       !d::readValue(read,widget+0x20u,m2) || !d::readValue(read,widget+0x24u,m3))return failure(Status::ReadFailed);
    const auto fresh=p::resolvePlayer(base,read);
    p::Address finalVt=0,finalSecondary=0;
    if(fresh.status!=p::ResolveStatus::Unique || fresh.vehicle!=player.vehicle || fresh.input!=player.input ||
       fresh.controls!=player.controls ||
       (player.inputVtable && fresh.inputVtable!=player.inputVtable) ||
       isDrag(base,fresh)!=(source==Source::Drag) || !d::readValue(read,widget,finalVt) || finalVt!=vt ||
       !d::readValue(read,widget+0x30u,finalSecondary) || finalSecondary!=secondary ||
       !session::sample(base,read).ready())return failure(Status::ChangedDuringRead);
    out.inputVtable=fresh.inputVtable;
    out.active=((m0&m2)|(m1&m3))!=0;
    if(!out.active)return failure(Status::Inactive);
    if(!std::isfinite(out.current) || out.current<0 || out.current>100000 ||
       !std::isfinite(out.threshold) || out.threshold<=0 || out.threshold>100000 ||
       !std::isfinite(out.scale) || out.scale<=0 || out.scale>100000 || out.gear<0 || out.gear>9)
        return failure(Status::ValueRejected);
    out.status=Status::Ok;return out;
}
constexpr std::uint32_t kFreshMs=500;
struct Cache {
    Snapshot latest{};
    bool seen=false,hadValid=false,ambiguous=false;
    std::uint32_t at=0,validAt=0;
    p::Address widget=0,vehicle=0,input=0,inputVtable=0;
};
inline void publish(Cache& c,Snapshot v,std::uint32_t now){
    if(v.status==Status::Ok){
        const bool sameContext=c.hadValid && c.vehicle==v.vehicle && c.input==v.input && c.inputVtable==v.inputVtable &&
            static_cast<std::uint32_t>(now-c.validAt)<=kFreshMs;
        if(!sameContext)c.ambiguous=false;
        else if(c.widget!=v.widget)c.ambiguous=true;
        c.hadValid=true;c.validAt=now;c.widget=v.widget;c.vehicle=v.vehicle;c.input=v.input;c.inputVtable=v.inputVtable;
        if(c.ambiguous)v=failure(Status::Ambiguous);
    }
    c.latest=v;c.at=now;c.seen=true;
}
inline Snapshot select(const Cache& c,std::uint32_t now,const p::Player& player,
                       const session::Snapshot& context,bool focused){
    if(player.status!=p::ResolveStatus::Unique)return failure(Status::NoPlayer);
    if(!focused || !context.ready())return failure(Status::ContextBlocked);
    if(!c.seen)return Snapshot{};
    if(static_cast<std::uint32_t>(now-c.at)>kFreshMs)return failure(Status::Stale);
    if(c.latest.status!=Status::Ok)return c.latest;
    if(c.latest.vehicle!=player.vehicle || c.latest.input!=player.input ||
       (player.inputVtable && c.latest.inputVtable!=player.inputVtable))return failure(Status::OwnerChanged);
    return c.latest;
}
}
#endif
