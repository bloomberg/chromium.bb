# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This GYP file defines untrusted (NaCl) targets.  All targets in this
# file should be conditionally depended upon via 'disable_nacl!=1' to avoid
# requiring NaCl sources for building.

{
  'includes': [
    '../native_client/build/untrusted.gypi',
    'ppapi_sources.gypi',
  ],
  'targets': [
    {
      'target_name': 'ppapi_cpp_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libppapi_cpp.a',
        'build_glibc': 1,
        'build_newlib': 1,
        'sources': [
          '<@(cpp_source_files)',
          'cpp/module_embedder.h',
          'cpp/ppp_entrypoints.cc',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
    {
      'target_name': 'ppapi_gles2_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libppapi_gles2.a',
        'build_glibc': 1,
        'build_newlib': 1,
        'include_dirs': [
          'lib/gl/include',
        ],
        'sources': [
          'lib/gl/gles2/gl2ext_ppapi.c',
          'lib/gl/gles2/gl2ext_ppapi.h',
          'lib/gl/gles2/gles2.c',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
    {
      'target_name': 'ppapi_nacl_tests',
      'type': 'none',
      'dependencies': [
         'ppapi_cpp_lib',
         'native_client/native_client.gyp:ppapi_lib',
      ],
      'variables': {
        # TODO(bradnelson): Remove this compile flag once new nacl_rev is
        # above 9362.
        'compile_flags': [
          '-DGL_GLEXT_PROTOTYPES',
        ],
        'defines': [
          'GL_GLEXT_PROTOTYPES',
        ],
        'nexe_target': 'ppapi_nacl_tests',
        'build_newlib': 1,
        'include_dirs': [
          'lib/gl/include',
          '..',
        ],
        'link_flags': [
          '-lppapi_cpp',
          '-lppapi',
          '-lpthread',
        ],
        # TODO(bradchen): get rid of extra_deps64 and extra_deps32
        # once native_client/build/untrusted.gypi no longer needs them.
        'extra_deps64': [
          '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libppapi_cpp.a',
          '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libppapi.a',
        ],
        'extra_deps_newlib64': [
          '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libppapi_cpp.a',
          '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib64/libppapi.a',
        ],
        'extra_deps_newlib32': [
          '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libppapi_cpp.a',
          '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/lib32/libppapi.a',
        ],
        'extra_deps_glibc64': [
          '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libppapi_cpp.a',
          '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib64/libppapi.a',
        ],
        'extra_deps_glibc32': [
          '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libppapi_cpp.a',
          '<(SHARED_INTERMEDIATE_DIR)/tc_glibc/lib32/libppapi.a',
        ],
        'extra_deps_arm': [
          '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm/libppapi_cpp.a',
          '<(SHARED_INTERMEDIATE_DIR)/tc_newlib/libarm/libppapi.a',
        ],
        'sources': [
          '<@(test_common_source_files)',
          '<@(test_nacl_source_files)',
        ],
      },
      'conditions': [
        ['target_arch!="arm"', {
          'variables': {
            'compile_flags': [
              '-mno-tls-use-call',
	    ],
          },
        }],
        ['target_arch!="arm" and disable_glibc==0', {
          'variables': {
            'build_glibc': 1,
            # NOTE: Use /lib, not /lib64 here; it is a symbolic link which
            # doesn't work on Windows.
            'libdir_glibc64': '>(nacl_glibc_tc_root)/x86_64-nacl/lib',
            'libdir_glibc32': '>(nacl_glibc_tc_root)/x86_64-nacl/lib32',
            'nacl_objdump': '>(nacl_glibc_tc_root)/bin/x86_64-nacl-objdump',
            'nmf_glibc%': '<(PRODUCT_DIR)/>(nexe_target)_glibc.nmf',
          },
          'actions': [
          {
            'action_name': 'Generate GLIBC NMF and copy libs',
            'inputs': ['>(out_glibc64)', '>(out_glibc32)'],
            # NOTE: There is no explicit dependency for the lib32
            # and lib64 directories created in the PRODUCT_DIR.
            # They are created as a side-effect of NMF creation.
            'outputs': ['>(nmf_glibc)'],
            'action': [
              'python',
              '<(DEPTH)/native_client_sdk/src/tools/create_nmf.py',
              '>@(_inputs)',
              '--objdump=>(nacl_objdump)',
              '--library-path=>(libdir_glibc64)',
              '--library-path=>(libdir_glibc32)',
              '--output=>(nmf_glibc)',
              '--stage-dependencies=<(PRODUCT_DIR)',
              '--toolchain=glibc',
            ],
          },
        ],
        }],
      ],
    },
  ],
}
