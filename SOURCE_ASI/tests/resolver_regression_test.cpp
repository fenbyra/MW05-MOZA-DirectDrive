#include "../examples/mw05_mirror_probe/probe_core.h"
#include <cassert>
#include <cstdio>
#include <map>
#include <limits>
namespace p=mw05_probe;
static std::map<p::Address,unsigned char> memory;
static unsigned headerReads=0, mutateHeader=0;
static bool tailRead=false;
static p::Address forbiddenTail=0;
template<class T>static void put(p::Address a,const T& v){const auto*b=reinterpret_cast<const unsigned char*>(&v);for(unsigned i=0;i<sizeof(v);++i)memory[a+i]=b[i];}
static bool read(p::Address a,void*out,std::size_t n){
 if(forbiddenTail && a>=forbiddenTail && a<p::kImageBase+p::kVehicleListRva+p::kMaxVehicles*8){tailRead=true;return false;}
 if(a==p::kImageBase+p::kVehicleContainerRva && ++headerReads==2 && mutateHeader){
  const auto off=mutateHeader==1?12u:mutateHeader==2?4u:mutateHeader==3?8u:0u;
  if(mutateHeader==5){memory.erase(a);return false;}
  put(a+off,std::uint32_t{0});
 }
 auto*b=static_cast<unsigned char*>(out);for(unsigned i=0;i<n;++i){auto it=memory.find(a+i);if(it==memory.end())return false;b[i]=it->second;}return true;
}
static void header(unsigned n){put(p::kImageBase+p::kVehicleContainerRva,p::VehicleContainer{p::kImageBase+p::kVehicleContainerVtableRva,p::kImageBase+p::kVehicleListRva,p::kMaxVehicles,n});}
static void reset(unsigned n){memory.clear();headerReads=mutateHeader=0;tailRead=false;forbiddenTail=0;header(n);}
static void car(unsigned i,p::Address obj,p::Address input,bool player=true,unsigned char enabled=1){
 put(p::kImageBase+p::kVehicleListRva+i*8,p::VehicleEntry{obj,enabled,{0,0,0}});
 put(obj,p::kImageBase+p::kVehicleVtableRva);put(obj+p::kVehicleInputOffset,input);
 put(input,player?p::kImageBase+p::kPlayerInputVtableRva:0x008AB5C8u);
}
static p::Player resolve(){return p::resolvePlayer(p::kImageBase,&read);}
int main(){unsigned tests=0;
 reset(0);forbiddenTail=p::kImageBase+p::kVehicleListRva;assert(resolve().status==p::ResolveStatus::NoPlayer&&!tailRead);++tests;
 reset(1);car(0,0x3203870,0x320F5A8);auto x=resolve();assert(x.status==p::ResolveStatus::Unique&&x.controls==0x320F5B0);++tests;
 reset(2);car(0,0x3203870,0x320F5A8,false);car(1,0x3303870,0x330F5A8);assert(resolve().vehicle==0x3303870);++tests;
 reset(2);car(0,0x3203870,0x320F5A8);car(1,0x3303870,0x330F5A8);assert(resolve().status==p::ResolveStatus::Ambiguous);++tests;
 reset(3);for(unsigned i=0;i<3;++i)car(i,0x3203870,0x320F5A8);assert(resolve().status==p::ResolveStatus::Unique);++tests;
 reset(2);car(0,0x3203870,0x320F5A8);car(1,0x3303870,0x320F5A8);assert(resolve().status==p::ResolveStatus::Ambiguous);++tests;
 reset(1);car(0,0x3203870,0x320F5A8,true,0);assert(resolve().status==p::ResolveStatus::NoPlayer);++tests;
 reset(1);car(0,0x3203870,0x320F5A8);put(0x3203870u,std::uint32_t{0xDEADBEEF});assert(resolve().status==p::ResolveStatus::NoPlayer);++tests;
 // Stale tails are not active members, even if they look exactly like players.
 reset(1);car(0,0x3203870,0x320F5A8);for(unsigned i=1;i<160;++i)car(i,0x3303870,0x330F5A8);
 forbiddenTail=p::kImageBase+p::kVehicleListRva+8;assert(resolve().status==p::ResolveStatus::Unique&&!tailRead);++tests;
 // Support full bounded capacity and candidates beyond the former 64-row ceiling.
 reset(160);for(unsigned i=0;i<160;++i)car(i,0x3400000,0x3500000,false);car(159,0x3203870,0x320F5A8);
 assert(resolve().status==p::ResolveStatus::Unique);++tests;
 reset(65);for(unsigned i=0;i<65;++i)car(i,0x3400000,0x3500000,false);car(0,0x3203870,0x320F5A8);car(64,0x3303870,0x330F5A8);
 assert(resolve().status==p::ResolveStatus::Ambiguous);++tests;
 reset(1);car(0,0x3203870,0x320F5A8);forbiddenTail=p::kImageBase+p::kVehicleListRva+8;
 assert(resolve().status==p::ResolveStatus::Unique&&!tailRead&&headerReads==2);++tests;
 reset(2);car(0,0x3203870,0x320F5A8);assert(resolve().status==p::ResolveStatus::Unreadable);++tests;
 reset(2);car(0,0x3203870,0x320F5A8);put(p::kImageBase+p::kVehicleListRva+8,p::VehicleEntry{});
 assert(resolve().status==p::ResolveStatus::InvalidContainer);++tests;
 reset(0);memory.erase(p::kImageBase+p::kVehicleContainerRva);assert(resolve().status==p::ResolveStatus::Unreadable);++tests;
 for(auto n:{161u,0xFFFFFFFFu}){reset(n);assert(resolve().status==p::ResolveStatus::InvalidContainer);}++tests;
 for(auto cap:{0u,64u,159u,161u,0xFFFFFFFFu}){reset(0);put(p::kImageBase+p::kVehicleContainerRva+8,cap);assert(resolve().status==p::ResolveStatus::InvalidContainer);}++tests;
 reset(0);put(p::kImageBase+p::kVehicleContainerRva,std::uint32_t{0});assert(resolve().status==p::ResolveStatus::InvalidContainer);++tests;
 for(auto data:{0u,0x9352B4u,0xFFFFFFFCu}){reset(0);put(p::kImageBase+p::kVehicleContainerRva+4,data);assert(resolve().status==p::ResolveStatus::InvalidContainer);}++tests;
 for(unsigned kind=1;kind<=4;++kind){reset(1);car(0,0x3203870,0x320F5A8);mutateHeader=kind;assert(resolve().status==p::ResolveStatus::ChangedDuringRead);}++tests;
 reset(1);car(0,0x3203870,0x320F5A8);mutateHeader=5;assert(resolve().status==p::ResolveStatus::Unreadable);++tests;
 assert(p::resolvePlayer(0x500000,&read).status==p::ResolveStatus::InvalidContainer);++tests;
 assert(p::resolvePlayer(p::kImageBase,nullptr).status==p::ResolveStatus::InvalidContainer);++tests;
 reset(1);put(p::kImageBase+p::kVehicleListRva,p::VehicleEntry{0xFFFFFFFCu,1,{0,0,0}});assert(resolve().status==p::ResolveStatus::NoPlayer);++tests;
 assert(p::validGetterResult(0x320F5A8,0x320F5B0)&&!p::validGetterResult(0x320F5A8,0x320F5B4)&&!p::validGetterResult(0,8));++tests;
 std::printf("PASS: %u counted-container resolver/layout groups\n",tests);
}
