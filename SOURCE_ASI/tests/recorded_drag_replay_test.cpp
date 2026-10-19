// Active row fields from the two observed drag episodes in the supplied diagnostic log.
// Replays only these reads; not a process dump or execution of game physics.
#include "probe_core.h"
#include <cassert>
#include <map>
#include <cstdio>
namespace p=mw05_probe;
std::map<p::Address,unsigned char> memory;
template<class T> void put(p::Address a,const T& v){auto*b=reinterpret_cast<const unsigned char*>(&v);for(unsigned i=0;i<sizeof(v);++i)memory[a+i]=b[i];}
bool read(p::Address a,void*out,std::size_t n){auto*b=static_cast<unsigned char*>(out);for(unsigned i=0;i<n;++i){auto it=memory.find(a+i);if(it==memory.end())return false;b[i]=it->second;}return true;}
void row(unsigned i,unsigned enabled,p::Address car,p::Address cv,p::Address input,p::Address iv){
 put(p::kImageBase+p::kVehicleListRva+i*8,p::VehicleEntry{car,static_cast<unsigned char>(enabled),{0,0,0}});
 put(car,cv);put(car+p::kVehicleInputOffset,input);if(p::pointerLike(input))put(input,iv);
}
int main(){
 memory.clear();put(p::kImageBase+p::kVehicleContainerRva,p::VehicleContainer{0x008AA038u,0x009352B0u,160u,10u});
 row(0,1,0x0337CB50u,0x008AA9D8u,0x0337D468u,0x008AC708u);
 row(1,1,0x0338A090u,0x008AA9D8u,0x0338B038u,0x008AB5C8u);
 row(2,1,0x0338D380u,0x008AA9D8u,0x0338E3B8u,0x008AB5C8u);
 row(3,1,0x03390790u,0x008AA9D8u,0x03391738u,0x008AB5C8u);
 row(4,1,0x03393940u,0x008AA9D8u,0x033945B8u,0x008AB5C8u);
 row(5,1,0x03395DA0u,0x008AA9D8u,0x03396A38u,0x008AB5C8u);
 row(6,1,0x033983D0u,0x008AA9D8u,0x03399038u,0x008AB5C8u);
 row(7,1,0x0339A9D0u,0x008AA9D8u,0x0339B638u,0x008AB5C8u);
 row(8,1,0x0339CFD0u,0x008AA9D8u,0x0339DC38u,0x008AB5C8u);
 row(9,1,0x0339F5D0u,0x008AA9D8u,0x033A0238u,0x008AB5C8u);
 {const auto found=p::resolvePlayer(p::kImageBase,&read);
#ifdef EXPECT_LEGACY_REJECTION
 assert(found.status==p::ResolveStatus::NoPlayer);
#else
 assert(found.status==p::ResolveStatus::Unique && found.vehicle==0x0337CB50u && found.input==0x0337D468u);
#endif
 }
 memory.clear();put(p::kImageBase+p::kVehicleContainerRva,p::VehicleContainer{0x008AA038u,0x009352B0u,160u,10u});
 row(0,1,0x033983D0u,0x008AA9D8u,0x0337D468u,0x008AC708u);
 row(1,1,0x03395DA0u,0x008AA9D8u,0x033EE338u,0x008AB5C8u);
 row(2,1,0x03391770u,0x008AA9D8u,0x033DB338u,0x008AB5C8u);
 row(3,1,0x0338E3F0u,0x008AA9D8u,0x033EAA38u,0x008AB5C8u);
 row(4,1,0x0338B070u,0x008AA9D8u,0x03391738u,0x008AB5C8u);
 row(5,1,0x0337CB50u,0x008AA9D8u,0x0338E3B8u,0x008AB5C8u);
 row(6,1,0x0337D750u,0x008AA9D8u,0x0338B038u,0x008AB5C8u);
 row(7,1,0x0339A9D0u,0x008AA9D8u,0x03396A38u,0x008AB5C8u);
 row(8,1,0x0339F5D0u,0x008AA9D8u,0x033E9AB8u,0x008AB5C8u);
 row(9,1,0x0339CFD0u,0x008AA9D8u,0x033ED438u,0x008AB5C8u);
 {const auto found=p::resolvePlayer(p::kImageBase,&read);
#ifdef EXPECT_LEGACY_REJECTION
 assert(found.status==p::ResolveStatus::NoPlayer);
#else
 assert(found.status==p::ResolveStatus::Unique && found.vehicle==0x033983D0u && found.input==0x0337D468u);
#endif
 }
 std::puts("PASS: 2 observed drag-row replays (not live game)");
}
