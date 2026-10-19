#include "../examples/mw05_mirror_probe/direction_core.h"
#include <cassert>
#include <cstdio>
#include <limits>
#include <map>

namespace dir = mw05_direction;
namespace p = mw05_probe;
namespace m = mw05_moza;
std::map<p::Address, unsigned char> memory;
template<class T> void put(p::Address a, const T& v) {
    const auto* data = reinterpret_cast<const unsigned char*>(&v);
    for (std::size_t i = 0; i < sizeof(v); ++i) memory[a+i] = data[i];
}
bool read(p::Address a, void* out, std::size_t n) {
    auto* data = static_cast<unsigned char*>(out);
    for (std::size_t i = 0; i < n; ++i) {
        const auto it = memory.find(a+i);
        if (it == memory.end()) return false;
        data[i] = it->second;
    }
    return true;
}
void setup() {
    memory.clear();
    const auto base = p::kImageBase;
    put(base+dir::kHelperRva, dir::kHelperEntry);
    put(base+dir::kHelperReturnRva, dir::kHelperReturn);
    put(base+dir::kFetchCallRva, dir::kFetchCall);
    put(base+dir::kReverseSwapRva, dir::kReverseSwap);
    put(base+dir::kSetGasRva, dir::kSetGasCode);
    put(base+dir::kSetBrakeRva, dir::kSetBrakeCode);
    put(base+p::kPlayerInputVtableRva+6*4, base+dir::kSetGasRva);
    put(base+p::kPlayerInputVtableRva+7*4, base+dir::kSetBrakeRva);
}
int main() {
    unsigned tests = 0;
    m::Axes command{}; command.gas=0.27f; command.brake=0.61f;
    for (int gear=0; gear<=9; ++gear) {
        const auto out=dir::map(command,gear);
        assert(out.valid && out.reverse==(gear==0));
        assert(out.gas==(gear==0 ? command.brake:command.gas));
        assert(out.brake==(gear==0 ? command.gas:command.brake));
    }
    ++tests;
    assert(!dir::map(command,-1).valid && !dir::map(command,10).valid);
    ++tests;
    for (float bad: {-1.0f,1.01f,std::numeric_limits<float>::quiet_NaN(),
                     std::numeric_limits<float>::infinity()}) {
        auto altered=command; altered.gas=bad;
        assert(!dir::map(altered,0).valid);
        altered=command; altered.brake=bad;
        assert(!dir::map(altered,2).valid);
    }
    ++tests;
    // Every raw pedal value still stays analog after mapping, in both directions.
    for (int raw=-32768; raw<=32767; ++raw) {
        auto sample=m::Axes{};
        sample.gas=m::pedal(static_cast<std::int16_t>(raw));
        sample.brake=m::pedal(static_cast<std::int16_t>(-1-raw));
        const auto forward=dir::map(sample,2), reverse=dir::map(sample,0);
        assert(forward.valid && reverse.valid);
        assert(forward.gas==reverse.brake && forward.brake==reverse.gas);
    }
    ++tests;
    setup(); assert(dir::fingerprint(p::kImageBase,&read)); ++tests;
    assert(!dir::fingerprint(p::kImageBase+0x1000,&read));
    assert(!dir::fingerprint(p::kImageBase,nullptr)); ++tests;
    const p::Address pieces[]={
        dir::kHelperRva,dir::kHelperReturnRva,dir::kFetchCallRva,dir::kReverseSwapRva,
        dir::kSetGasRva,dir::kSetBrakeRva
    };
    for (auto rva: pieces) {
        setup(); memory[p::kImageBase+rva]^=0x01;
        assert(!dir::fingerprint(p::kImageBase,&read));
    }
    ++tests;
    setup(); put(p::kImageBase+p::kPlayerInputVtableRva+6*4,std::uint32_t{0});
    assert(!dir::settersMatch(p::kImageBase,&read)); ++tests;
    setup(); put(p::kImageBase+p::kPlayerInputVtableRva+7*4,std::uint32_t{0});
    assert(!dir::settersMatch(p::kImageBase,&read)); ++tests;
    setup(); memory.erase(p::kImageBase+dir::kHelperRva);
    assert(!dir::fingerprint(p::kImageBase,&read)); ++tests;
    p::Player player{}; player.status=p::ResolveStatus::Unique;
    constexpr p::Address owner=0x0320F560;
    player.input=owner+dir::kOwnerOffset;
    put(owner,p::kImageBase+dir::kOwnerVtableRva);
    put(player.input,p::kImageBase+p::kPlayerInputVtableRva);
    assert(dir::ownerMatches(p::kImageBase,player,owner,&read)); ++tests;
    assert(!dir::ownerMatches(p::kImageBase,player,owner+4,&read));
    assert(!dir::ownerMatches(p::kImageBase,player,0xFFFFFFFC,&read));
    assert(!dir::ownerMatches(p::kImageBase,player,0,&read)); ++tests;
    auto nonunique=player; nonunique.status=p::ResolveStatus::Ambiguous;
    assert(!dir::ownerMatches(p::kImageBase,nonunique,owner,&read)); ++tests;
    put(owner,std::uint32_t{0});
    assert(!dir::ownerMatches(p::kImageBase,player,owner,&read)); ++tests;
    std::printf("PASS: %u direction core groups; analog mapping exhaustive over 65,536 raw values\n",tests);
}