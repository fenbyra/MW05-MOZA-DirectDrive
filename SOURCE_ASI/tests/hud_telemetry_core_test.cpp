#include "../examples/mw05_mirror_probe/hud_telemetry_core.h"
#include <cassert>
#include <cstdio>
#include <map>
#include <limits>
namespace p=mw05_probe;namespace h=mw05_hud;namespace s=mw05_session_gate;
namespace {
std::map<p::Address,unsigned char> mem;
constexpr p::Address car=0x3203870,input=0x320F5A8,widget=0x11243880;
int mutation=0;
template<class T>void put(p::Address a,const T& v){const auto*b=reinterpret_cast<const unsigned char*>(&v);for(unsigned i=0;i<sizeof(v);++i)mem[a+i]=b[i];}
bool read(p::Address a,void*out,std::size_t n){auto*b=static_cast<unsigned char*>(out);for(unsigned i=0;i<n;++i){auto it=mem.find(a+i);if(it==mem.end())return false;b[i]=it->second;}
 if(a==widget+0x24){
  if(mutation==1)put(widget,std::uint32_t{0});
  if(mutation==2)put(widget+0x30,std::uint32_t{0});
  if(mutation==3)put(p::kImageBase+s::kSimFlagsRva,std::uint32_t{2});
  if(mutation==5)put(input,p::kImageBase+p::kPlayerInputVtableRva);
  if(mutation==4)put(p::kImageBase+p::kVehicleListRva,p::VehicleEntry{});
 }return true;}
p::Player player(){return {p::ResolveStatus::Unique,car,input,input+8};}
s::Snapshot context(){return {true,6,0};}
void setup(){mem.clear();mutation=0;
 put(p::kImageBase+p::kVehicleContainerRva,p::VehicleContainer{p::kImageBase+p::kVehicleContainerVtableRva,p::kImageBase+p::kVehicleListRva,p::kMaxVehicles,1});
 put(p::kImageBase+p::kVehicleListRva,p::VehicleEntry{car,1,{0,0,0}});put(p::kImageBase+p::kVehicleListRva+8,p::VehicleEntry{});
 put(car,p::kImageBase+p::kVehicleVtableRva);put(car+p::kVehicleInputOffset,input);put(input,p::kImageBase+p::kPlayerInputVtableRva);
 put(p::kImageBase+s::kFlowRva,std::uint32_t{6});put(p::kImageBase+s::kSimFlagsRva,std::uint32_t{0});
 put(widget,p::kImageBase+h::kVtableRva);put(widget+0x30,p::kImageBase+h::kSecondaryRva);
 put(widget+0x54,800.0f);put(widget+0x58,6400.0f);put(widget+0x5C,7400.0f);put(widget+0x60,2);
 put(widget+0x18,std::uint32_t{2});put(widget+0x1C,std::uint32_t{0});put(widget+0x20,std::uint32_t{2});put(widget+0x24,std::uint32_t{0});
 put(p::kImageBase+h::kTickRva,h::kEntry);put(p::kImageBase+0x17A8B7,h::kReturn);put(p::kImageBase+0x17A8F2,h::kReturn);
 put(p::kImageBase+h::kVtableRva,h::kVtable);put(p::kImageBase+h::kSecondaryRva,h::kSecondary);
 put(p::kImageBase+0x17A708,h::kRatio);put(p::kImageBase+0x17A75B,h::kThreshold);
 put(p::kImageBase+0x19EA1E,h::kLayout);put(p::kImageBase+0x19EA41,h::kCtor);
}
h::Snapshot sample(){return h::sample(p::kImageBase,player(),widget,&read);}
void fail(h::Status status){const auto v=sample();assert(v.status==status&&v.current==0&&v.widget==0&&v.gear==-1);}
}
int main(){unsigned tests=0;
 setup();const auto before=mem;const auto good=sample();assert(good.status==h::Status::Ok&&good.current==800&&good.threshold==6400&&good.scale==7400&&good.active&&mem==before);++tests;
 assert(h::fingerprint(p::kImageBase,&read));put(p::kImageBase+h::kTickRva,std::uint8_t{0xE9});assert(!h::fingerprint(p::kImageBase,&read));++tests;
 setup();assert(h::sample(p::kImageBase,p::Player{},widget,&read).status==h::Status::NoPlayer);++tests;
 setup();assert(h::sample(p::kImageBase,player(),widget,nullptr).status==h::Status::LayoutMismatch);++tests;
 setup();assert(h::sample(0x500000,player(),widget,&read).status==h::Status::LayoutMismatch);++tests;
 setup();put(p::kImageBase+s::kFlowRva,std::uint32_t{3});fail(h::Status::ContextBlocked);++tests;
 setup();put(p::kImageBase+s::kSimFlagsRva,std::uint32_t{2});fail(h::Status::ContextBlocked);++tests;
 for(auto off:{0u,0x30u}){setup();put(widget+off,std::uint32_t{0});fail(h::Status::LayoutMismatch);}++tests;
 for(auto off:{0u,0x30u,0x54u,0x58u,0x5Cu,0x60u,0x18u,0x1Cu,0x20u,0x24u}){setup();mem.erase(widget+off);fail(h::Status::ReadFailed);}++tests;
 for(float bad:{-1.f,100001.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
  for(auto off:{0x54u,0x58u,0x5Cu}){setup();put(widget+off,bad);fail(h::Status::ValueRejected);}
 }++tests;
 for(auto off:{0x58u,0x5Cu}){setup();put(widget+off,0.f);fail(h::Status::ValueRejected);}++tests;
 setup();put(widget+0x54,0.f);assert(sample().status==h::Status::Ok);++tests;
 for(int gear:{-1,10}){setup();put(widget+0x60,gear);fail(h::Status::ValueRejected);}++tests;
 setup();put(widget+0x18,std::uint32_t{0});fail(h::Status::Inactive);++tests;
 for(int i=1;i<=4;++i){setup();mutation=i;fail(h::Status::ChangedDuringRead);}++tests;
 setup();assert(h::sample(p::kImageBase,player(),0xFFFFFFF0,&read).status==h::Status::LayoutMismatch);++tests;
 h::Cache cache{};assert(h::select(cache,1000,player(),context(),true).status==h::Status::NoSample);++tests;
 h::publish(cache,good,1000);assert(h::select(cache,1500,player(),context(),true).status==h::Status::Ok);assert(h::select(cache,1501,player(),context(),true).status==h::Status::Stale);++tests;
 auto changed=player();changed.input+=0x100;assert(h::select(cache,1100,changed,context(),true).status==h::Status::OwnerChanged);++tests;
 assert(h::select(cache,1100,player(),context(),false).status==h::Status::ContextBlocked);assert(h::select(cache,1100,player(),{true,6,2},true).status==h::Status::ContextBlocked);++tests;
 auto other=good;other.widget+=0x100;h::publish(cache,other,1200);assert(cache.latest.status==h::Status::Ambiguous);h::publish(cache,other,1300);assert(cache.latest.status==h::Status::Ambiguous);++tests;
 h::publish(cache,good,1901);assert(cache.latest.status==h::Status::Ok);++tests;
 other=good;other.vehicle+=0x100;h::publish(cache,other,1950);assert(cache.latest.status==h::Status::Ok&&!cache.ambiguous);++tests;
 h::publish(cache,h::failure(h::Status::ReadFailed),2000);assert(h::select(cache,2050,player(),context(),true).status==h::Status::ReadFailed);++tests;
 cache=h::Cache{};h::publish(cache,good,0xFFFFFFF0u);assert(h::select(cache,0x20,player(),context(),true).status==h::Status::Ok);++tests;
 // Drag uses the same validated RPM fields but a distinct widget class and player type.
 auto dragSetup=[](){setup();put(input,p::kImageBase+p::kDragInputVtableRva);
   put(widget,p::kImageBase+h::kDragVtableRva);put(widget+0x30,p::kImageBase+h::kDragSecondaryRva);};
 auto dragSample=[](){return h::sample(p::kImageBase,p::resolvePlayer(p::kImageBase,&read),widget,&read,h::Source::Drag);};
 dragSetup();const auto dragBefore=mem;const auto dragGood=dragSample();
 assert(dragGood.status==h::Status::Ok && dragGood.current==800 && dragGood.inputVtable==p::kImageBase+p::kDragInputVtableRva && mem==dragBefore);++tests;
 for(auto off:{0u,0x30u}){dragSetup();put(widget+off,std::uint32_t{0});assert(dragSample().status==h::Status::LayoutMismatch);}++tests;
 for(auto off:{0x54u,0x58u,0x5Cu,0x60u,0x18u,0x1Cu,0x20u,0x24u}){dragSetup();mem.erase(widget+off);assert(dragSample().status==h::Status::ReadFailed);}++tests;
 for(float bad:{-1.f,100001.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
   for(auto off:{0x54u,0x58u,0x5Cu}){dragSetup();put(widget+off,bad);assert(dragSample().status==h::Status::ValueRejected);}
 }++tests;
 for(auto off:{0x58u,0x5Cu}){dragSetup();put(widget+off,0.f);assert(dragSample().status==h::Status::ValueRejected);}++tests;
 for(int gear:{-1,10}){dragSetup();put(widget+0x60,gear);assert(dragSample().status==h::Status::ValueRejected);}++tests;
 dragSetup();put(widget+0x18,std::uint32_t{0});assert(dragSample().status==h::Status::Inactive);++tests;
 for(int change=1;change<=5;++change){dragSetup();mutation=change;assert(dragSample().status==h::Status::ChangedDuringRead);}++tests;
 dragSetup();put(input,p::kImageBase+p::kPlayerInputVtableRva);
 assert(dragSample().status==h::Status::ChangedDuringRead);++tests;
 cache=h::Cache{};h::publish(cache,dragGood,1000);auto normal=player();normal.inputVtable=p::kImageBase+p::kPlayerInputVtableRva;
 assert(h::select(cache,1000,normal,context(),true).status==h::Status::OwnerChanged);++tests;
 auto typed=good;typed.widget=dragGood.widget+0x100;h::publish(cache,typed,1001);
 assert(!cache.ambiguous && cache.latest.status==h::Status::Ok);++tests;
 std::printf("PASS: %u read-only HUD core groups\n",tests);
}
