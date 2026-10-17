#ifndef MW05_SESSION_GATE_H
#define MW05_SESSION_GATE_H
#include "probe_core.h"
#include <cstring>
namespace mw05_session_gate {
using Address = mw05_probe::Address;
constexpr Address kFlowRva=0x00525E90u;
constexpr Address kSimFlagsRva=0x00511FE8u;
constexpr Address kBlockedPredicateRva=0x000AE8B0u;
constexpr Address kRaceWriteRva=0x002627E7u;
constexpr unsigned char kBlockedPredicate[]={0xA1,0xE8,0x1F,0x91,0x00,0xA8,0x3B,0x75,0x0A,0xA9,0x00,0x48,0x03,0x00,0x75,0x03,0x32,0xC0,0xC3,0xB0,0x01,0xC3};
constexpr unsigned char kRaceWrite[]={0xC7,0x05,0x90,0x5E,0x92,0x00,0x06,0x00,0x00,0x00};
constexpr std::uint32_t kBlockingMask=0x0003483Bu;
struct Snapshot {
    bool readable=false;
    std::uint32_t flow=0xFFFFFFFFu, flags=0xFFFFFFFFu;
    bool ready() const { return readable && flow==6 && (flags&kBlockingMask)==0; }
};
template<class Reader> Snapshot sample(Address base,Reader read) {
    Snapshot s;
    if (base!=mw05_probe::kImageBase) return s;
    s.readable=read(base+kFlowRva,&s.flow,sizeof(s.flow)) &&
               read(base+kSimFlagsRva,&s.flags,sizeof(s.flags));
    return s;
}
template<class Reader> bool fingerprint(Address base, Reader read) {
    unsigned char a[sizeof(kBlockedPredicate)]{},b[sizeof(kRaceWrite)]{};
    return base==mw05_probe::kImageBase &&
      read(base+kBlockedPredicateRva,a,sizeof(a)) &&
      read(base+kRaceWriteRva,b,sizeof(b)) &&
      std::memcmp(a,kBlockedPredicate,sizeof(a))==0 &&
      std::memcmp(b,kRaceWrite,sizeof(b))==0;
}
} // namespace mw05_session_gate
#endif
