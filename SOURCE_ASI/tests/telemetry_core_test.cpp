#include "../examples/mw05_mirror_probe/telemetry_core.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <map>
namespace p=mw05_probe;
namespace d=mw05_drive_probe;
namespace t=mw05_telemetry;
namespace s=mw05_session_gate;
namespace {
std::map<p::Address,unsigned char> memory;
constexpr p::Address car=0x03203870,input=0x0320F5A8,transmission=0x0321FB3C;
constexpr p::Address engine=transmission+t::kEngineDelta,iv=car+d::kVehicleInterfaceOffset;
unsigned reads=0;
int mutation=0;
template<class T>void put(p::Address a,const T&v) {
 auto*b=reinterpret_cast<const unsigned char*>(&v);
 for(unsigned i=0;i<sizeof(v);++i)memory[a+i]=b[i];
}
bool read(p::Address a,void*out,std::size_t size) {
 ++reads;auto*b=static_cast<unsigned char*>(out);
 for(unsigned i=0;i<size;++i){auto it=memory.find(a+i);if(it==memory.end())return false;b[i]=it->second;}
 if(a==engine+0x34u) {
  if(mutation==1)put(car+d::kTransmissionOffset,transmission+0x100u);
  if(mutation==2)put(engine,std::uint32_t{0});
  if(mutation==3)put(p::kImageBase+s::kFlowRva,std::uint32_t{3});
  if(mutation==4)put(p::kImageBase+p::kVehicleListRva,p::VehicleEntry{});
  if(mutation==5)put(transmission,std::uint32_t{0});
 }
 return true;
}
p::Player player(){return {p::ResolveStatus::Unique,car,input,input+8};}
void setup(){
 memory.clear();reads=0;mutation=0;
 put(p::kImageBase+p::kVehicleContainerRva,p::VehicleContainer{p::kImageBase+p::kVehicleContainerVtableRva,p::kImageBase+p::kVehicleListRva,p::kMaxVehicles,1});
 put(p::kImageBase+p::kVehicleListRva,p::VehicleEntry{car,1,{0,0,0}});
 put(p::kImageBase+p::kVehicleListRva+8,p::VehicleEntry{});
 put(car,p::kImageBase+p::kVehicleVtableRva);
 put(car+p::kVehicleInputOffset,input);put(input,p::kImageBase+p::kPlayerInputVtableRva);
 put(input+d::kInputVehicleOffset,iv);put(iv,p::kImageBase+d::kVehicleInterfaceVtableRva);
 put(car+d::kTransmissionOffset,transmission);put(transmission,p::kImageBase+d::kTransmissionVtableRva);
 put(p::kImageBase+d::kTransmissionVtableRva+4,p::kImageBase+d::kGetGearRva);
 put(p::kImageBase+d::kVehicleInterfaceVtableRva+36*4,p::kImageBase+d::kGetSpeedRva);
 put(p::kImageBase+d::kGetGearRva,d::kGetGearCode);put(p::kImageBase+d::kGetSpeedRva,d::kGetSpeedCode);
 put(transmission+d::kGearOffset,std::uint32_t{2});put(iv+d::kSpeedOffset,12.5f);
 put(engine,p::kImageBase+t::kEngineVtableRva);
 put(p::kImageBase+d::kTransmissionVtableRva+8*4,p::kImageBase+t::kCurrentRva);
 put(p::kImageBase+t::kEngineVtableRva+2*4,p::kImageBase+t::kMaxRva);
 put(p::kImageBase+t::kEngineVtableRva+3*4,p::kImageBase+t::kLimitRva);
 put(p::kImageBase+t::kCurrentRva,t::kCurrentCode);put(p::kImageBase+t::kMaxRva,t::kMaxCode);
 put(p::kImageBase+t::kLimitRva,t::kLimitCode);put(p::kImageBase+t::kCtorRva,t::kCtorCode);put(p::kImageBase+t::kCtorLayoutRva,t::kCtorLayoutCode);
 put(transmission+0x34,850.0f);put(engine+0x30,8000.0f);put(engine+0x34,7500.0f);
 put(p::kImageBase+s::kFlowRva,std::uint32_t{6});put(p::kImageBase+s::kSimFlagsRva,std::uint32_t{0});
}
t::Snapshot sample(){return t::sample(p::kImageBase,player(),&read);}
void fails(t::Status status){auto x=sample();assert(x.status==status&&x.vehicle==0&&x.engine==0&&x.transmission==0&&x.gear==-1&&x.rpm==0&&x.maxRpm==0&&x.limitRpm==0&&x.speed==0);}
}
int main(){
 unsigned tests=0;
 setup();auto none=t::sample(p::kImageBase,p::Player{},&read);assert(none.status==t::Status::NoPlayer&&reads==0);++tests;
 setup();assert(t::sample(p::kImageBase,player(),nullptr).status==t::Status::FingerprintMismatch);++tests;
 setup();assert(t::sample(0x500000,player(),&read).status==t::Status::FingerprintMismatch);++tests;
 setup();auto before=memory;auto x=sample();assert(x.status==t::Status::Ok&&x.rpm==850&&x.maxRpm==8000&&x.limitRpm==7500&&x.speed==12.5&&x.gear==2&&x.engine==engine);assert(before==memory);++tests;
 setup();put(transmission+0x34,0.0f);assert(sample().status==t::Status::Ok);++tests;
 setup();put(iv+d::kSpeedOffset,-3.25f);put(transmission+d::kGearOffset,std::uint32_t{0});x=sample();assert(x.speed==-3.25f&&x.gear==0);++tests;
 setup();put(p::kImageBase+s::kFlowRva,std::uint32_t{3});fails(t::Status::ContextBlocked);++tests;
 setup();put(p::kImageBase+s::kSimFlagsRva,std::uint32_t{2});fails(t::Status::ContextBlocked);++tests;
 setup();put(car+p::kVehicleInputOffset,std::uint32_t{0});fails(t::Status::DriveUnavailable);++tests;
 setup();put(engine,std::uint32_t{0});fails(t::Status::LayoutMismatch);++tests;
 setup();memory.erase(engine);fails(t::Status::ReadFailed);++tests;
 for(auto address:{p::kImageBase+d::kTransmissionVtableRva+8*4,p::kImageBase+t::kEngineVtableRva+2*4,p::kImageBase+t::kEngineVtableRva+3*4}){
  setup();put(address,std::uint32_t{0});fails(t::Status::FingerprintMismatch);
 }++tests;
 for(auto address:{p::kImageBase+t::kCurrentRva,p::kImageBase+t::kMaxRva,p::kImageBase+t::kLimitRva,p::kImageBase+t::kCtorRva,p::kImageBase+t::kCtorLayoutRva}){
  setup();put(address,static_cast<unsigned char>(0xCC));fails(t::Status::FingerprintMismatch);
 }++tests;
 for(auto address:{transmission+0x34,engine+0x30,engine+0x34}){
  setup();memory.erase(address);fails(t::Status::ReadFailed);
 }++tests;
 for(float bad:{-1.0f,100001.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
  for(auto address:{transmission+0x34,engine+0x30,engine+0x34}){
   setup();put(address,bad);fails(t::Status::ValueRejected);
  }
 }++tests;
 for(auto address:{engine+0x30,engine+0x34}){setup();put(address,0.0f);fails(t::Status::ValueRejected);}++tests;
 for(int change=1;change<=5;++change){setup();mutation=change;fails(t::Status::ChangedDuringRead);}++tests;
 setup();put(transmission+0x34,100000.0f);assert(sample().status==t::Status::Ok);++tests;
 setup();put(transmission+0x34,-1.0f);x=sample();
 assert(x.status==t::Status::ValueRejected&&x.rpm==0&&x.raw.available&&x.raw.rejectedMask==1&&x.raw.current==-1&&x.raw.engine==engine);++tests;
 setup();put(engine+0x30,0.0f);x=sample();assert(x.raw.available&&x.raw.rejectedMask==2&&x.raw.maximum==0);++tests;
 setup();put(engine+0x34,std::numeric_limits<float>::quiet_NaN());x=sample();
 assert(x.raw.available&&x.raw.rejectedMask==4&&std::isnan(x.raw.limit)&&x.rpm==0);++tests;
 for(unsigned mask=0;mask<8;++mask){
  setup();if(mask&1)put(transmission+0x34,-1.0f);if(mask&2)put(engine+0x30,0.0f);if(mask&4)put(engine+0x34,0.0f);
  x=sample();assert(x.raw.available&&x.raw.rejectedMask==mask&&x.status==(mask?t::Status::ValueRejected:t::Status::Ok));
 }++tests;
 for(int change=1;change<=5;++change){setup();put(transmission+0x34,-1.0f);mutation=change;x=sample();
  assert(x.status==t::Status::ChangedDuringRead&&!x.raw.available);
 }++tests;
 assert(t::rawBits(-1.0f)==0xBF800000u&&t::rawBits(0.0f)==0);++tests;
 setup();put(p::kImageBase+s::kFlowRva,std::uint32_t{3});x=sample();assert(!x.raw.available);++tests;
 std::printf("PASS: %u read-only telemetry groups (candidate units NOT physically verified)\n",tests);
}
