#include "../examples/mw05_mirror_probe/probe_core.h"
#include <cassert>
#include <map>
#include <cstdio>
#include <cstring>
namespace p=mw05_probe;
static std::map<p::Address,unsigned char> memory;
template<class T> void put(p::Address a,T value){const auto* b=reinterpret_cast<const unsigned char*>(&value);for(std::size_t i=0;i<sizeof(value);++i)memory[a+i]=b[i];}
static bool read(p::Address a,void* out,std::size_t n){auto* b=static_cast<unsigned char*>(out);for(std::size_t i=0;i<n;++i){auto it=memory.find(a+i);if(it==memory.end())return false;b[i]=it->second;}return true;}
int main(){
    // Recorded BEFORE snapshot; no user paths or executable bytes.
    memory.clear();
    // Synthetic 19-entry header; old snapshot did NOT record real count metadata.
    put(p::kImageBase+p::kVehicleContainerRva,p::VehicleContainer{p::kImageBase+p::kVehicleContainerVtableRva,p::kImageBase+p::kVehicleListRva,p::kMaxVehicles,19});
    put(p::kImageBase+p::kVehicleListRva+0u*8u,p::VehicleEntry{0x0335E870u,1,{0,0,0}});
    put(0x0335E870u,std::uint32_t{0x008AA9D8u});put(0x0335E870u+p::kVehicleInputOffset,std::uint32_t{0x033A0378u});
    put(0x033A0378u,std::uint32_t{0x008AC670u});
    put(p::kImageBase+p::kVehicleListRva+1u*8u,p::VehicleEntry{0x033B0980u,1,{0,0,0}});
    put(0x033B0980u,std::uint32_t{0x008AA9D8u});put(0x033B0980u+p::kVehicleInputOffset,std::uint32_t{0x033B0938u});
    put(0x033B0938u,std::uint32_t{0x008AB5C8u});
    put(p::kImageBase+p::kVehicleListRva+2u*8u,p::VehicleEntry{0x033B3350u,1,{0,0,0}});
    put(0x033B3350u,std::uint32_t{0x008AA9D8u});put(0x033B3350u+p::kVehicleInputOffset,std::uint32_t{0x033B31B8u});
    put(0x033B31B8u,std::uint32_t{0x008AB5C8u});
    put(p::kImageBase+p::kVehicleListRva+3u*8u,p::VehicleEntry{0x033B6CD0u,1,{0,0,0}});
    put(0x033B6CD0u,std::uint32_t{0x008AA9D8u});put(0x033B6CD0u+p::kVehicleInputOffset,std::uint32_t{0x033B5938u});
    put(0x033B5938u,std::uint32_t{0x008AB5C8u});
    put(p::kImageBase+p::kVehicleListRva+4u*8u,p::VehicleEntry{0x033C6540u,1,{0,0,0}});
    put(0x033C6540u,std::uint32_t{0x008AA9D8u});put(0x033C6540u+p::kVehicleInputOffset,std::uint32_t{0x033BEF38u});
    put(0x033BEF38u,std::uint32_t{0x008AB5C8u});
    put(p::kImageBase+p::kVehicleListRva+5u*8u,p::VehicleEntry{0x033D2820u,1,{0,0,0}});
    put(0x033D2820u,std::uint32_t{0x008AA9D8u});put(0x033D2820u+p::kVehicleInputOffset,std::uint32_t{0x033B95B8u});
    put(0x033B95B8u,std::uint32_t{0x008AB5C8u});
    put(p::kImageBase+p::kVehicleListRva+6u*8u,p::VehicleEntry{0x033D4C90u,1,{0,0,0}});
    put(0x033D4C90u,std::uint32_t{0x008AA9D8u});put(0x033D4C90u+p::kVehicleInputOffset,std::uint32_t{0x033C96B8u});
    put(0x033C96B8u,std::uint32_t{0x008AB5C8u});
    put(p::kImageBase+p::kVehicleListRva+7u*8u,p::VehicleEntry{0x033D90E0u,1,{0,0,0}});
    put(0x033D90E0u,std::uint32_t{0x008AA9D8u});put(0x033D90E0u+p::kVehicleInputOffset,std::uint32_t{0x033D7238u});
    put(0x033D7238u,std::uint32_t{0x008AB5C8u});
    put(p::kImageBase+p::kVehicleListRva+8u*8u,p::VehicleEntry{0x033DC570u,1,{0,0,0}});
    put(0x033DC570u,std::uint32_t{0x008AA9D8u});put(0x033DC570u+p::kVehicleInputOffset,std::uint32_t{0x033DBA38u});
    put(0x033DBA38u,std::uint32_t{0x008AB5C8u});
    put(p::kImageBase+p::kVehicleListRva+9u*8u,p::VehicleEntry{0x033E7800u,1,{0,0,0}});
    put(0x033E7800u,std::uint32_t{0x008AA9D8u});put(0x033E7800u+p::kVehicleInputOffset,std::uint32_t{0x033E4A38u});
    put(0x033E4A38u,std::uint32_t{0x008AB5C8u});
    put(p::kImageBase+p::kVehicleListRva+10u*8u,p::VehicleEntry{0x033E9050u,1,{0,0,0}});
    put(0x033E9050u,std::uint32_t{0x008AA9D8u});put(0x033E9050u+p::kVehicleInputOffset,std::uint32_t{0x033E57B8u});
    put(0x033E57B8u,std::uint32_t{0x008AB5C8u});
    put(p::kImageBase+p::kVehicleListRva+11u*8u,p::VehicleEntry{0x033E55D0u,0,{0,0,0}});
    put(0x033E55D0u,std::uint32_t{0x033E5820u});put(0x033E55D0u+p::kVehicleInputOffset,std::uint32_t{0x3EAAAAADu});
    put(p::kImageBase+p::kVehicleListRva+12u*8u,p::VehicleEntry{0x033E55D0u,1,{0,0,0}});
    put(0x033E55D0u,std::uint32_t{0x033E5820u});put(0x033E55D0u+p::kVehicleInputOffset,std::uint32_t{0x3EAAAAADu});
    put(p::kImageBase+p::kVehicleListRva+13u*8u,p::VehicleEntry{0x033E7800u,1,{0,0,0}});
    put(0x033E7800u,std::uint32_t{0x008AA9D8u});put(0x033E7800u+p::kVehicleInputOffset,std::uint32_t{0x033E4A38u});
    put(0x033E4A38u,std::uint32_t{0x008AB5C8u});
    put(p::kImageBase+p::kVehicleListRva+14u*8u,p::VehicleEntry{0x033E6350u,1,{0,0,0}});
    put(0x033E6350u,std::uint32_t{0x033E65A0u});put(0x033E6350u+p::kVehicleInputOffset,std::uint32_t{0x405FFFF5u});
    put(p::kImageBase+p::kVehicleListRva+15u*8u,p::VehicleEntry{0x033E55D0u,1,{0,0,0}});
    put(0x033E55D0u,std::uint32_t{0x033E5820u});put(0x033E55D0u+p::kVehicleInputOffset,std::uint32_t{0x3EAAAAADu});
    put(p::kImageBase+p::kVehicleListRva+16u*8u,p::VehicleEntry{0x033E6350u,1,{0,0,0}});
    put(0x033E6350u,std::uint32_t{0x033E65A0u});put(0x033E6350u+p::kVehicleInputOffset,std::uint32_t{0x405FFFF5u});
    put(p::kImageBase+p::kVehicleListRva+17u*8u,p::VehicleEntry{0x033E2890u,0,{0,0,0}});
    put(0x033E2890u,std::uint32_t{0x033E2AA0u});put(0x033E2890u+p::kVehicleInputOffset,std::uint32_t{0x3EAAAAADu});
    put(p::kImageBase+p::kVehicleListRva+18u*8u,p::VehicleEntry{0x033E2890u,1,{0,0,0}});
    put(0x033E2890u,std::uint32_t{0x033E2AA0u});put(0x033E2890u+p::kVehicleInputOffset,std::uint32_t{0x3EAAAAADu});
    put(p::kImageBase+p::kVehicleListRva+19u*8u,p::VehicleEntry{});
    {const auto found=p::resolvePlayer(p::kImageBase,&read);assert(found.status==p::ResolveStatus::Unique && found.vehicle==0x0335E870u && found.input==0x033A0378u);}
    // Recorded AFTER snapshot; no user paths or executable bytes.
    memory.clear();
    // Synthetic 19-entry header; old snapshot did NOT record real count metadata.
    put(p::kImageBase+p::kVehicleContainerRva,p::VehicleContainer{p::kImageBase+p::kVehicleContainerVtableRva,p::kImageBase+p::kVehicleListRva,p::kMaxVehicles,19});
    put(p::kImageBase+p::kVehicleListRva+0u*8u,p::VehicleEntry{0x033E7800u,1,{0,0,0}});
    put(0x033E7800u,std::uint32_t{0x008AA9D8u});put(0x033E7800u+p::kVehicleInputOffset,std::uint32_t{0x033A0378u});
    put(0x033A0378u,std::uint32_t{0x008AC670u});
    put(p::kImageBase+p::kVehicleListRva+1u*8u,p::VehicleEntry{0x033D90E0u,1,{0,0,0}});
    put(0x033D90E0u,std::uint32_t{0x008AA9D8u});put(0x033D90E0u+p::kVehicleInputOffset,std::uint32_t{0x033D7FB8u});
    put(0x033D7FB8u,std::uint32_t{0x008AB5C8u});
    put(p::kImageBase+p::kVehicleListRva+2u*8u,p::VehicleEntry{0x033D2820u,1,{0,0,0}});
    put(0x033D2820u,std::uint32_t{0x008AA9D8u});put(0x033D2820u+p::kVehicleInputOffset,std::uint32_t{0x033E57B8u});
    put(0x033E57B8u,std::uint32_t{0x008AB5C8u});
    put(p::kImageBase+p::kVehicleListRva+3u*8u,p::VehicleEntry{0x033B6CD0u,1,{0,0,0}});
    put(0x033B6CD0u,std::uint32_t{0x008AA9D8u});put(0x033B6CD0u+p::kVehicleInputOffset,std::uint32_t{0x033E4A38u});
    put(0x033E4A38u,std::uint32_t{0x008AB5C8u});
    put(p::kImageBase+p::kVehicleListRva+4u*8u,p::VehicleEntry{0x033B0980u,1,{0,0,0}});
    put(0x033B0980u,std::uint32_t{0x008AA9D8u});put(0x033B0980u+p::kVehicleInputOffset,std::uint32_t{0x033D7238u});
    put(0x033D7238u,std::uint32_t{0x008AB5C8u});
    put(p::kImageBase+p::kVehicleListRva+5u*8u,p::VehicleEntry{0x0335E870u,1,{0,0,0}});
    put(0x0335E870u,std::uint32_t{0x008AA9D8u});put(0x0335E870u+p::kVehicleInputOffset,std::uint32_t{0x033DBA38u});
    put(0x033DBA38u,std::uint32_t{0x008AB5C8u});
    put(p::kImageBase+p::kVehicleListRva+6u*8u,p::VehicleEntry{0x033D4C90u,1,{0,0,0}});
    put(0x033D4C90u,std::uint32_t{0x008AB758u});put(0x033D4C90u+p::kVehicleInputOffset,std::uint32_t{0x00000000u});
    put(p::kImageBase+p::kVehicleListRva+7u*8u,p::VehicleEntry{0x033D90E0u,1,{0,0,0}});
    put(0x033D90E0u,std::uint32_t{0x008AA9D8u});put(0x033D90E0u+p::kVehicleInputOffset,std::uint32_t{0x033D7FB8u});
    put(0x033D7FB8u,std::uint32_t{0x008AB5C8u});
    put(p::kImageBase+p::kVehicleListRva+8u*8u,p::VehicleEntry{0x033DC570u,1,{0,0,0}});
    put(0x033DC570u,std::uint32_t{0x008AB758u});put(0x033DC570u+p::kVehicleInputOffset,std::uint32_t{0x00000000u});
    put(p::kImageBase+p::kVehicleListRva+9u*8u,p::VehicleEntry{0x033E7800u,1,{0,0,0}});
    put(0x033E7800u,std::uint32_t{0x008AA9D8u});put(0x033E7800u+p::kVehicleInputOffset,std::uint32_t{0x033A0378u});
    put(0x033A0378u,std::uint32_t{0x008AC670u});
    put(p::kImageBase+p::kVehicleListRva+10u*8u,p::VehicleEntry{0x033E9050u,1,{0,0,0}});
    put(0x033E9050u,std::uint32_t{0x00000000u});put(0x033E9050u+p::kVehicleInputOffset,std::uint32_t{0x033E9134u});
    put(0x033E9134u,std::uint32_t{0x00000000u});
    put(p::kImageBase+p::kVehicleListRva+11u*8u,p::VehicleEntry{0x033E55D0u,0,{0,0,0}});
    put(0x033E55D0u,std::uint32_t{0x033E5820u});put(0x033E55D0u+p::kVehicleInputOffset,std::uint32_t{0x033E56B4u});
    put(0x033E56B4u,std::uint32_t{0x00000000u});
    put(p::kImageBase+p::kVehicleListRva+12u*8u,p::VehicleEntry{0x033E55D0u,1,{0,0,0}});
    put(0x033E55D0u,std::uint32_t{0x033E5820u});put(0x033E55D0u+p::kVehicleInputOffset,std::uint32_t{0x033E56B4u});
    put(0x033E56B4u,std::uint32_t{0x00000000u});
    put(p::kImageBase+p::kVehicleListRva+13u*8u,p::VehicleEntry{0x033E7800u,1,{0,0,0}});
    put(0x033E7800u,std::uint32_t{0x008AA9D8u});put(0x033E7800u+p::kVehicleInputOffset,std::uint32_t{0x033A0378u});
    put(0x033A0378u,std::uint32_t{0x008AC670u});
    put(p::kImageBase+p::kVehicleListRva+14u*8u,p::VehicleEntry{0x033E6350u,1,{0,0,0}});
    put(0x033E6350u,std::uint32_t{0x033E65A0u});put(0x033E6350u+p::kVehicleInputOffset,std::uint32_t{0x405FFFF5u});
    put(p::kImageBase+p::kVehicleListRva+15u*8u,p::VehicleEntry{0x033E55D0u,1,{0,0,0}});
    put(0x033E55D0u,std::uint32_t{0x033E5820u});put(0x033E55D0u+p::kVehicleInputOffset,std::uint32_t{0x033E56B4u});
    put(0x033E56B4u,std::uint32_t{0x00000000u});
    put(p::kImageBase+p::kVehicleListRva+16u*8u,p::VehicleEntry{0x033E6350u,1,{0,0,0}});
    put(0x033E6350u,std::uint32_t{0x033E65A0u});put(0x033E6350u+p::kVehicleInputOffset,std::uint32_t{0x405FFFF5u});
    put(p::kImageBase+p::kVehicleListRva+17u*8u,p::VehicleEntry{0x033E2890u,0,{0,0,0}});
    put(0x033E2890u,std::uint32_t{0x033E2AA0u});put(0x033E2890u+p::kVehicleInputOffset,std::uint32_t{0x3EAAAAADu});
    put(p::kImageBase+p::kVehicleListRva+18u*8u,p::VehicleEntry{0x033E2890u,1,{0,0,0}});
    put(0x033E2890u,std::uint32_t{0x033E2AA0u});put(0x033E2890u+p::kVehicleInputOffset,std::uint32_t{0x3EAAAAADu});
    put(p::kImageBase+p::kVehicleListRva+19u*8u,p::VehicleEntry{});
    {const auto found=p::resolvePlayer(p::kImageBase,&read);assert(found.status==p::ResolveStatus::Unique && found.vehicle==0x033E7800u && found.input==0x033A0378u);}
    std::puts("PASS: 2 legacy transition ROW-content regressions with synthetic count metadata");
}
