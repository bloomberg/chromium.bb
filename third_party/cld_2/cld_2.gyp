# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Note to maintainers: In the October 2014 release, there are some options for
# building:
# Pick one quadgram file (cld2_generated_quadchrome*.cc):
#   _16 = 160K entries, smallest size, lowest accuracy (set cld2_table_size=0)
#   _2  = 256K entries, largest size, highest accuracy (set cld2_table_size=2)
#
# For the CJK bigram file (cld_generated_cjk_delta_bi*.cc), always use
# cld_generated_cjk_delta_bi_4.cc, as this is intended for use with Chromium.
# The _32 variant of the file is intended for applications that use the full
# 175-language version of CLD2.

{
  'target_defaults': {
    'conditions': [
      ['OS=="win"', {
        'msvs_disabled_warnings': [4267],
      }],
    ],
  },
  'variables': {
    # This variable controls which dependency is resolved by the pass-through
    # target 'cld2_platform_impl', and allows the embedder to choose which
    # kind of CLD2 support is required at build time:
    #
    # - If the value is 'static', then the cld2_platform_impl target will depend
    #   upon the cld2_static target
    # - If the value is 'dynamic', then the cld2_platform_impl target will
    #   depend upon the cld2_dynamic target.
    #
    # High-level targets for Chromium unit tests hard-code a dependency upon
    # cld2_static because doing so makes sense for use cases that aren't
    # affected by the loading of language detection data; however, most other
    # targets (e.g. the final executables and interactive UI tests) should be
    # linked against whatever the embedder needs.
    #
    # Maintainers:
    # This value may be reasonably tweaked in a 'conditions' block below on a
    # per-platform basis. Don't forget to update the expectations in
    # components/translate/content/browser/browser_cld_utils.cc as well, to
    # match whatever is done here.
    'cld2_platform_support%': 'static',

    # These sources need to be included in both static and dynamic builds as
    # well as the dynamic data tool.
    'cld2_core_sources': [
      'src/internal/cld2tablesummary.h',
      'src/internal/cldutil.h',
      'src/internal/cldutil_shared.h',
      'src/internal/compact_lang_det_hint_code.h',
      'src/internal/compact_lang_det_impl.h',
      'src/internal/debug.h',
      'src/internal/fixunicodevalue.h',
      'src/internal/generated_language.h',
      'src/internal/generated_ulscript.h',
      'src/internal/getonescriptspan.h',
      'src/internal/integral_types.h',
      'src/internal/lang_script.h',
      'src/internal/langspan.h',
      'src/internal/offsetmap.h',
      'src/internal/port.h',
      'src/internal/scoreonescriptspan.h',
      'src/internal/stringpiece.h',
      'src/internal/tote.h',
      'src/internal/utf8prop_lettermarkscriptnum.h',
      'src/internal/utf8repl_lettermarklower.h',
      'src/internal/utf8scannot_lettermarkspecial.h',
      'src/internal/utf8statetable.h',
      'src/public/compact_lang_det.h',
      'src/public/encodings.h',
    ],
    'cld2_core_impl_sources': [
      # Compilation is dependent upon flags.
      'src/internal/cldutil.cc',
      'src/internal/cldutil_shared.cc',
      'src/internal/compact_lang_det.cc',
      'src/internal/compact_lang_det_hint_code.cc',
      'src/internal/compact_lang_det_impl.cc',
      'src/internal/debug_empty.cc',
      'src/internal/fixunicodevalue.cc',
      'src/internal/generated_distinct_bi_0.cc',
      'src/internal/generated_entities.cc',
      'src/internal/generated_language.cc',
      'src/internal/generated_ulscript.cc',
      'src/internal/getonescriptspan.cc',
      'src/internal/lang_script.cc',
      'src/internal/offsetmap.cc',
      'src/internal/scoreonescriptspan.cc',
      'src/internal/tote.cc',
      'src/internal/utf8statetable.cc',
    ],
    'cld2_dynamic_data_loader_sources': [
      'src/internal/cld2_dynamic_data.h',
      'src/internal/cld2_dynamic_data.cc',
      'src/internal/cld2_dynamic_data_loader.h',
      'src/internal/cld2_dynamic_data_loader.cc',
    ],
    'cld2_data_sources': [
      'src/internal/cld2_generated_cjk_compatible.cc',
      'src/internal/cld2_generated_deltaoctachrome.cc',
      'src/internal/cld2_generated_distinctoctachrome.cc',
      'src/internal/cld_generated_cjk_delta_bi_4.cc',
      'src/internal/cld_generated_cjk_uni_prop_80.cc',
      'src/internal/cld_generated_score_quad_octa_2.cc',
      'src/internal/generated_distinct_bi_0.cc',
    ],
    'conditions': [
      ['cld2_table_size==0', {
        'cld2_data_sources+': ['src/internal/cld2_generated_quadchrome_16.cc']
      }],
      ['cld2_table_size==2', {
         'cld2_data_sources+': ['src/internal/cld2_generated_quadchrome_2.cc']
      }],
    ],
  },

  'targets': [
    {
      # GN version: //third_party/cld_2
      'target_name': 'cld_2_dynamic_data_tool',
      'type': 'executable',
      'include_dirs': [
        'src/internal',
        'src/public',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        '<@(cld2_core_sources)',
        '<@(cld2_core_impl_sources)',
        '<@(cld2_data_sources)',
        '<@(cld2_dynamic_data_loader_sources)',
        'src/internal/cld2_dynamic_data_extractor.h',
        'src/internal/cld2_dynamic_data_extractor.cc',
        'src/internal/cld2_dynamic_data_tool.cc',
      ],
      'defines': ['CLD2_DYNAMIC_MODE'],
    },

    {
      # GN version: //third_party/cld_2
      # Depending upon cld_2 will provide core headers and function definitions,
      # but no data. You must also depend on one of the following two targets:
      # cld2_static - for a statically-linked data set built into the executable
      # cld2_dynamic - for a dynamic data set loaded at runtime
      'target_name': 'cld_2',
      'type': 'static_library',
      'sources': ['<@(cld2_core_sources)'],
      'dependencies': [],
    },

    # As described above in the comments for cld2_platform_support, this is a
    # passthrough target that allows high-level targets to depend upon the same
    # CLD support as desired by the embedder.
    {
      'target_name': 'cld2_platform_impl',
      'type': 'none',
      'dependencies': ['cld2_<(cld2_platform_support)'],
    },

    {
      # GN version: //third_party/cld_2
      'target_name': 'cld2_static',
      'type': 'static_library',
      'include_dirs': [
        'src/internal',
        'src/public',
      ],
      'sources': [
        '<@(cld2_core_sources)',
        '<@(cld2_core_impl_sources)',
        '<@(cld2_data_sources)',
      ],
    },

    {
      # GN version: //third_party/cld_2
      'target_name': 'cld2_dynamic',
      'type': 'static_library',
      'include_dirs': [
        'src/internal',
        'src/public',
      ],
      'sources': [
        '<@(cld2_core_sources)',
        '<@(cld2_core_impl_sources)',
        '<@(cld2_dynamic_data_loader_sources)',
      ],
      'defines': ['CLD2_DYNAMIC_MODE'],
    },
  ],
}
