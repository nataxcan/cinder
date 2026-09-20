import pathlib

p = pathlib.Path('src/vanilla_gen.c')
s = p.read_text()

old = '''#ifndef CINDER_INC_VANILLA_FEATURES_C
#define CINDER_INC_VANILLA_FEATURES_C "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_features.c"
#endif
#include CINDER_INC_VANILLA_FEATURES_C
#ifndef CINDER_INC_VANILLA_PLACEMENTS_C
#define CINDER_INC_VANILLA_PLACEMENTS_C /mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_placements.c
#endif
#ifndef CINDER_INC_VANILLA_FEATURE_HELPERS_C
#define CINDER_INC_VANILLA_FEATURE_HELPERS_C /mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_feature_helpers.c
#endif
#include CINDER_INC_VANILLA_PLACEMENTS_C
#include CINDER_INC_VANILLA_FEATURE_HELPERS_C'''

new = '''#ifndef CINDER_INC_VANILLA_FEATURE_HELPERS_C
#define CINDER_INC_VANILLA_FEATURE_HELPERS_C "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_feature_helpers.c"
#endif
#ifndef CINDER_INC_VANILLA_PLACEMENTS_C
#define CINDER_INC_VANILLA_PLACEMENTS_C "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_placements.c"
#endif
#ifndef CINDER_INC_VANILLA_FEATURES_C
#define CINDER_INC_VANILLA_FEATURES_C "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_features.c"
#endif
/* helpers first (the placement layer uses them), then the placement chain, then
 * the driver with the feature families at its end */
#include CINDER_INC_VANILLA_FEATURE_HELPERS_C
#include CINDER_INC_VANILLA_PLACEMENTS_C
#include CINDER_INC_VANILLA_FEATURES_C'''

assert old in s, "include block not found"
s = s.replace(old, new, 1)
p.write_text(s)
print("include order fixed")
