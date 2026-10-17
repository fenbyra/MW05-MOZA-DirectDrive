#include "../examples/mw05_mirror_probe/moza_bridge_api.h"
#include <stddef.h>
_Static_assert(sizeof(MW05MozaSample) == 28, "C API packet size");
_Static_assert(offsetof(MW05MozaSample, angleDegrees) == 16, "C API angle offset");
_Static_assert(offsetof(MW05MozaSample, throttle) == 22, "C API gas offset");
_Static_assert(offsetof(MW05MozaSample, brake) == 24, "C API brake offset");
int main(void) { return 0; }
