# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'printing',
      'type': '<(library)',
      'dependencies': [
        '../app/app.gyp:app_base',  # Only required for Font support
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../build/temp_gyp/googleurl.gyp:googleurl',
        '../skia/skia.gyp:skia',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
      ],
      'msvs_guid': '9E5416B9-B91B-4029-93F4-102C1AD5CAF4',
      'include_dirs': [
        '..',
      ],
      'sources': [
        'emf_win.cc',
        'emf_win.h',
        'image.cc',
        'image.h',
        'native_metafile.h',
        'page_number.cc',
        'page_number.h',
        'page_overlays.cc',
        'page_overlays.h',
        'page_range.cc',
        'page_range.h',
        'page_setup.cc',
        'page_setup.h',
        'pdf_metafile_mac.h',
        'pdf_metafile_mac.cc',
        'pdf_ps_metafile_cairo.h',
        'pdf_ps_metafile_cairo.cc',
        'print_settings.cc',
        'print_settings.h',
        'printed_document.cc',
        'printed_document_cairo.cc',
        'printed_document_mac.cc',
        'printed_document_win.cc',
        'printed_document.h',
        'printed_page.cc',
        'printed_page.h',
        'printed_pages_source.h',
        'printing_context.h',
        'printing_context_cairo.cc',
        'printing_context_mac.mm',
        'printing_context_win.cc',
        'units.cc',
        'units.h',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
      },
      'conditions': [
        ['OS!="linux" and OS!="freebsd" and OS!="openbsd"',{
            'sources/': [['exclude', '_cairo\\.cc$']]
        }],
        ['OS!="mac"', {'sources/': [['exclude', '_mac\\.(cc|mm?)$']]}],
        ['OS!="win"', {'sources/': [['exclude', '_win\\.cc$']]
          }, {  # else: OS=="win"
            'sources/': [['exclude', '_posix\\.cc$']]
        }],
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
          'dependencies': [
            # For FT_Init_FreeType and friends.
            '../build/linux/system.gyp:freetype2',
            '../build/linux/system.gyp:gtk',
            '../build/linux/system.gyp:gtkprint',
          ],
        }],
      ],
    },
    {
      'target_name': 'printing_unittests',
      'type': 'executable',
      'msvs_guid': '8B2EE5D9-41BC-4AA2-A401-2DC143A05D2E',
      'dependencies': [
        'printing',
        '../testing/gtest.gyp:gtest',
        '../base/base.gyp:test_support_base',
      ],
      'sources': [
        'emf_win_unittest.cc',
        'printing_test.h',
        'page_number_unittest.cc',
        'page_overlays_unittest.cc',
        'page_range_unittest.cc',
        'page_setup_unittest.cc',
        'pdf_metafile_mac_unittest.cc',
        'pdf_ps_metafile_cairo_unittest.cc',
        'printed_page_unittest.cc',
        'printing_context_win_unittest.cc',
        'run_all_unittests.cc',
        'units_unittest.cc',
      ],
      'conditions': [
        ['OS!="linux"', {'sources/': [['exclude', '_cairo_unittest\\.cc$']]}],
        ['OS!="mac"', {'sources/': [['exclude', '_mac_unittest\\.(cc|mm?)$']]}],
        ['OS!="win"', {'sources/': [['exclude', '_win_unittest\\.cc$']]
          }, {  # else: OS=="win"
            'sources/': [['exclude', '_cairo_unittest\\.cc$']]
          }
        ],
        ['OS=="linux" or OS=="freebsd" or OS=="openbsd"', {
            'dependencies': [
              '../build/linux/system.gyp:gtk',
           ],
        }],
        ['OS=="linux"', {
          'conditions': [
            ['linux_use_tcmalloc==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
