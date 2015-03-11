# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'use_system_sqlite%': 0,
    'required_sqlite_version': '3.6.1',
  },
  'target_defaults': {
    'defines': [
      'SQLITE_CORE',
      'SQLITE_ENABLE_FTS3',
      # New unicode61 tokenizer with built-in tables.
      'SQLITE_DISABLE_FTS3_UNICODE',
      # Chromium currently does not enable fts4, disable extra code.
      'SQLITE_DISABLE_FTS4_DEFERRED',
      'SQLITE_ENABLE_ICU',
      'SQLITE_ENABLE_MEMORY_MANAGEMENT',
      'SQLITE_SECURE_DELETE',
      # Custom flag to tweak pcache pools.
      # TODO(shess): This shouldn't use faux-SQLite naming.      
      'SQLITE_SEPARATE_CACHE_POOLS',
      # TODO(shess): SQLite adds mutexes to protect structures which cross
      # threads.  In theory Chromium should be able to turn this off for a
      # slight speed boost.
      'THREADSAFE',
      # TODO(shess): Figure out why this is here.  Nobody references it
      # directly.
      '_HAS_EXCEPTIONS=0',
      # NOTE(shess): Some defines can affect the amalgamation.  Those should be
      # added to google_generate_amalgamation.sh, and the amalgamation
      # re-generated.  Usually this involves disabling features which include
      # keywords or syntax, for instance SQLITE_OMIT_VIRTUALTABLE omits the
      # virtual table syntax entirely.  Missing an item usually results in
      # syntax working but execution failing.  Review:
      #   src/src/parse.py
      #   src/tool/mkkeywordhash.c
    ],
  },
  'targets': [
    {
      'target_name': 'sqlite',
      'conditions': [
        [ 'chromeos==1' , {
            'defines': [
                # Despite obvious warnings about not using this flag
                # in deployment, we are turning off sync in ChromeOS
                # and relying on the underlying journaling filesystem
                # to do error recovery properly.  It's much faster.
                'SQLITE_NO_SYNC',
                ],
          },
        ],
        ['os_posix == 1', {
          'defines': [
            # Allow xSleep() call on Unix to use usleep() rather than sleep().
            # Microsecond precision is better than second precision.  Should
            # only affect contended databases via the busy callback.  Browser
            # profile databases are mostly exclusive, but renderer databases may
            # allow for contention.
            'HAVE_USLEEP=1',
          ],
        }],
        ['use_system_sqlite', {
          'type': 'none',
          'direct_dependent_settings': {
            'defines': [
              'USE_SYSTEM_SQLITE',
            ],
          },

          'conditions': [
            ['OS == "ios"', {
              'dependencies': [
                'sqlite_regexp',
              ],
              'link_settings': {
                'libraries': [
                  '$(SDKROOT)/usr/lib/libsqlite3.dylib',
                ],
              },
            }],
            ['os_posix == 1 and OS != "mac" and OS != "ios" and OS != "android"', {
              'direct_dependent_settings': {
                'cflags': [
                  # This next command produces no output but it it will fail
                  # (and cause GYP to fail) if we don't have a recent enough
                  # version of sqlite.
                  '<!@(pkg-config --atleast-version=<(required_sqlite_version) sqlite3)',

                  '<!@(pkg-config --cflags sqlite3)',
                ],
              },
              'link_settings': {
                'ldflags': [
                  '<!@(pkg-config --libs-only-L --libs-only-other sqlite3)',
                ],
                'libraries': [
                  '<!@(pkg-config --libs-only-l sqlite3)',
                ],
              },
            }],
          ],
        }, { # !use_system_sqlite
          'product_name': 'sqlite3',
          'type': 'static_library',
          'sources': [
            'amalgamation/sqlite3.h',
            'amalgamation/sqlite3.c',
          ],
          'include_dirs': [
            'amalgamation',
          ],
          'dependencies': [
            '../icu/icu.gyp:icui18n',
            '../icu/icu.gyp:icuuc',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '.',
              '../..',
            ],
          },
          'msvs_disabled_warnings': [
            4018, 4244, 4267,
          ],
          'conditions': [
            ['OS=="linux"', {
              'link_settings': {
                'libraries': [
                  '-ldl',
                ],
              },
            }],
            ['OS == "mac" or OS == "ios"', {
              'link_settings': {
                'libraries': [
                  '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
                ],
              },
            }],
            ['OS == "android"', {
              'defines': [
                'SQLITE_DEFAULT_JOURNAL_SIZE_LIMIT=1048576',
                'SQLITE_DEFAULT_AUTOVACUUM=1',
                'SQLITE_TEMP_STORE=3',
                'SQLITE_ENABLE_FTS3_BACKWARDS',
                'SQLITE_DEFAULT_FILE_FORMAT=4',
              ],
            }],
            ['os_posix == 1 and OS != "mac" and OS != "android"', {
              'cflags': [
                # SQLite doesn't believe in compiler warnings,
                # preferring testing.
                #   http://www.sqlite.org/faq.html#q17
                '-Wno-int-to-pointer-cast',
                '-Wno-pointer-to-int-cast',
              ],
            }],
            # Enable feedback-directed optimisation for sqlite when building in android.
            ['android_webview_build == 1', {
              'aosp_build_settings': {
                'LOCAL_FDO_SUPPORT': 'true',
              },
            }],
          ],
        }],
      ],
      'includes': [
        # Disable LTO due to ELF section name out of range
        # crbug.com/422251
        '../../build/android/disable_lto.gypi',
      ],
    },
  ],
  'conditions': [
    ['os_posix == 1 and OS != "mac" and OS != "ios" and OS != "android" and not use_system_sqlite', {
      'targets': [
        {
          'target_name': 'sqlite_shell',
          'type': 'executable',
          'dependencies': [
            '../icu/icu.gyp:icuuc',
            'sqlite',
          ],
          'sources': [
            'src/src/shell.c',
            'src/src/shell_icu_linux.c',
            # Include a dummy c++ file to force linking of libstdc++.
            'build_as_cpp.cc',
          ],
        },
      ],
    },],
    ['OS == "ios"', {
      'targets': [
        {
          'target_name': 'sqlite_regexp',
          'type': 'static_library',
          'dependencies': [
            '../icu/icu.gyp:icui18n',
            '../icu/icu.gyp:icuuc',
          ],
          'sources': [
            'src/ext/icu/icu.c',
          ],
        },
      ],
    }],
  ],
}
