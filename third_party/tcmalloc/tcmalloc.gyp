# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'tcmalloc',
      'type': '<(library)',
      'msvs_guid': 'C564F145-9172-42C3-BFCB-60FDEA124321',
      'include_dirs': [
        '.',
        'chromium/src/base',
        'tcmalloc/src',
        '../..',
      ],
      'defines': [
        'NO_TCMALLOC_SAMPLES',
      ],
      'direct_dependent_settings': {
        'configurations': {
          'Common': {
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
        'conditions': [
          ['OS=="win"', {
            'defines': [
              ['PERFTOOLS_DLL_DECL', '']
            ],
          }],
        ],
      },
      'sources': [
        'chromium/src/config.h',
        'chromium/src/config_linux.h',
        'chromium/src/config_win.h',

        # tcmalloc files
        'chromium/src/base/dynamic_annotations.cc',
        'chromium/src/base/dynamic_annotations.h',
        'chromium/src/base/logging.cc',
        'chromium/src/base/logging.h',
        'chromium/src/base/low_level_alloc.cc',
        'chromium/src/base/low_level_alloc.h',
        'chromium/src/base/spinlock.cc',
        'chromium/src/base/spinlock.h',
        'chromium/src/base/sysinfo.cc',
        'chromium/src/base/sysinfo.h',
        'chromium/src/central_freelist.cc',
        'chromium/src/central_freelist.h',
        'chromium/src/common.cc',
        'chromium/src/common.h',
        'chromium/src/heap-profile-table.cc',
        'chromium/src/heap-profile-table.h',
        'chromium/src/internal_logging.cc',
        'chromium/src/internal_logging.h',
        'chromium/src/linked_list.h',
        'chromium/src/malloc_hook.cc',
        'chromium/src/malloc_hook-inl.h',
        'chromium/src/malloc_extension.cc',
        'chromium/src/google/malloc_extension.h',
        'chromium/src/page_heap.cc',
        'chromium/src/page_heap.h',
        'chromium/src/port.h',
        'chromium/src/sampler.cc',
        'chromium/src/sampler.h',
        'chromium/src/span.cc',
        'chromium/src/span.h',
        'chromium/src/stack_trace_table.cc',
        'chromium/src/stack_trace_table.h',
        'chromium/src/stacktrace.cc',
        'chromium/src/stacktrace.h',
        'chromium/src/static_vars.cc',
        'chromium/src/static_vars.h',
        'chromium/src/thread_cache.cc',
        'chromium/src/thread_cache.h',

        # non-windows
        'chromium/src/base/linuxthreads.cc',
        'chromium/src/base/linuxthreads.h',
        'chromium/src/base/vdso_support.cc',
        'chromium/src/base/vdso_support.h',
        'chromium/src/google/tcmalloc.h',
        'chromium/src/maybe_threads.cc',
        'chromium/src/maybe_threads.h',
        'chromium/src/symbolize.cc',
        'chromium/src/symbolize.h',
        'chromium/src/system-alloc.cc',
        'chromium/src/system-alloc.h',
        'chromium/src/tcmalloc.cc',

        # heap-profiler/checker/cpuprofiler
        'chromium/src/base/thread_lister.c',
        'chromium/src/base/thread_lister.h',
        'chromium/src/heap-checker-bcad.cc',
        'chromium/src/heap-checker.cc',
        'chromium/src/heap-profiler.cc',
        'chromium/src/memory_region_map.cc',
        'chromium/src/memory_region_map.h',
        'chromium/src/profiledata.cc',
        'chromium/src/profiledata.h',
        'chromium/src/profile-handler.cc',
        'chromium/src/profile-handler.h',
        'chromium/src/profiler.cc',
        'chromium/src/raw_printer.cc',
        'chromium/src/raw_printer.h',

        # tcmalloc forked files
        'chromium/src/allocator_shim.cc',
        'chromium/src/generic_allocators.cc',
        'chromium/src/page_heap.cc',
        'chromium/src/page_heap.h',
        'chromium/src/port.cc',
        'chromium/src/system-alloc.h',
        'chromium/src/tcmalloc.cc',
        'chromium/src/win_allocator.cc',        

        # jemalloc files
        'jemalloc/jemalloc.c',
        'jemalloc/jemalloc.h',
        'jemalloc/ql.h',
        'jemalloc/qr.h',
        'jemalloc/rb.h',
      ],
      # sources! means that these are not compiled directly.
      'sources!': [
        'chromium/src/generic_allocators.cc',
        'chromium/src/tcmalloc.cc',
        'chromium/src/win_allocator.cc',
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
      'conditions': [
        ['OS=="win"', {
          'defines': [
            ['PERFTOOLS_DLL_DECL', '']
          ],
          'dependencies': [
            'libcmt',
          ],
          'include_dirs': [
            'chromium/src/windows',
          ],
          'sources!': [
            'chromium/src/base/linuxthreads.cc',
            'chromium/src/base/linuxthreads.h',
            'chromium/src/base/vdso_support.cc',
            'chromium/src/base/vdso_support.h',
            'chromium/src/maybe_threads.cc',
            'chromium/src/maybe_threads.h',
            'chromium/src/symbolize.cc',
            'chromium/src/symbolize.h',
            'chromium/src/system-alloc.cc',
            'chromium/src/system-alloc.h',

            # heap-profiler/checker/cpuprofiler
            'chromium/src/base/thread_lister.c',
            'chromium/src/base/thread_lister.h',
            'chromium/src/heap-checker-bcad.cc',
            'chromium/src/heap-checker.cc',
            'chromium/src/heap-profiler.cc',
            'chromium/src/memory_region_map.cc',
            'chromium/src/memory_region_map.h',
            'chromium/src/profiledata.cc',
            'chromium/src/profiledata.h',
            'chromium/src/profile-handler.cc',
            'chromium/src/profile-handler.h',
            'chromium/src/profiler.cc',
          ],
        }],
        ['OS=="linux"', {
          'sources!': [
            'chromium/src/page_heap.cc',
            'chromium/src/port.cc',
            'chromium/src/system-alloc.h',
            'chromium/src/win_allocator.cc',        

            # TODO(willchan): Support allocator shim later on.
            'chromium/src/allocator_shim.cc',

            # TODO(willchan): support jemalloc on other platforms
            # jemalloc files
            'jemalloc/jemalloc.c',
            'jemalloc/jemalloc.h',
            'jemalloc/ql.h',
            'jemalloc/qr.h',
            'jemalloc/rb.h',
          ],
          'cflags!': [
            '-fvisibility=hidden',
          ],
          'link_settings': {
            'ldflags': [
              # Don't let linker rip this symbol out, otherwise the heap&cpu
              # profilers will not initialize properly on startup.
              '-Wl,-uIsHeapProfilerRunning,-uProfilerStart',
              # Do the same for heap leak checker.
              '-Wl,-u_Z21InitialMallocHook_NewPKvj,-u_Z22InitialMallocHook_MMapPKvS0_jiiix,-u_Z22InitialMallocHook_SbrkPKvi',
              '-Wl,-u_Z21InitialMallocHook_NewPKvm,-u_Z22InitialMallocHook_MMapPKvS0_miiil,-u_Z22InitialMallocHook_SbrkPKvl',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'tcmalloc_unittests',
      'type': 'executable',
      'dependencies': [
        'tcmalloc',
        '../../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '.',
        'chromium/src/base',
        'tcmalloc/src',
        '../..',
      ],
      'msvs_guid': 'E99DA267-BE90-4F45-1294-6919DB2C9999',
      'sources': [
        'chromium/src/unittest_utils.cc',
        'chromium/src/tcmalloc_unittests.cc',
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
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
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
