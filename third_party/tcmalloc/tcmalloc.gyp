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
        'tcmalloc/src/base',
        'tcmalloc/src',
        '../..',
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
        'conditions': [
          ['OS=="win"', {
            'defines': [
              ['PERFTOOLS_DLL_DECL', '']
            ],
          }],
        ],
      },
      'sources': [
        'config.h',
        'config_linux.h',
        'config_win.h',

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
        'tcmalloc/src/malloc_hook.cc',
        'tcmalloc/src/malloc_hook-inl.h',
        'tcmalloc/src/malloc_extension.cc',
        'tcmalloc/src/google/malloc_extension.h',
        'tcmalloc/src/page_heap.cc',
        'tcmalloc/src/page_heap.h',
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

        # non-windows
        'tcmalloc/src/base/linuxthreads.cc',
        'tcmalloc/src/base/linuxthreads.h',
        'tcmalloc/src/base/vdso_support.cc',
        'tcmalloc/src/base/vdso_support.h',
        'tcmalloc/src/google/tcmalloc.h',
        'tcmalloc/src/maybe_threads.cc',
        'tcmalloc/src/maybe_threads.h',
        'tcmalloc/src/system-alloc.cc',
        'tcmalloc/src/system-alloc.h',
        'tcmalloc/src/tcmalloc.cc',

        # heap-profiler/checker/cpuprofiler
        'tcmalloc/src/base/thread_lister.c',
        'tcmalloc/src/base/thread_lister.h',
        'tcmalloc/src/heap-checker-bcad.cc',
        'tcmalloc/src/heap-checker.cc',
        'tcmalloc/src/heap-profiler.cc',
        'tcmalloc/src/memory_region_map.cc',
        'tcmalloc/src/memory_region_map.h',
        'tcmalloc/src/profiledata.cc',
        'tcmalloc/src/profiledata.h',
        'tcmalloc/src/profile-handler.cc',
        'tcmalloc/src/profile-handler.h',
        'tcmalloc/src/profiler.cc',
        'tcmalloc/src/raw_printer.cc',
        'tcmalloc/src/raw_printer.h',

        # tcmalloc forked files
        'allocator_shim.cc',
        'generic_allocators.cc',
        'page_heap.cc',
        'page_heap.h',
        'port.cc',
        'system-alloc.h',
        'tcmalloc.cc',
        'win_allocator.cc',        

        'malloc_hook.cc',

        # jemalloc files
        'jemalloc/jemalloc.c',
        'jemalloc/jemalloc.h',
        'jemalloc/ql.h',
        'jemalloc/qr.h',
        'jemalloc/rb.h',
      ],
      # sources! means that these are not compiled directly.
      'sources!': [
        'generic_allocators.cc',
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
      'conditions': [
        ['OS=="win"', {
          'defines': [
            ['PERFTOOLS_DLL_DECL', '']
          ],
          'dependencies': [
            'libcmt',
          ],
          'include_dirs': [
            'tcmalloc/src/windows',
          ],
          'sources!': [
            'tcmalloc/src/base/linuxthreads.cc',
            'tcmalloc/src/base/linuxthreads.h',
            'tcmalloc/src/base/vdso_support.cc',
            'tcmalloc/src/base/vdso_support.h',
            'tcmalloc/src/maybe_threads.cc',
            'tcmalloc/src/maybe_threads.h',
            'tcmalloc/src/system-alloc.cc',
            'tcmalloc/src/system-alloc.h',

            # use forked version in windows
            'tcmalloc/src/tcmalloc.cc',
            'tcmalloc/src/page_heap.cc',
            'tcmalloc/src/page_heap.h',

            # heap-profiler/checker/cpuprofiler
            'tcmalloc/src/base/thread_lister.c',
            'tcmalloc/src/base/thread_lister.h',
            'tcmalloc/src/heap-checker-bcad.cc',
            'tcmalloc/src/heap-checker.cc',
            'tcmalloc/src/heap-profiler.cc',
            'tcmalloc/src/memory_region_map.cc',
            'tcmalloc/src/memory_region_map.h',
            'tcmalloc/src/profiledata.cc',
            'tcmalloc/src/profiledata.h',
            'tcmalloc/src/profile-handler.cc',
            'tcmalloc/src/profile-handler.h',
            'tcmalloc/src/profiler.cc',

            # don't use linux forked versions
            'malloc_hook.cc',
          ],
        }],
        ['OS=="linux"', {
          'sources!': [
            'page_heap.cc',
            'port.cc',
            'system-alloc.h',
            'win_allocator.cc',        

            # TODO(willchan): Support allocator shim later on.
            'allocator_shim.cc',

            # TODO(willchan): support jemalloc on other platforms
            # jemalloc files
            'jemalloc/jemalloc.c',
            'jemalloc/jemalloc.h',
            'jemalloc/ql.h',
            'jemalloc/qr.h',
            'jemalloc/rb.h',

            # TODO(willchan): Unfork linux.
            'tcmalloc/src/malloc_hook.cc',
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
        'tcmalloc/src/base',
        'tcmalloc/src',
        '../..',
      ],
      'msvs_guid': 'E99DA267-BE90-4F45-1294-6919DB2C9999',
      'sources': [
        'unittest_utils.cc',
        'tcmalloc_unittests.cc',
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
