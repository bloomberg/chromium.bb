# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'target_define%': 'TARGET_UNSUPPORTED',
    'conditions': [
      [ 'target_arch == "arm"', {
        'target_define': 'TARGET_ARM',
      }],
      [ 'target_arch == "arm64"', {
        'target_define': 'TARGET_ARM64',
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'lib_relocation_packer',
      'toolsets': ['host'],
      'type': 'static_library',
      'defines': [
        '<(target_define)',
      ],
      'dependencies': [
        '../../third_party/elfutils/elfutils.gyp:libelf',
      ],
      'sources': [
        'src/debug.cc',
        'src/elf_file.cc',
        'src/leb128.cc',
        'src/packer.cc',
        'src/run_length_encoder.cc',
      ],
    },
    {
      'target_name': 'relocation_packer',
      'toolsets': ['host'],
      'type': 'executable',
      'defines': [
        '<(target_define)',
      ],
      'dependencies': [
        '../../third_party/elfutils/elfutils.gyp:libelf',
        'lib_relocation_packer',
      ],
      'sources': [
        'src/main.cc',
      ],
    },
    {
      'target_name': 'relocation_packer_unittests',
      'toolsets': ['host'],
      'type': 'executable',
      'defines': [
        '<(target_define)',
      ],
      'cflags': [
        '-DINTERMEDIATE_DIR="<(INTERMEDIATE_DIR)"',
      ],
      'dependencies': [
        '../../testing/gtest.gyp:gtest',
        'lib_relocation_packer',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'src/debug_unittest.cc',
        'src/elf_file_unittest.cc',
        'src/leb128_unittest.cc',
        'src/packer_unittest.cc',
        'src/run_length_encoder_unittest.cc',
        'src/run_all_unittests.cc',
      ],
      'copies': [
        {
          'destination': '<(INTERMEDIATE_DIR)',
          'files': [
            'test_data/elf_file_unittest_relocs.so',
            'test_data/elf_file_unittest_relocs_packed.so',
          ],
        },
      ],
    },
  ],
}
