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
        'bar/common/scopedptr.h',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/cldutil.cc',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/cldutil.h',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/cldutil_dbg.h',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/cldutil_dbg_empty.cc',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/compact_lang_det.cc',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/compact_lang_det.h',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/compact_lang_det_impl.cc',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/compact_lang_det_impl.h',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/ext_lang_enc.cc',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/ext_lang_enc.h',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/getonescriptspan.cc',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/getonescriptspan.h',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/letterscript_enum.cc',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/letterscript_enum.h',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/subsetsequence.cc',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/subsetsequence.h',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/tote.cc',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/tote.h',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/utf8propjustletter.h',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/utf8propletterscriptnum.h',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/utf8scannotjustletterspecial.h',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/generated/compact_lang_det_generated_cjkbis_0.cc',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/generated/compact_lang_det_generated_ctjkvz.cc',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/generated/compact_lang_det_generated_longwords8_0.cc',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/generated/compact_lang_det_generated_meanscore.h',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/generated/compact_lang_det_generated_quads_128.cc',
        # For now using the 128 bytes detection in order to save hundreds of KBs on the final package.
        # 'bar/toolbar/cld/i18n/encodings/compact_lang_det/generated/compact_lang_det_generated_quads_256.cc',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_basictypes.h',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_commandlineflags.h',
        # We use the static table at this point, so we don't need to compile the following files:
        #'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_dynamicstate.h',
        #'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_dynamicstate.cc',
        #'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_loadpolicy.cc',
        #'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_loadpolicy.h',
        #'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_loadpolicyinterface.h',
        #'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_resourceids.h',
        #'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_service.h',
        #'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_service.cc',
        #'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_serviceinterface.h',
        #'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_tables.cc',
        #'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_tables.h',
        #'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/resourceinmemory.cc',
        #'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/resourceinmemory.h',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_google.h',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_htmlutils.h',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_htmlutils_windows.cc',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_logging.h',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_macros.h',
	# None of files we build require these two headers.
        #'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_scoped_ptr.h',
        #'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_scopedptr.h',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_strtoint.h',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_unicodetext.cc',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_unicodetext.h',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_unilib.h',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_unilib_windows.cc',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_utf.h',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_utf8statetable.cc',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_utf8statetable.h',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_utf8utils.h',
        'bar/toolbar/cld/i18n/encodings/compact_lang_det/win/cld_utf8utils_windows.cc',
        'bar/toolbar/cld/i18n/encodings/internal/encodings.cc',
        'bar/toolbar/cld/i18n/encodings/proto/encodings.pb.h',
        'bar/toolbar/cld/i18n/encodings/public/encodings.h',
        'bar/toolbar/cld/i18n/languages/internal/languages.cc',
        'bar/toolbar/cld/i18n/languages/proto/languages.pb.h',
        'bar/toolbar/cld/i18n/languages/public/languages.h',
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
