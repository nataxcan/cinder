import pathlib

root = pathlib.Path('src')

# 1) feature-common header needs its own include guard (it is included from
#    several files with a path macro, which does not guard the content)
p = root / 'vanilla_feature_common.h'
s = p.read_text()
if '#pragma once' not in s:
    s = s.replace('// Feature stage:',
                  '#pragma once\n\n// Feature stage:', 1)
p.write_text(s)
print('feature header guarded')

# 2) helpers: define the tags path macro, fix B_MOSS_BLOCK -> B_MOSS
p = root / 'vanilla_feature_helpers.c'
s = p.read_text()
s = s.replace('#include CINDER_INC_VANILLA_BLOCK_TAGS_H',
              '''#ifndef CINDER_INC_VANILLA_BLOCK_TAGS_H
#define CINDER_INC_VANILLA_BLOCK_TAGS_H "/mnt/c/Users/nataxcan/Documents/dev/cinder/src/vanilla_block_tags.h"
#endif
#include CINDER_INC_VANILLA_BLOCK_TAGS_H''')
s = s.replace('B_MOSS_BLOCK', 'B_MOSS')
p.write_text(s)
print('helpers fixed')
