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
        'trunk/src/abort_message.cpp',
        'trunk/src/cxa_aux_runtime.cpp',
        'trunk/src/cxa_default_handlers.cpp',
        'trunk/src/cxa_demangle.cpp',
        'trunk/src/cxa_exception.cpp',
        'trunk/src/cxa_exception_storage.cpp',
        'trunk/src/cxa_guard.cpp',
        'trunk/src/cxa_handlers.cpp',
        'trunk/src/cxa_new_delete.cpp',
        'trunk/src/cxa_personality.cpp',
        'trunk/src/cxa_unexpected.cpp',
        'trunk/src/cxa_vector.cpp',
        'trunk/src/cxa_virtual.cpp',
        'trunk/src/exception.cpp',
        'trunk/src/private_typeinfo.cpp',
        'trunk/src/stdexcept.cpp',
        'trunk/src/typeinfo.cpp',
      ],
      'include_dirs': [
        'trunk/include',
        '../libc++/trunk/include',
        # TODO(earthdok): remove when http://crbug.com/337426 is fixed
        '../llvm-build/Release+Asserts/lib/clang/3.5/include/'
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
              'trunk/include',
              # TODO(earthdok): remove when http://crbug.com/337426 is fixed
              '../llvm-build/Release+Asserts/lib/clang/3.5/include/'
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
