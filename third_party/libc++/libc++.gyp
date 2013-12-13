# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'libc++',
      'type': 'shared_library',
      'variables': {
         'prune_self_dependency': 1,
      },
      'dependencies=': [],
      'sources': [
        'src/algorithm.cpp',
        'src/bind.cpp',
        'src/chrono.cpp',
        'src/condition_variable.cpp',
        'src/debug.cpp',
        'src/exception.cpp',
        'src/future.cpp',
        'src/hash.cpp',
        'src/ios.cpp',
        'src/iostream.cpp',
        'src/locale.cpp',
        'src/memory.cpp',
        'src/mutex.cpp',
        'src/new.cpp',
        'src/optional.cpp',
        'src/random.cpp',
        'src/regex.cpp',
        'src/shared_mutex.cpp',
        'src/stdexcept.cpp',
        'src/string.cpp',
        'src/strstream.cpp',
        'src/system_error.cpp',
        'src/thread.cpp',
        'src/typeinfo.cpp',
        'src/utility.cpp',
        'src/valarray.cpp',
      ],
      'include_dirs': [
        'include',
        '../libc++abi/include',
      ],
      'cflags': [
        '-g', '-Os', '-fPIC',
        '-std=c++11',
        '-fstrict-aliasing',
        '-Wall',
        '-Wextra',
        '-Wshadow',
        '-Wconversion',
        '-Wnewline-eof',
        '-Wpadded',
        '-Wmissing-prototypes',
        '-Wstrict-aliasing=2',
        '-Wstrict-overflow=4',
        '-nostdinc++',
      ],
      'direct_dependent_settings': {
        'target_conditions': [
          ['_type!="none"', {
            'include_dirs': [
              'include',
            ],
            'cflags_cc': [
              '-nostdinc++',
            ],
            'ldflags': [
              '-stdlib=libc++',
              '-L<(PRODUCT_DIR)/lib/',
            ],
          }],
        ],
      },
      'cflags_cc!': [
        '-fno-rtti',
      ],
      'cflags!': [
        '-fno-exceptions',
        '-fvisibility=hidden',
      ],
      'ldflags': [
        '-nodefaultlibs',
      ],
      'ldflags!': [
        '-pthread',
      ],
      'libraries': [
        '-lrt',
        '-lc',
      ],
    },
  ]
}
