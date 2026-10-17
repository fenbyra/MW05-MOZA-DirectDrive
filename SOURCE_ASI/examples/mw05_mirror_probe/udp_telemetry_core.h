#ifndef MW05_UDP_TELEMETRY_CORE_H
#define MW05_UDP_TELEMETRY_CORE_H
#include "hud_telemetry_core.h"
#include <array>
#include <cstring>
#include <limits>
namespace mw05_udp {
// Horizon Car Dash 324-byte little-endian wire layout. RPM-only compatibility.
// Scale is a DISPLAY reference, not a measured engine limiter. Other fields unavailable.
using Packet=std::array<unsigned char,324>;
static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559,"IEEE754 float required");
inline void u32(Packet& b,unsigned o,std::uint32_t v){
    for(unsigned i=0;i<4;++i)b[o+i]=static_cast<unsigned char>((v>>(i*8))&255u);
}
inline void f32(Packet& b,unsigned o,float v){std::uint32_t bits=0;std::memcpy(&bits,&v,4);u32(b,o,bits);}
inline bool valid(const mw05_hud::Snapshot& v){
    return v.status==mw05_hud::Status::Ok && v.active && v.vehicle!=0 && v.input!=0 &&
        std::isfinite(v.current) && v.current>=0 && v.current<=100000 &&
        std::isfinite(v.scale) && v.scale>0 && v.scale<=100000;
}
inline Packet encode(const mw05_hud::Snapshot& v,std::uint32_t now,bool live){
    Packet b{};u32(b,4,now);
    if(live && valid(v)){u32(b,0,1);f32(b,8,v.scale);f32(b,16,v.current);}
    // IdleRPM=0 unknown; speed/gear/vehicle/physics fields zero placeholders, not measurements.
    return b;
}
enum class Action {None,Live,Clear};
struct Stream {bool live=false;unsigned clears=0;std::uint32_t vehicle=0,input=0;};
inline Action step(Stream& s,const mw05_hud::Snapshot& v){
    if(valid(v)){
        // A different verified player can start a new stream; no cached packet is reused.
        s.live=true;s.clears=0;s.vehicle=v.vehicle;s.input=v.input;return Action::Live;
    }
    if(s.live){s.live=false;s.clears=3;s.vehicle=0;s.input=0;}
    if(s.clears){--s.clears;return Action::Clear;}
    return Action::None;
}
}
#endif
