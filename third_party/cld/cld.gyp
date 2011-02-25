# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'cld',
      'type': '<(library)',
      'dependencies': [
      	'../icu/icu.gyp:icuuc',
      ],
      'include_dirs': [
        '.',
      ],
      'defines': [
        'CLD_WINDOWS',
      ],
      'sources': [
        'encodings/compact_lang_det/cldutil.cc',
        'encodings/compact_lang_det/cldutil.h',
        'encodings/compact_lang_det/cldutil_dbg.h',
        'encodings/compact_lang_det/cldutil_dbg_empty.cc',
        'encodings/compact_lang_det/compact_lang_det.cc',
        'encodings/compact_lang_det/compact_lang_det.h',
        'encodings/compact_lang_det/compact_lang_det_impl.cc',
        'encodings/compact_lang_det/compact_lang_det_impl.h',
        'encodings/compact_lang_det/ext_lang_enc.cc',
        'encodings/compact_lang_det/ext_lang_enc.h',
        'encodings/compact_lang_det/getonescriptspan.cc',
        'encodings/compact_lang_det/getonescriptspan.h',
        'encodings/compact_lang_det/letterscript_enum.cc',
        'encodings/compact_lang_det/letterscript_enum.h',
        'encodings/compact_lang_det/subsetsequence.cc',
        'encodings/compact_lang_det/subsetsequence.h',
        'encodings/compact_lang_det/tote.cc',
        'encodings/compact_lang_det/tote.h',
        'encodings/compact_lang_det/utf8propjustletter.h',
        'encodings/compact_lang_det/utf8propletterscriptnum.h',
        'encodings/compact_lang_det/utf8scannotjustletterspecial.h',
        'encodings/compact_lang_det/generated/compact_lang_det_generated_cjkbis_0.cc',
        'encodings/compact_lang_det/generated/compact_lang_det_generated_ctjkvz.cc',
        'encodings/compact_lang_det/generated/compact_lang_det_generated_longwords8_0.cc',
        'encodings/compact_lang_det/generated/compact_lang_det_generated_meanscore.h',
        # 'encodings/compact_lang_det/generated/compact_lang_det_generated_quads_34rr.cc',
        # 'encodings/compact_lang_det/generated/compact_lang_det_generated_quads_128.cc',
        'encodings/compact_lang_det/generated/compact_lang_det_generated_quads_256.cc',
        'encodings/compact_lang_det/win/cld_basictypes.h',
        'encodings/compact_lang_det/win/cld_commandlineflags.h',
        'encodings/compact_lang_det/win/cld_google.h',
        'encodings/compact_lang_det/win/cld_htmlutils.h',
        'encodings/compact_lang_det/win/cld_htmlutils_windows.cc',
        'encodings/compact_lang_det/win/cld_logging.h',
        'encodings/compact_lang_det/win/cld_macros.h',
        'encodings/compact_lang_det/win/cld_strtoint.h',
        'encodings/compact_lang_det/win/cld_unicodetext.cc',
        'encodings/compact_lang_det/win/cld_unicodetext.h',
        'encodings/compact_lang_det/win/cld_unilib.h',
        'encodings/compact_lang_det/win/cld_unilib_windows.cc',
        'encodings/compact_lang_det/win/cld_utf.h',
        'encodings/compact_lang_det/win/cld_utf8statetable.cc',
        'encodings/compact_lang_det/win/cld_utf8statetable.h',
        'encodings/compact_lang_det/win/cld_utf8utils.h',
        'encodings/compact_lang_det/win/cld_utf8utils_windows.cc',
        'encodings/internal/encodings.cc',
        'encodings/proto/encodings.pb.h',
        'encodings/public/encodings.h',
        'languages/internal/languages.cc',
        'languages/proto/languages.pb.h',
        'languages/public/languages.h',
        'base/basictypes.h',
        'base/build_config.h',
        'base/casts.h',
        'base/commandlineflags.h',
        'base/global_strip_options.h',
        'base/logging.h',
        'base/macros.h',
        'base/port.h',
        'base/crash.h',
        'base/dynamic_annotations.h',
        'base/scoped_ptr.h',
        'base/stl_decl_msvc.h',
        'base/log_severity.h',
        'base/strtoint.h',
        'base/vlog_is_on.h',
        'base/string_util.h',
        'base/type_traits.h',
        'base/template_util.h',
      ],
      'direct_dependent_settings': {
        'defines': [
          'CLD_WINDOWS',
        ],
        'include_dirs': [
          '.',
        ],
      },
      'conditions': [
        ['OS=="win"', {
              'direct_dependent_settings': {
                'defines': [
                  'COMPILER_MSVC',
                ],
              },
              'msvs_disabled_warnings': [4005, 4006, 4018, 4244, 4309, 4800],
            },
        ],
        ['OS!="win"', {
              'direct_dependent_settings': {
                'defines': [
                  'COMPILER_GCC',
                ],
              },
            },
        ],
      ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
