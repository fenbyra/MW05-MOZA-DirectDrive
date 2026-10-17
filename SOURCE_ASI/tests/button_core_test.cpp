#include "../examples/mw05_mirror_probe/button_core.h"
#include <cassert>
#include <cstdio>
int main() {
    mw05_buttons::PressGate g;
    assert(!g.observe(true,true,1)); // Held at startup: no edge.
    assert(!g.observe(true,true,11));
    assert(!g.observe(true,false,21));
    assert(g.observe(true,true,31));
    for (unsigned t=41;t<1000;t+=10) assert(!g.observe(true,true,t));
    assert(!g.observe(true,false,1000)); assert(g.observe(true,true,1010));
    assert(!g.observe(false,false,1020));
    assert(!g.observe(true,true,1030));
    assert(!g.observe(true,false,1040)); assert(g.observe(true,true,1050));
    g.reset(); assert(!g.observe(true,false,2000));
    assert(!g.observe(true,true,2200)); // stale release invalid.
    assert(!g.observe(true,false,2210)); assert(g.observe(true,true,2220));
    g.reset(); assert(!g.observe(true,false,0xFFFFFFF0u));
    assert(g.observe(true,true,0x00000004u)); // DWORD wrap.
    std::puts("PASS: button edge/release/focus/stale/wrap tests");
}
