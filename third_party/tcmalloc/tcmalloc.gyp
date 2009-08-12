# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'tcmalloc',
      'type': '<(library)',
      'msvs_guid': 'C564F145-9172-42C3-BFCB-60FDEA124321',
      'dependencies': [
        'libcmt',
      ],
      'include_dirs': [
        '.',
        'tcmalloc/src/windows',
        'tcmalloc/src/base',
        'tcmalloc/src',
      ],
      'direct_dependent_settings': {
        'configurations': {
          # TODO(bradnelson): find a way to make this more graceful in gyp.
          #    Ideally configurations should be able to have some sort of
          #    inheritance hierarchy. So that Purify no-tcmalloc could be
          #    be derived from Release.
          'Debug': {
            'msvs_settings': {
              'VCLinkerTool': {
                'IgnoreDefaultLibraryNames': ['libcmtd.lib', 'libcmt.lib'],
                'AdditionalDependencies': [
                  '<(SHARED_INTERMEDIATE_DIR)/tcmalloc/libcmt.lib'
                ],
              },
            },
          },
          'Release': {
            'msvs_settings': {
              'VCLinkerTool': {
                'IgnoreDefaultLibraryNames': ['libcmtd.lib', 'libcmt.lib'],
                'AdditionalDependencies': [
                  '<(SHARED_INTERMEDIATE_DIR)/tcmalloc/libcmt.lib'
                ],
              },
            },
          },
        },
      },
      'sources': [
        # tcmalloc files
        'tcmalloc/src/base/dynamic_annotations.cc',
        'tcmalloc/src/base/dynamic_annotations.h',
        'tcmalloc/src/base/logging.cc',
        'tcmalloc/src/base/logging.h',
        'tcmalloc/src/base/low_level_alloc.cc',
        'tcmalloc/src/base/low_level_alloc.h',
        'tcmalloc/src/base/spinlock.cc',
        'tcmalloc/src/base/spinlock.h',
        'tcmalloc/src/base/sysinfo.cc',
        'tcmalloc/src/base/sysinfo.h',
        'tcmalloc/src/central_freelist.cc',
        'tcmalloc/src/central_freelist.h',
        'tcmalloc/src/common.cc',
        'tcmalloc/src/common.h',
        'tcmalloc/src/heap-profile-table.cc',
        'tcmalloc/src/heap-profile-table.h',
        'tcmalloc/src/internal_logging.cc',
        'tcmalloc/src/internal_logging.h',
        'tcmalloc/src/linked_list.h',
        'tcmalloc/src/malloc_extension.cc',
        'tcmalloc/src/malloc_hook.cc',
        'tcmalloc/src/malloc_hook-inl.h',
        'tcmalloc/src/port.h',
        'tcmalloc/src/sampler.cc',
        'tcmalloc/src/sampler.h',
        'tcmalloc/src/span.cc',
        'tcmalloc/src/span.h',
        'tcmalloc/src/stack_trace_table.cc',
        'tcmalloc/src/stack_trace_table.h',
        'tcmalloc/src/stacktrace.cc',
        'tcmalloc/src/stacktrace.h',
        'tcmalloc/src/static_vars.cc',
        'tcmalloc/src/static_vars.h',
        'tcmalloc/src/thread_cache.cc',
        'tcmalloc/src/thread_cache.h',

        # tcmalloc forked files
        'allocator_shim.cc',
        'page_heap.cc',
        'port.cc',
        'system-alloc.h',
        'tcmalloc.cc',
        'win_allocator.cc',        

        # jemalloc files
        'jemalloc/jemalloc.c',
        'jemalloc/jemalloc.h',
        'jemalloc/ql.h',
        'jemalloc/qr.h',
        'jemalloc/rb.h',
      ],
      # sources! means that these are not compiled directly.
      'sources!': [
        'tcmalloc.cc',
        'win_allocator.cc',
      ],
      'msvs_settings': {
        # TODO(sgk):  merge this with build/common.gypi settings
        'VCLibrarianTool=': {
          'AdditionalOptions': '/ignore:4006,4221',
          'AdditionalLibraryDirectories':
            ['<(DEPTH)/third_party/platformsdk_win2008_6_1/files/Lib'],
        },
        'VCLinkerTool': {
          'AdditionalOptions': '/ignore:4006',
        },
      },
      'configurations': {
        'Debug': {
          'msvs_settings': {
            'VCCLCompilerTool': {
              'RuntimeLibrary': '0',
            },
          },
        },
      },
    },
    {
      'target_name': 'libcmt',
      'type': 'none',
      'actions': [
        {
          'action_name': 'libcmt',
          'inputs': [
            'prep_libc.sh',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/tcmalloc/libcmt.lib',
          ],
          'action': [
            './prep_libc.sh',
            '$(VCInstallDir)lib',
            '<(SHARED_INTERMEDIATE_DIR)/tcmalloc',
          ],
        },
      ],
    },
  ],
}
