#include "../examples/mw05_mirror_probe/udp_transport.h"
#include "../examples/mw05_mirror_probe/udp_telemetry_core.h"
#include <cassert>
#include <cstdio>
#include <limits>
namespace u=mw05_udp;namespace h=mw05_hud;
std::uint32_t read32(const u::Packet& b,unsigned o){return std::uint32_t(b[o])|(std::uint32_t(b[o+1])<<8)|(std::uint32_t(b[o+2])<<16)|(std::uint32_t(b[o+3])<<24);}
float readf(const u::Packet& b,unsigned o){auto n=read32(b,o);float v=0;std::memcpy(&v,&n,4);return v;}
int main(){
    unsigned tests=0;
    h::Snapshot v{};v.status=h::Status::Ok;v.active=true;v.vehicle=0x3203870;v.input=0x320f5a8;
    v.current=4800;v.scale=7400;v.threshold=6400;v.gear=2;
    auto b=u::encode(v,0xF1234567u,true);
    assert(b.size()==324 && read32(b,0)==1 && read32(b,4)==0xF1234567u);
    assert(b[4]==0x67 && b[5]==0x45 && b[6]==0x23 && b[7]==0xF1);++tests;
    assert(readf(b,8)==7400 && readf(b,12)==0 && readf(b,16)==4800);++tests;
    for(unsigned i=20;i<324;++i){assert(b[i]==0);}++tests;
    assert(b[8]==0 && b[9]==0x40 && b[10]==0xE7 && b[11]==0x45);
    assert(b[16]==0 && b[17]==0 && b[18]==0x96 && b[19]==0x45);++tests;
    b=u::encode(v,0xFFFFFFFFu,false);assert(read32(b,0)==0 && read32(b,4)==0xFFFFFFFFu);
    for(unsigned i=8;i<324;++i){assert(b[i]==0);}++tests;
    for(unsigned status=0;status<=12;++status){
        auto bad=v;bad.status=static_cast<h::Status>(status);
        assert(u::valid(bad)==(status==1));
        const auto packet=u::encode(bad,0,true);assert(read32(packet,0)==(status==1?1u:0u));
    }++tests;
    for(float x:{-1.0f,100001.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        auto bad=v;bad.current=x;assert(!u::valid(bad));bad=v;bad.scale=x;assert(!u::valid(bad));
    }++tests;
    auto bad=v;bad.scale=0;assert(!u::valid(bad));bad=v;bad.active=false;assert(!u::valid(bad));
    bad=v;bad.vehicle=0;assert(!u::valid(bad));bad=v;bad.input=0;assert(!u::valid(bad));++tests;
    for(float x:{0.0f,800.0f,100000.0f}){auto edge=v;edge.current=x;assert(u::valid(edge));}++tests;
    u::Stream state{};bad=h::failure(h::Status::Stale);
    assert(u::step(state,bad)==u::Action::None);assert(u::step(state,v)==u::Action::Live);++tests;
    for(unsigned i=0;i<3;++i)assert(u::step(state,bad)==u::Action::Clear);
    assert(u::step(state,bad)==u::Action::None && !state.live);++tests;
    assert(u::step(state,v)==u::Action::Live);assert(u::step(state,bad)==u::Action::Clear);
    assert(u::step(state,v)==u::Action::Live && state.clears==0);++tests;
    auto changed=v;changed.vehicle+=0x100;assert(u::step(state,changed)==u::Action::Live && state.vehicle==changed.vehicle);++tests;
    using T=u::Transport;
    qa_udp::reset();{T t;assert(t.open(20066));assert(qa_udp::ioctls==1);
        assert(t.send(b.data(),b.size()));assert(qa_udp::destination.sin_addr.s_addr==htonl(0x7f000001));
        assert(qa_udp::destination.sin_port==htons(20066));assert(qa_udp::payload.size()==324);}
    assert(qa_udp::cleanups==1 && qa_udp::closes==1);++tests;
    qa_udp::reset();{T t;assert(!t.open(0) && t.error==WSAEINVAL);assert(!t.open(65536));}
    assert(qa_udp::startups==0);++tests;
    qa_udp::reset();qa_udp::startupResult=123;{T t;assert(!t.open(20066) && t.error==123);}assert(qa_udp::cleanups==0);++tests;
    qa_udp::reset();qa_udp::badVersion=true;{T t;assert(!t.open(20066) && t.error==WSAVERNOTSUPPORTED);}assert(qa_udp::cleanups==1);++tests;
    qa_udp::reset();qa_udp::badSocket=true;{T t;assert(!t.open(20066) && t.error==10035);}assert(qa_udp::cleanups==1 && qa_udp::closes==0);++tests;
    qa_udp::reset();qa_udp::ioctlResult=-1;{T t;assert(!t.open(20066) && t.error==10035);}assert(qa_udp::cleanups==1 && qa_udp::closes==1);++tests;
    qa_udp::reset();{T t;assert(t.open(20066));qa_udp::sendResult=-1;assert(!t.send(b.data(),b.size()) && t.error==10035);
        qa_udp::sendResult=1;assert(!t.send(b.data(),b.size()) && t.error==WSAEMSGSIZE);
        qa_udp::sendResult=324;assert(t.send(b.data(),b.size()) && t.error==0);
        assert(t.open(20055));assert(qa_udp::closes==1 && qa_udp::cleanups==1);}
    assert(qa_udp::closes==2 && qa_udp::cleanups==2);++tests;
    qa_udp::reset();{T t;assert(!t.send(b.data(),b.size()) && t.error==WSAENOTSOCK);}++tests;
    std::printf("PASS: %u UDP packet/stream/transport mock groups (not Pit House acceptance)\n",tests);
}
