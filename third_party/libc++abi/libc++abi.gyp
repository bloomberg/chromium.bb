# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'libc++abi',
      'type': 'shared_library',
      'variables': {
         'prune_self_dependency': 1,
      },
      'dependencies=': [],
      'sources': [
        'src/abort_message.cpp',
        'src/cxa_aux_runtime.cpp',
        'src/cxa_default_handlers.cpp',
        'src/cxa_demangle.cpp',
        'src/cxa_exception.cpp',
        'src/cxa_exception_storage.cpp',
        'src/cxa_guard.cpp',
        'src/cxa_handlers.cpp',
        'src/cxa_new_delete.cpp',
        'src/cxa_personality.cpp',
        'src/cxa_unexpected.cpp',
        'src/cxa_vector.cpp',
        'src/cxa_virtual.cpp',
        'src/exception.cpp',
        'src/private_typeinfo.cpp',
        'src/stdexcept.cpp',
        'src/typeinfo.cpp',
      ],
      'include_dirs': [
        'include',
        '../libc++/include'
      ],
      'cflags': [
        '-g', '-O3', '-fPIC',
        '-std=c++11',
        '-fstrict-aliasing',
        '-Wsign-conversion',
        '-Wshadow',
        '-Wconversion',
        '-Wunused-variable',
        '-Wmissing-field-initializers',
        '-Wchar-subscripts',
        '-Wmismatched-tags',
        '-Wmissing-braces',
        '-Wshorten-64-to-32',
        '-Wsign-compare',
        '-Wstrict-aliasing=2',
        '-Wstrict-overflow=4',
        '-Wunused-parameter',
        '-Wnewline-eof',
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
      ]
    },
  ]
}
