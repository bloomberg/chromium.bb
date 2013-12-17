# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  # Default value for all libraries.
  'custom_configure_flags': '',
  
  'variables': {
    'verbose_libraries_build%': 0,
  },
  'conditions': [
    ['asan==1', {
      'sanitizer_type': 'asan',
    }],
    ['msan==1', {
      'sanitizer_type': 'msan',
    }],
    ['verbose_libraries_build==1', {
      'verbose_libraries_build_flag': '--verbose',
    }, {
      'verbose_libraries_build_flag': '',
    }],
  ],
  'targets': [
    {
      'target_name': 'instrumented_libraries',
      'type': 'none',
      'variables': {
         'prune_self_dependency': 1,
      },
      'dependencies': [
        '<(_sanitizer_type)-libpng12-0',
        '<(_sanitizer_type)-libxau6',
        '<(_sanitizer_type)-libxdmcp6',
        '<(_sanitizer_type)-libx11-6',
        '<(_sanitizer_type)-libxcb1',
        '<(_sanitizer_type)-libxext6',
        '<(_sanitizer_type)-libxi6',
        '<(_sanitizer_type)-libxrandr2',
        '<(_sanitizer_type)-libxrender1',
        '<(_sanitizer_type)-libxtst6',
        '<(_sanitizer_type)-libpixman-1-0',
        '<(_sanitizer_type)-libp11-kit0',
        '<(_sanitizer_type)-libgpg-error0',
        '<(_sanitizer_type)-libexpat1',
        '<(_sanitizer_type)-libffi6',
        '<(_sanitizer_type)-libcairo2',
        '<(_sanitizer_type)-libpcre3',
      ],
      'conditions': [
        ['asan==1', {
          'dependencies': [
            '<(_sanitizer_type)-libfontconfig1',
            '<(_sanitizer_type)-libglib2.0-0',
          ],
        }],
      ],
      'actions': [
        {
          'action_name': 'fix_rpaths',
          'inputs': [
            'fix_rpaths.sh',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/instrumented_libraries/<(_sanitizer_type)/rpaths.fixed.txt',
          ],
          'action': [
            '<(DEPTH)/third_party/instrumented_libraries/fix_rpaths.sh',
            '<(PRODUCT_DIR)/instrumented_libraries/<(_sanitizer_type)'
          ],
        },
      ],
    },
    {
      'library_name': 'libpng12-0',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libpixman-1-0',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libp11-kit0',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libgpg-error0',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libexpat1',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libffi6',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libfontconfig1',
      'dependencies=': [],
      'custom_configure_flags': '--disable-docs',
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libcairo2',
      'dependencies=': [],
      'custom_configure_flags': '--disable-gtk-doc',
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libpcre3',
      'dependencies=': [],
      'custom_configure_flags': [
        '--enable-utf8',
        '--enable-unicode-properties',
      ],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libxau6',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libglib2.0-0',
      'dependencies=': [],
      'custom_configure_flags': [
        '--disable-gtk-doc',
        '--disable-gtk-doc-html',
        '--disable-gtk-doc-pdf',
      ],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libxdmcp6',
      'dependencies=': [],
      'custom_configure_flags': '--disable-docs',
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libx11-6',
      'dependencies=': [],
      'custom_configure_flags': '--disable-specs',
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libxcb1',
      'dependencies=': [],
      'custom_configure_flags': '--disable-build-docs',
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libxext6',
      'dependencies=': [],
      'custom_configure_flags': '--disable-specs',
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libxi6',
      'dependencies=': [],
      'custom_configure_flags': [
        '--disable-specs',
        '--disable-docs',
      ],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libxrandr2',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libxrender1',
      'dependencies=': [],
      'includes': ['standard_instrumented_library_target.gypi'],
    },
    {
      'library_name': 'libxtst6',
      'dependencies=': [],
      'custom_configure_flags': '--disable-specs',
      'includes': ['standard_instrumented_library_target.gypi'],
    },
  ],
}
