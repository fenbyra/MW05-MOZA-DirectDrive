#include "../examples/mw05_mirror_probe/session_gate.h"
#include <cassert>
#include <cstdio>
#include <map>
#include <vector>
namespace s=mw05_session_gate;
std::map<s::Address,unsigned char> mem;
bool read(s::Address a,void* out,std::size_t n) {
 auto* b=static_cast<unsigned char*>(out);
 for (std::size_t i=0;i<n;++i) { auto f=mem.find(a+i); if (f==mem.end()) return false; b[i]=f->second; }
 return true;
}
template<class T> void put(s::Address a,const T& v) {
 auto* b=reinterpret_cast<const unsigned char*>(&v);
 for(std::size_t i=0;i<sizeof(v);++i) mem[a+i]=b[i];
}
int main() {
 constexpr auto base=mw05_probe::kImageBase; unsigned tests=0;
 assert(!s::sample(base,&read).ready()); ++tests;
 put(base+s::kFlowRva,std::uint32_t{6}); put(base+s::kSimFlagsRva,std::uint32_t{0});
 assert(s::sample(base,&read).ready()); ++tests;
 for (std::uint32_t flow : {0u,1u,2u,3u,4u,5u,7u,8u,0xffffffffu}) {
  put(base+s::kFlowRva,flow); assert(!s::sample(base,&read).ready()); }
 ++tests; put(base+s::kFlowRva,std::uint32_t{6});
 for(unsigned bit=0;bit<32;++bit) { auto mask=std::uint32_t{1}<<bit;
  put(base+s::kSimFlagsRva,mask);
  assert(s::sample(base,&read).ready()==((mask&s::kBlockingMask)==0)); }
 ++tests;
 assert(!s::sample(0,&read).ready()); ++tests;
 put(base+s::kBlockedPredicateRva,s::kBlockedPredicate); put(base+s::kRaceWriteRva,s::kRaceWrite);
 assert(s::fingerprint(base,&read)); ++tests;
 mem[base+s::kBlockedPredicateRva]^=1; assert(!s::fingerprint(base,&read)); ++tests;
 std::printf("PASS: %u session-gate groups, all 32 flag bits checked\n",tests);
}
