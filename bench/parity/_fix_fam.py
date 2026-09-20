import pathlib

# The family files land from the two feature agents; until then, keep the
# includes conditional so the TU builds with whatever exists.
p = pathlib.Path('src/vanilla_features.c')
s = p.read_text()
s = s.replace('''/* Feature-family implementations (included last so they see the whole driver). */
#include CINDER_INC_VANILLA_FEATURE_TERRAIN_C
#include CINDER_INC_VANILLA_FEATURE_VEGETATION_C''',
'''/* Feature-family implementations (included last so they see the whole driver).
 * __has_include keeps the TU buildable while a family file is being written. */
#if defined(__has_include)
#if __has_include(CINDER_INC_VANILLA_FEATURE_TERRAIN_C)
#include CINDER_INC_VANILLA_FEATURE_TERRAIN_C
#endif
#if __has_include(CINDER_INC_VANILLA_FEATURE_VEGETATION_C)
#include CINDER_INC_VANILLA_FEATURE_VEGETATION_C
#endif
#else
#include CINDER_INC_VANILLA_FEATURE_TERRAIN_C
#include CINDER_INC_VANILLA_FEATURE_VEGETATION_C
#endif''')
p.write_text(s)
print('conditional family includes')
