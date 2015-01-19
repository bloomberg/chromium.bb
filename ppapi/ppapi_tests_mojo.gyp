# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'ppapi_nacl_test_common.gypi',
  ],
  'targets': [
    {
      'target_name': 'ppapi_tests_mojo',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_tests_mojo',
        # Only the pnacl toolchain can be used with mojo dependencies
        # currently.
        'build_newlib': 0,
        'build_glibc': 0,
        'build_pnacl_newlib': 1,
        # TODO(teravest): Build a translated nexe as well.
        'nexe_destination_dir': 'test_data/ppapi/tests/mojo',
        'sources': [
          'tests/mojo/test_mojo.cc',
          'tests/mojo/test_mojo.h',

          # Test support files.
          'tests/test_case.cc',
          'tests/test_case.h',
          'tests/test_utils.cc',
          'tests/test_utils.h',
          'tests/testing_instance.cc',
          'tests/testing_instance.h',
        ],
        'link_flags': [
          '-lmojo',
          '-limc_syscalls',
          '-lppapi_cpp',
          '-lppapi',
        ],
      },
      'dependencies': [
        '../mojo/mojo_nacl_untrusted.gyp:libmojo',
        '../mojo/mojo_nacl.gyp:monacl_codegen',
        '../native_client/src/untrusted/nacl/nacl.gyp:imc_syscalls_lib',
        '../third_party/mojo/mojo_public.gyp:mojo_system_placeholder',
        'native_client/native_client.gyp:ppapi_lib',
        'ppapi_nacl.gyp:ppapi_cpp_lib',
      ],
    },
  ],
}
