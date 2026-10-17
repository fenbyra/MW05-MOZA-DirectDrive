#include "../examples/mw05_mirror_probe/menu_core.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
int main() {
    namespace m=mw05_menu;
    mw05_session_gate::Snapshot main{true,3,0x800},race{true,6,0},pause{true,6,2},load{true,4,0};
    assert(m::allowed(main)&&m::allowed(pause)&&!m::allowed(race)&&!m::allowed(load));
    assert(!m::allowed({false,3,0}));
    for(unsigned bit=0;bit<32;++bit) assert(m::allowed({true,6,1u<<bit})==(bit==1||bit==4));
    // Explicit race-dialog state, reject combined/unknown states.
    const mw05_session_gate::Snapshot dialog{true,6,0x10};
    assert(m::allowed(dialog));
    for (unsigned flags=0;flags<0x40000;++flags)
        assert(m::allowed({true,6,flags})==(flags==2 || flags==0x10));
    for(unsigned raw=0;raw<256;++raw) {
        m::Sample s{true,100,static_cast<unsigned char>(raw)};
        assert(m::merge(0x100,0,s,100,dialog)==(0x100|m::directionMask(static_cast<unsigned char>(raw))));
        assert(m::merge(0x100,1,s,100,dialog)==0x100);
        assert(m::merge(0x100,0,s,251,dialog)==0x100);
    }
    m::ReleaseGate gate;assert(!gate.observe(true,1));
    assert(!gate.observe(true,0));assert(gate.observe(true,1)==1);
    assert(!gate.observe(false,0));assert(!gate.observe(true,1));assert(!gate.observe(true,0));
    for(unsigned raw=1;raw<256;++raw) assert(gate.observe(true,static_cast<unsigned char>(raw))==((raw==1||raw==2||raw==4||raw==8)?raw:0));
    for(unsigned player=0;player<256;++player) for(unsigned raw=0;raw<256;++raw) {
        m::Sample sample{true,100,static_cast<unsigned char>(raw)};
        unsigned add=raw==1?1:raw==2?8:raw==4?2:raw==8?4:0;
        for(auto native:{0u,0x10u,0xFFFF0000u,0xFFFFFFFFu}) {
            assert(m::merge(native,static_cast<unsigned char>(player),sample,100,main)==(native|(player==0?add:0)));
            assert(m::merge(native,static_cast<unsigned char>(player),sample,100,race)==native);
            assert(m::merge(native,static_cast<unsigned char>(player),sample,251,main)==native);
        }
    }
    assert(m::merge(0,0,{true,0xFFFFFFF0u,1},4,main)==1);
    assert(m::merge(0,0,{false,1,1},1,main)==0);
    assert(m::merge(0,0,{true,1,1},151,main)==1);
    assert(m::merge(0,0,{true,1,1},152,main)==0);
    std::puts("PASS: UI mask all256players/all256rawstates/native preservation/context/release/stale/wrap");
}
