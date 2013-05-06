#
# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

{
  'includes': [
    '../core.gypi',
    '../../bindings/bindings.gypi',
    '../features.gypi',
  ],

  'targets': [
    {
      'target_name': 'generate_test_support_idls',
      'type': 'none',
      'actions': [
        {
          'action_name': 'Settings',
          'inputs': [
            '../page/make_settings.pl',
            '../page/Settings.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/SettingsMacros.h',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/InternalSettingsGenerated.idl',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/InternalSettingsGenerated.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/InternalSettingsGenerated.h',
          ],
          'action': [
            'python',
            'scripts/action_makenames.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)',
          ],
          'msvs_cygwin_shell': 1,
        },
        {
          'action_name': 'InternalRuntimeFlags',
          'inputs': [
            '<@(scripts_for_in_files)',
            '../scripts/make_internal_runtime_flags.py',
            '../page/RuntimeEnabledFeatures.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/InternalRuntimeFlags.idl',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/InternalRuntimeFlags.h',
          ],
          'action': [
            'python',
            '../scripts/make_internal_runtime_flags.py',
            '../page/RuntimeEnabledFeatures.in',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/',
          ],
        },
      ]
    },
    {
      'target_name': 'make_derived_sources',
      'type': 'none',
      'hard_dependency': 1,
      'dependencies': [
        'generate_test_support_idls',
      ],
      'sources': [
        # bison rule
        '<(SHARED_INTERMEDIATE_DIR)/webkit/CSSGrammar.y',
        '../xml/XPathGrammar.y',

        # gperf rule
        '../platform/ColorData.gperf',
      ],
      'actions': [
        {
          'action_name': 'generateV8ArrayBufferViewCustomScript',
          'inputs': [
            '<(bindings_dir)/v8/custom/V8ArrayBufferViewCustomScript.js',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/V8ArrayBufferViewCustomScript.h',
          ],
          'msvs_cygwin_shell': 0,
          'action': [
            '<(perl_exe)',
            '../inspector/xxd.pl',
            'V8ArrayBufferViewCustomScript_js',
            '<@(_inputs)',
            '<@(_outputs)'
          ],
          'message': 'Generating V8ArrayBufferViewCustomScript.h from V8ArrayBufferViewCustomScript.js',
        },
        {
          'action_name': 'generateXMLViewerCSS',
          'inputs': [
            '../xml/XMLViewer.css',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/XMLViewerCSS.h',
          ],
          'msvs_cygwin_shell': 0,
          'action': [
            '<(perl_exe)',
            '../inspector/xxd.pl',
            'XMLViewer_css',
            '<@(_inputs)',
            '<@(_outputs)'
          ],
        },
        {
          'action_name': 'generateXMLViewerJS',
          'inputs': [
            '../xml/XMLViewer.js',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/XMLViewerJS.h',
          ],
          'msvs_cygwin_shell': 0,
          'action': [
            '<(perl_exe)',
            '../inspector/xxd.pl',
            'XMLViewer_js',
            '<@(_inputs)',
            '<@(_outputs)'
          ],
        },
        {
          'action_name': 'HTMLEntityTable',
          'inputs': [
            '../html/parser/create-html-entity-table',
            '../html/parser/HTMLEntityNames.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/HTMLEntityTable.cpp'
          ],
          'action': [
            'python',
            '../html/parser/create-html-entity-table',
            '-o',
            '<@(_outputs)',
            '<@(_inputs)'
          ],
        },
        {
          'action_name': 'RuntimeEnabledFeatures',
          'inputs': [
            '<@(scripts_for_in_files)',
            '../scripts/make_runtime_features.py',
            '../page/RuntimeEnabledFeatures.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/RuntimeEnabledFeatures.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/RuntimeEnabledFeatures.h',
          ],
          'action': [
            'python',
            '../scripts/make_runtime_features.py',
            '../page/RuntimeEnabledFeatures.in',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/',
          ],
        },
        {
          'action_name': 'CSSPropertyNames',
          'inputs': [
            '../css/makeprop.pl',
            '../css/CSSPropertyNames.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/CSSPropertyNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/CSSPropertyNames.h',
          ],
          'action': [
            'python',
            'scripts/action_csspropertynames.py',
            '<@(_outputs)',
            '--',
            '--defines', '<(feature_defines)',
            '--',
            '<@(_inputs)',
          ],
          'conditions': [
            # TODO(maruel): Move it in its own project or generate it anyway?
            ['enable_svg!=0', {
              'inputs': [
                '../css/SVGCSSPropertyNames.in',
              ],
            }],
          ],
          'msvs_cygwin_shell': 1,
        },
        {
          'action_name': 'CSSValueKeywords',
          'inputs': [
            '../css/makevalues.pl',
            '../css/CSSValueKeywords.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/CSSValueKeywords.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/CSSValueKeywords.h',
          ],
          'action': [
            'python',
            'scripts/action_cssvaluekeywords.py',
            '<@(_outputs)',
            '--',
            '--defines', '<(feature_defines)',
            '--',
            '<@(_inputs)',
          ],
          'conditions': [
            # TODO(maruel): Move it in its own project or generate it anyway?
            ['enable_svg!=0', {
              'inputs': [
                '../css/SVGCSSValueKeywords.in',
              ],
            }],
          ],
          'msvs_cygwin_shell': 1,
        },
        {
          'action_name': 'HTMLNames',
          'inputs': [
            '../scripts/Hasher.pm',
            '../scripts/StaticString.pm',
            '../scripts/make_names.pl',
            '../html/HTMLTagNames.in',
            '../html/HTMLAttributeNames.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/HTMLNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/HTMLNames.h',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/HTMLElementFactory.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/V8HTMLElementWrapperFactory.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/V8HTMLElementWrapperFactory.h',
          ],
          'action': [
            'python',
            'scripts/action_makenames.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)',
            '--',
            '--factory',
            '--extraDefines', '<(feature_defines)'
          ],
          'msvs_cygwin_shell': 1,
        },
        {
          'action_name': 'WebKitFontFamilyNames',
          'inputs': [
            '../scripts/Hasher.pm',
            '../scripts/StaticString.pm',
            '../scripts/make_names.pl',
            '../css/WebKitFontFamilyNames.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/WebKitFontFamilyNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/WebKitFontFamilyNames.h',
          ],
          'action': [
            'python',
            'scripts/action_makenames.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)',
            '--',
            '--fonts',
          ],
          'msvs_cygwin_shell': 1,
        },
        {
          'action_name': 'SVGNames',
          'inputs': [
            '../scripts/Hasher.pm',
            '../scripts/StaticString.pm',
            '../scripts/make_names.pl',
            '../svg/svgtags.in',
            '../svg/svgattrs.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/SVGNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/SVGNames.h',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/SVGElementFactory.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/SVGElementFactory.h',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/V8SVGElementWrapperFactory.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/V8SVGElementWrapperFactory.h',
          ],
          'action': [
            'python',
            'scripts/action_makenames.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)',
            '--',
            '--factory',
            '--extraDefines', '<(feature_defines)'
          ],
          'msvs_cygwin_shell': 1,
        },
        {
          'action_name': 'EventFactory',
          'inputs': [
            '<@(scripts_for_in_files)',
            '../scripts/make_event_factory.py',
            '../dom/EventNames.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/EventFactory.cpp',
          ],
          'action': [
            'python',
            '../scripts/make_event_factory.py',
            '../dom/EventNames.in',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/',
          ],
        },
        {
          'action_name': 'EventFactoryHeaders',
          'inputs': [
            '../scripts/InFilesCompiler.pm',
            '../scripts/InFilesParser.pm',
            '../scripts/make_event_factory.pl',
            '../dom/EventNames.in',
          ],
          'outputs': [
            # FIXME: These files should be generated by make_event_factory.py.
            '<(SHARED_INTERMEDIATE_DIR)/webkit/EventHeaders.h',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/EventInterfaces.h',
          ],
          'action': [
            'python',
            'scripts/action_makenames.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)',
          ],
          'msvs_cygwin_shell': 1,
        },
        {
          'action_name': 'EventTargetFactory',
          'inputs': [
            '../scripts/InFilesCompiler.pm',
            '../scripts/InFilesParser.pm',
            '../scripts/make_event_factory.pl',
            '../dom/EventTargetFactory.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/EventTargetHeaders.h',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/EventTargetInterfaces.h',
          ],
          'action': [
            'python',
            'scripts/action_makenames.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)',
          ],
          'msvs_cygwin_shell': 1,
        },
        {
          'action_name': 'ExceptionCodeDescription',
          'inputs': [
            '../scripts/InFilesCompiler.pm',
            '../scripts/InFilesParser.pm',
            '../scripts/make_dom_exceptions.pl',
            '../dom/DOMExceptions.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/ExceptionCodeDescription.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/ExceptionCodeDescription.h',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/ExceptionHeaders.h',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/ExceptionInterfaces.h',
          ],
          'action': [
            'python',
            'scripts/action_makenames.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)',
          ],
          'msvs_cygwin_shell': 1,
        },
        {
          'action_name': 'MathMLNames',
          'inputs': [
            '../scripts/Hasher.pm',
            '../scripts/StaticString.pm',
            '../scripts/make_names.pl',
            '../mathml/mathtags.in',
            '../mathml/mathattrs.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/MathMLNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/MathMLNames.h',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/MathMLElementFactory.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/MathMLElementFactory.h',
          ],
          'action': [
            'python',
            'scripts/action_makenames.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)',
            '--',
            '--factory',
            '--extraDefines', '<(feature_defines)'
          ],
          'msvs_cygwin_shell': 1,
        },
        {
          'action_name': 'UserAgentStyleSheets',
          'variables': {
            'scripts': [
              '../css/make-css-file-arrays.pl',
              '../scripts/preprocessor.pm',
            ],
            # The .css files are in the same order as ../DerivedSources.make.
            'stylesheets': [
              '../css/html.css',
              '../css/quirks.css',
              '../css/view-source.css',
              '../css/themeChromium.css', # Chromium only.
              '../css/themeChromiumAndroid.css', # Chromium only.
              '../css/themeChromiumLinux.css', # Chromium only.
              '../css/themeChromiumSkia.css',  # Chromium only.
              '../css/themeWin.css',
              '../css/themeWinQuirks.css',
              '../css/svg.css',
              '../css/mathml.css',
              '../css/mediaControls.css',
              '../css/mediaControlsChromium.css',
              '../css/mediaControlsChromiumAndroid.css',
              '../css/fullscreen.css',
              # Skip fullscreenQuickTime.
            ],
          },
          'inputs': [
            '<@(scripts)',
            '<@(stylesheets)'
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/UserAgentStyleSheets.h',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/UserAgentStyleSheetsData.cpp',
          ],
          'action': [
            'python',
            'scripts/action_useragentstylesheets.py',
            '<@(_outputs)',
            '<@(stylesheets)',
            '--',
            '<@(scripts)',
            '--',
            '--defines', '<(feature_defines)',
          ],
          'msvs_cygwin_shell': 1,
        },
        {
          'action_name': 'PickerCommon',
          'inputs': [
            '../Resources/pagepopups/pickerCommon.css',
            '../Resources/pagepopups/pickerCommon.js',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/PickerCommon.h',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/PickerCommon.cpp',
          ],
          'action': [
            'python',
            '../scripts/make-file-arrays.py',
            '--condition=ENABLE(CALENDAR_PICKER) OR ENABLE(INPUT_TYPE_COLOR)',
            '--out-h=<(SHARED_INTERMEDIATE_DIR)/webkit/PickerCommon.h',
            '--out-cpp=<(SHARED_INTERMEDIATE_DIR)/webkit/PickerCommon.cpp',
            '<@(_inputs)',
          ],
        },
        {
          'action_name': 'CalendarPicker',
          'inputs': [
            '../Resources/pagepopups/calendarPicker.css',
            '../Resources/pagepopups/calendarPicker.js',
            '../Resources/pagepopups/chromium/calendarPickerChromium.css',
            '../Resources/pagepopups/chromium/pickerCommonChromium.css',
            '../Resources/pagepopups/suggestionPicker.css',
            '../Resources/pagepopups/suggestionPicker.js',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/CalendarPicker.h',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/CalendarPicker.cpp',
          ],
          'action': [
            'python',
            '../scripts/make-file-arrays.py',
            '--condition=ENABLE(CALENDAR_PICKER)',
            '--out-h=<(SHARED_INTERMEDIATE_DIR)/webkit/CalendarPicker.h',
            '--out-cpp=<(SHARED_INTERMEDIATE_DIR)/webkit/CalendarPicker.cpp',
            '<@(_inputs)',
          ],
        },
        {
          'action_name': 'ColorSuggestionPicker',
          'inputs': [
            '../Resources/pagepopups/colorSuggestionPicker.css',
            '../Resources/pagepopups/colorSuggestionPicker.js',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/ColorSuggestionPicker.h',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/ColorSuggestionPicker.cpp',
          ],
          'action': [
            'python',
            '../scripts/make-file-arrays.py',
            '--condition=ENABLE(INPUT_TYPE_COLOR)',
            '--out-h=<(SHARED_INTERMEDIATE_DIR)/webkit/ColorSuggestionPicker.h',
            '--out-cpp=<(SHARED_INTERMEDIATE_DIR)/webkit/ColorSuggestionPicker.cpp',
            '<@(_inputs)',
          ],
        },
        {
          'action_name': 'XLinkNames',
          'inputs': [
            '../scripts/Hasher.pm',
            '../scripts/StaticString.pm',
            '../scripts/make_names.pl',
            '../svg/xlinkattrs.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/XLinkNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/XLinkNames.h',
          ],
          'action': [
            'python',
            'scripts/action_makenames.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)',
            '--',
            '--extraDefines', '<(feature_defines)'
          ],
          'msvs_cygwin_shell': 1,
        },
        {
          'action_name': 'XMLNSNames',
          'inputs': [
            '../scripts/Hasher.pm',
            '../scripts/StaticString.pm',
            '../scripts/make_names.pl',
            '../xml/xmlnsattrs.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/XMLNSNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/XMLNSNames.h',
          ],
          'action': [
            'python',
            'scripts/action_makenames.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)',
            '--',
            '--extraDefines', '<(feature_defines)'
          ],
          'msvs_cygwin_shell': 1,
        },
        {
          'action_name': 'XMLNames',
          'inputs': [
            '../scripts/Hasher.pm',
            '../scripts/StaticString.pm',
            '../scripts/make_names.pl',
            '../xml/xmlattrs.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/XMLNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/XMLNames.h',
          ],
          'action': [
            'python',
            'scripts/action_makenames.py',
            '<@(_outputs)',
            '--',
            '<@(_inputs)',
            '--',
            '--extraDefines', '<(feature_defines)'
          ],
          'msvs_cygwin_shell': 1,
        },
        {
          'action_name': 'preprocess_grammar',
          'inputs': [
            '../css/CSSGrammar.y.in',
            '../css/CSSGrammar.y.includes',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/CSSGrammar.y',
          ],
          'action': [
            '<(perl_exe)',
            '-I../scripts',
            '../css/makegrammar.pl',
            '--outputDir',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/',
            '--extraDefines',
            '<(feature_defines)',
            '--preprocessOnly',
            '<@(preprocessor)',
            '<@(_inputs)',
          ],
        },
      ],
      'rules': [
        {
          'rule_name': 'bison',
          'extension': 'y',
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/<(RULE_INPUT_ROOT).cpp',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/<(RULE_INPUT_ROOT).h'
          ],
          'action': [
            'python',
            'scripts/rule_bison.py',
            '<(RULE_INPUT_PATH)',
            '<(SHARED_INTERMEDIATE_DIR)/webkit',
            '<(bison_exe)',
          ],
          'msvs_cygwin_shell': 1,
        },
        {
          'rule_name': 'gperf',
          'extension': 'gperf',
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/webkit/<(RULE_INPUT_ROOT).cpp',
          ],
          'inputs': [
            '../scripts/make-hash-tools.pl',
          ],
          'msvs_cygwin_shell': 0,
          'action': [
            '<(perl_exe)',
            '../scripts/make-hash-tools.pl',
            '<(SHARED_INTERMEDIATE_DIR)/webkit',
            '<(RULE_INPUT_PATH)',
            '<(gperf_exe)',
          ],
        },
      ],
    },
  ],
}
