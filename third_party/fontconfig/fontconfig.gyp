# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'fontconfig',
      'type': '<(component)',
      'dependencies' : [
        '../zlib/zlib.gyp:zlib',
        '../../build/linux/system.gyp:freetype2',
        '../libxml/libxml.gyp:libxml',
      ],
      'defines': [
          'HAVE_CONFIG_H',
          'FC_CACHEDIR="/var/cache/fontconfig"',
          'FONTCONFIG_PATH="/etc/fonts"',
      ],
      'sources': [
        'src/fcarch.h',
        'src/fcatomic.c',
        'src/fcblanks.c',
        'src/fccache.c',
        'src/fccfg.c',
        'src/fccharset.c',
        'src/fccompat.c',
        'src/fcdbg.c',
        'src/fcdefault.c',
        'src/fcdir.c',
        'src/fcformat.c',
        'src/fcfreetype.c',
        'src/fcfs.c',
        'src/fchash.c',
        'src/fcinit.c',
        'src/fclang.c',
        'src/fclist.c',
        'src/fcmatch.c',
        'src/fcmatrix.c',
        'src/fcname.c',
        'src/fcobjs.c',
        'src/fcpat.c',
        'src/fcserialize.c',
        'src/fcstat.c',
        'src/fcstr.c',
        'src/fcxml.c',
        'src/ftglue.h',
        'src/ftglue.c',
      ],
      'include_dirs': [
        'src',
        'include',
        'include/src',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
           'src',
        ],
      },
    },
  ],
}
