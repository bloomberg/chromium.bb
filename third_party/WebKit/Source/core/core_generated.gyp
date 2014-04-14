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
    'core.gypi',
    '../bindings/bindings.gypi',
    '../build/features.gypi',
    '../build/scripts/scripts.gypi',
  ],

  'targets': [
    {
      'target_name': 'generated_testing_idls',
      'type': 'none',
      'actions': [
        {
          'action_name': 'Settings',
          'inputs': [
            '<@(scripts_for_in_files)',
            '../build/scripts/make_settings.py',
            '../build/scripts/templates/InternalSettingsGenerated.idl.tmpl',
            '../build/scripts/templates/InternalSettingsGenerated.cpp.tmpl',
            '../build/scripts/templates/InternalSettingsGenerated.h.tmpl',
            '../build/scripts/templates/SettingsMacros.h.tmpl',
            'frame/Settings.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/SettingsMacros.h',
            '<(SHARED_INTERMEDIATE_DIR)/blink/InternalSettingsGenerated.idl',
            '<(SHARED_INTERMEDIATE_DIR)/blink/InternalSettingsGenerated.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/InternalSettingsGenerated.h',
          ],
          'action': [
            'python',
            '../build/scripts/make_settings.py',
            'frame/Settings.in',
            '--output_dir',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
          ],
        },
        {
          'action_name': 'InternalRuntimeFlags',
          'inputs': [
            '<@(scripts_for_in_files)',
            '../build/scripts/make_internal_runtime_flags.py',
            '../platform/RuntimeEnabledFeatures.in',
            '../build/scripts/templates/InternalRuntimeFlags.h.tmpl',
            '../build/scripts/templates/InternalRuntimeFlags.idl.tmpl',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/InternalRuntimeFlags.idl',
            '<(SHARED_INTERMEDIATE_DIR)/blink/InternalRuntimeFlags.h',
          ],
          'action': [
            'python',
            '../build/scripts/make_internal_runtime_flags.py',
            '../platform/RuntimeEnabledFeatures.in',
            '--output_dir',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
          ],
        },
      ]
    },
    {
      'target_name': 'make_core_generated',
      'type': 'none',
      'hard_dependency': 1,
      'dependencies': [
        'generated_testing_idls',
        '../bindings/core_bindings_generated.gyp:core_bindings_generated',
        '../config.gyp:config',
      ],
      'sources': [
        # bison rule
        'css/CSSGrammar.y',
        'xml/XPathGrammar.y',
      ],
      'actions': [
        {
          'action_name': 'generateXMLViewerCSS',
          'inputs': [
            'xml/XMLViewer.css',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/XMLViewerCSS.h',
          ],
          'action': [
            'python',
            '../build/scripts/xxd.py',
            'XMLViewer_css',
            '<@(_inputs)',
            '<@(_outputs)'
          ],
        },
        {
          'action_name': 'generateXMLViewerJS',
          'inputs': [
            'xml/XMLViewer.js',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/XMLViewerJS.h',
          ],
          'action': [
            'python',
            '../build/scripts/xxd.py',
            'XMLViewer_js',
            '<@(_inputs)',
            '<@(_outputs)'
          ],
        },
        {
          'action_name': 'HTMLEntityTable',
          'inputs': [
            'html/parser/create-html-entity-table',
            'html/parser/HTMLEntityNames.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/HTMLEntityTable.cpp'
          ],
          'action': [
            'python',
            'html/parser/create-html-entity-table',
            '-o',
            '<@(_outputs)',
            '<@(_inputs)'
          ],
        },
        {
          'action_name': 'CSSPropertyNames',
          'variables': {
            'in_files': [
              'css/CSSPropertyNames.in',
              'css/SVGCSSPropertyNames.in',
            ],
          },
          'inputs': [
            '<@(scripts_for_in_files)',
            '../build/scripts/make_css_property_names.py',
            '<@(in_files)'
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/CSSPropertyNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/CSSPropertyNames.h',
          ],
          'action': [
            'python',
            '../build/scripts/make_css_property_names.py',
            '<@(in_files)',
            '--output_dir',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
            '--gperf', '<(gperf_exe)',
            '--defines', '<(feature_defines)',
          ],
        },
        {
          'action_name': 'MediaFeatureNames',
          'variables': {
            'in_files': [
              'css/MediaFeatureNames.in',
            ],
          },
          'inputs': [
            '<@(make_names_files)',
            '../build/scripts/make_media_feature_names.py',
            '<@(in_files)'
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/MediaFeatureNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/MediaFeatureNames.h',
          ],
          'action': [
            'python',
            '../build/scripts/make_media_feature_names.py',
            '<@(in_files)',
            '--output_dir',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
            '--defines', '<(feature_defines)',
          ],
        },
        {
          'action_name': 'MediaFeatures',
          'variables': {
            'in_files': [
              'css/MediaFeatureNames.in',
            ],
          },
          'inputs': [
            '<@(scripts_for_in_files)',
            '../build/scripts/make_media_features.py',
            '../build/scripts/templates/MediaFeatures.h.tmpl',
            '<@(in_files)'
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/MediaFeatures.h',
          ],
          'action': [
            'python',
            '../build/scripts/make_media_features.py',
            '<@(in_files)',
            '--output_dir',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
            '--defines', '<(feature_defines)',
          ],
        },
        {
          'action_name': 'MediaTypeNames',
          'variables': {
            'in_files': [
              'css/MediaTypeNames.in',
            ],
          },
          'inputs': [
            '<@(make_names_files)',
            '<@(in_files)'
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/MediaTypeNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/MediaTypeNames.h',
          ],
          'action': [
            'python',
            '../build/scripts/make_names.py',
            '<@(in_files)',
            '--output_dir',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
            '--defines', '<(feature_defines)',
          ],
        },
        {
          'action_name': 'MediaQueryTokenizerCodepoints',
          'inputs': [
            '../build/scripts/make_mediaquery_tokenizer_codepoints.py',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/MediaQueryTokenizerCodepoints.cpp',
          ],
          'action': [
            'python',
            '../build/scripts/make_mediaquery_tokenizer_codepoints.py',
            '--output_dir',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
            '--defines', '<(feature_defines)',
          ],
        },
        {
          'action_name': 'StylePropertyShorthand',
          'inputs': [
            '<@(scripts_for_in_files)',
            '../build/scripts/make_style_shorthands.py',
            '../build/scripts/templates/StylePropertyShorthand.cpp.tmpl',
            '../build/scripts/templates/StylePropertyShorthand.h.tmpl',
            'css/CSSShorthands.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/StylePropertyShorthand.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/StylePropertyShorthand.h',
          ],
          'action': [
            'python',
            '../build/scripts/make_style_shorthands.py',
            'css/CSSShorthands.in',
            '--output_dir',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
          ],
        },
        {
          'action_name': 'StyleBuilder',
          'inputs': [
            '<@(scripts_for_in_files)',
            '../build/scripts/make_style_builder.py',
            '../build/scripts/templates/StyleBuilder.cpp.tmpl',
            '../build/scripts/templates/StyleBuilderFunctions.cpp.tmpl',
            '../build/scripts/templates/StyleBuilderFunctions.h.tmpl',
            'css/CSSProperties.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/StyleBuilder.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/StyleBuilderFunctions.h',
            '<(SHARED_INTERMEDIATE_DIR)/blink/StyleBuilderFunctions.cpp',
          ],
          'action': [
            'python',
            '../build/scripts/make_style_builder.py',
            'css/CSSProperties.in',
            '--output_dir',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
          ],
        },
        {
          'action_name': 'CSSValueKeywords',
          'variables': {
            'in_files': [
              'css/CSSValueKeywords.in',
              'css/SVGCSSValueKeywords.in',
            ],
          },
          'inputs': [
            '<@(scripts_for_in_files)',
            '../build/scripts/make_css_value_keywords.py',
            '<@(in_files)'
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/CSSValueKeywords.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/CSSValueKeywords.h',
          ],
          'action': [
             'python',
             '../build/scripts/make_css_value_keywords.py',
             '<@(in_files)',
             '--output_dir',
             '<(SHARED_INTERMEDIATE_DIR)/blink',
            '--gperf', '<(gperf_exe)',
            '--defines', '<(feature_defines)',
          ],
        },
        {
          'action_name': 'HTMLElementFactory',
          'inputs': [
            '<@(make_element_factory_files)',
            'html/HTMLTagNames.in',
            'html/HTMLAttributeNames.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/HTMLElementFactory.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/HTMLElementFactory.h',
            '<(SHARED_INTERMEDIATE_DIR)/blink/HTMLNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/HTMLNames.h',
            '<(SHARED_INTERMEDIATE_DIR)/blink/V8HTMLElementWrapperFactory.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/V8HTMLElementWrapperFactory.h',
          ],
          'action': [
            'python',
            '../build/scripts/make_element_factory.py',
            'html/HTMLTagNames.in',
            'html/HTMLAttributeNames.in',
            '--output_dir',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
          ],
        },
        {
          'action_name': 'HTMLElementTypeHelpers',
          'inputs': [
            '<@(make_element_type_helpers_files)',
            'html/HTMLTagNames.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/HTMLElementTypeHelpers.h',
          ],
          'action': [
            'python',
            '../build/scripts/make_element_type_helpers.py',
            'html/HTMLTagNames.in',
            '--output_dir',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
          ],
        },
        {
          'action_name': 'SVGNames',
          'inputs': [
            '<@(make_element_factory_files)',
            'svg/SVGTagNames.in',
            'svg/SVGAttributeNames.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/SVGElementFactory.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/SVGElementFactory.h',
            '<(SHARED_INTERMEDIATE_DIR)/blink/SVGNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/SVGNames.h',
            '<(SHARED_INTERMEDIATE_DIR)/blink/V8SVGElementWrapperFactory.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/V8SVGElementWrapperFactory.h',
          ],
          'action': [
            'python',
            '../build/scripts/make_element_factory.py',
            'svg/SVGTagNames.in',
            'svg/SVGAttributeNames.in',
            '--output_dir',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
          ],
        },
        {
          'action_name': 'SVGElementTypeHelpers',
          'inputs': [
            '<@(make_element_type_helpers_files)',
            'svg/SVGTagNames.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/SVGElementTypeHelpers.h',
          ],
          'action': [
            'python',
            '../build/scripts/make_element_type_helpers.py',
            'svg/SVGTagNames.in',
            '--output_dir',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
          ],
        },
        {
          'action_name': 'EventFactory',
          'inputs': [
            '<@(make_event_factory_files)',
            '<(SHARED_INTERMEDIATE_DIR)/blink/EventInterfaces.in',
            'events/EventAliases.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/Event.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/EventHeaders.h',
            '<(SHARED_INTERMEDIATE_DIR)/blink/EventInterfaces.h',
          ],
          'action': [
            'python',
            '../build/scripts/make_event_factory.py',
            '<(SHARED_INTERMEDIATE_DIR)/blink/EventInterfaces.in',
            'events/EventAliases.in',
            '--output_dir',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
          ],
        },
        {
          'action_name': 'EventNames',
          'inputs': [
            '<@(make_names_files)',
            '<(SHARED_INTERMEDIATE_DIR)/blink/EventInterfaces.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/EventNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/EventNames.h',
          ],
          'action': [
            'python',
            '../build/scripts/make_names.py',
            '<(SHARED_INTERMEDIATE_DIR)/blink/EventInterfaces.in',
            '--output_dir',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
          ],
        },
        {
          'action_name': 'EventTargetFactory',
          'inputs': [
            '<@(make_event_factory_files)',
            'events/EventTargetFactory.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/EventTargetHeaders.h',
            '<(SHARED_INTERMEDIATE_DIR)/blink/EventTargetInterfaces.h',
          ],
          'action': [
            'python',
            '../build/scripts/make_event_factory.py',
            'events/EventTargetFactory.in',
            '--output_dir',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
          ],
        },
        {
          'action_name': 'EventTargetNames',
          'inputs': [
            '<@(make_names_files)',
            'events/EventTargetFactory.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/EventTargetNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/EventTargetNames.h',
          ],
          'action': [
            'python',
            '../build/scripts/make_names.py',
            'events/EventTargetFactory.in',
            '--output_dir',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
          ],
        },
        {
          'action_name': 'MathMLNames',
          'inputs': [
            '<@(make_qualified_names_files)',
            'html/parser/MathMLTagNames.in',
            'html/parser/MathMLAttributeNames.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/MathMLNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/MathMLNames.h',
          ],
          'action': [
            'python',
            '../build/scripts/make_qualified_names.py',
            'html/parser/MathMLTagNames.in',
            'html/parser/MathMLAttributeNames.in',
            '--output_dir',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
            '--defines', '<(feature_defines)'
          ],
        },
        {
          'action_name': 'UserAgentStyleSheets',
          'variables': {
            'scripts': [
              'css/make-css-file-arrays.pl',
              '../build/scripts/preprocessor.pm',
            ],
            'stylesheets': [
              'css/html.css',
              'css/quirks.css',
              'css/view-source.css',
              'css/themeChromium.css',
              'css/themeChromiumAndroid.css',
              'css/themeChromiumLinux.css',
              'css/themeChromiumSkia.css',
              'css/themeMac.css',
              'css/themeWin.css',
              'css/themeWinQuirks.css',
              'css/svg.css',
              'css/mathml.css',
              'css/mediaControls.css',
              'css/mediaControlsAndroid.css',
              'css/fullscreen.css',
              'css/xhtmlmp.css',
              'css/viewportAndroid.css',
            ],
          },
          'inputs': [
            '<@(scripts)',
            '<@(stylesheets)'
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/UserAgentStyleSheets.h',
            '<(SHARED_INTERMEDIATE_DIR)/blink/UserAgentStyleSheetsData.cpp',
          ],
          'action': [
            'python',
            '../build/scripts/action_useragentstylesheets.py',
            '<@(_outputs)',
            '<@(stylesheets)',
            '--',
            '<@(scripts)',
            '--',
            '--defines', '<(feature_defines)',
            '<@(preprocessor)',
            '--perl', '<(perl_exe)',
          ],
        },
        {
          'action_name': 'FetchInitiatorTypeNames',
          'inputs': [
            '<@(make_names_files)',
            'fetch/FetchInitiatorTypeNames.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/FetchInitiatorTypeNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/FetchInitiatorTypeNames.h',
          ],
          'action': [
            'python',
            '../build/scripts/make_names.py',
            'fetch/FetchInitiatorTypeNames.in',
            '--output_dir',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
          ],
        },
        {
          'action_name': 'EventTypeNames',
          'inputs': [
            '<@(make_names_files)',
            'events/EventTypeNames.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/EventTypeNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/EventTypeNames.h',
          ],
          'action': [
            'python',
            '../build/scripts/make_names.py',
            'events/EventTypeNames.in',
            '--output_dir',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
          ],
        },
        {
          'action_name': 'InputTypeNames',
          'inputs': [
            '<@(make_names_files)',
            'html/forms/InputTypeNames.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/InputTypeNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/InputTypeNames.h',
          ],
          'action': [
            'python',
            '../build/scripts/make_names.py',
            'html/forms/InputTypeNames.in',
            '--output_dir',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
          ],
        },
        {
          'action_name': 'XLinkNames',
          'inputs': [
            '<@(make_qualified_names_files)',
            'svg/xlinkattrs.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/XLinkNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/XLinkNames.h',
          ],
          'action': [
            'python',
            '../build/scripts/make_qualified_names.py',
            'svg/xlinkattrs.in',
            '--output_dir',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
          ],
        },
        {
          'action_name': 'XMLNSNames',
          'inputs': [
            '<@(make_qualified_names_files)',
            'xml/xmlnsattrs.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/XMLNSNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/XMLNSNames.h',
          ],
          'action': [
            'python',
            '../build/scripts/make_qualified_names.py',
            'xml/xmlnsattrs.in',
            '--output_dir',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
          ],
        },
        {
          'action_name': 'XMLNames',
          'inputs': [
            '<@(make_qualified_names_files)',
            'xml/xmlattrs.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/XMLNames.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/XMLNames.h',
          ],
          'action': [
            'python',
            '../build/scripts/make_qualified_names.py',
            'xml/xmlattrs.in',
            '--output_dir',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
          ],
        },
        {
          'action_name': 'MakeTokenMatcher',
          'inputs': [
            '<@(scripts_for_in_files)',
            '../build/scripts/make_token_matcher.py',
            '../core/css/CSSTokenizer-in.cpp',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/CSSTokenizer.cpp',
          ],
          'action': [
            'python',
            '../build/scripts/make_token_matcher.py',
            '../core/css/CSSTokenizer-in.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/CSSTokenizer.cpp',
          ],
        },
        {
          'action_name': 'MakeParser',
          'inputs': [
            '<@(scripts_for_in_files)',
            '../build/scripts/make_token_matcher.py',
            '../core/css/parser/BisonCSSParser-in.cpp',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/BisonCSSParser.cpp',
          ],
          'action': [
            'python',
            '../build/scripts/make_token_matcher.py',
            '../core/css/parser/BisonCSSParser-in.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/BisonCSSParser.cpp',
          ],
        },
        {
          'action_name': 'MakeTokenMatcherForViewport',
          'inputs': [
            '<@(scripts_for_in_files)',
            '../build/scripts/make_token_matcher.py',
            '../core/html/HTMLMetaElement-in.cpp',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/HTMLMetaElement.cpp',
          ],
          'action': [
            'python',
            '../build/scripts/make_token_matcher.py',
            '../core/html/HTMLMetaElement-in.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/HTMLMetaElement.cpp',
          ],
        },
        {
          'action_name': 'HTMLElementLookupTrie',
          'inputs': [
            '<@(scripts_for_in_files)',
            '../build/scripts/make_element_lookup_trie.py',
            '../build/scripts/templates/ElementLookupTrie.cpp.tmpl',
            '../build/scripts/templates/ElementLookupTrie.h.tmpl',
            'html/HTMLTagNames.in',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/HTMLElementLookupTrie.cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/HTMLElementLookupTrie.h',
          ],
          'action': [
            'python',
            '../build/scripts/make_element_lookup_trie.py',
            'html/HTMLTagNames.in',
            '--output_dir',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
          ],
        },
      ],
      'rules': [
        {
          'rule_name': 'bison',
          'extension': 'y',
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/blink/<(RULE_INPUT_ROOT).cpp',
            '<(SHARED_INTERMEDIATE_DIR)/blink/<(RULE_INPUT_ROOT).h'
          ],
          'action': [
            'python',
            '../build/scripts/rule_bison.py',
            '<(RULE_INPUT_PATH)',
            '<(SHARED_INTERMEDIATE_DIR)/blink',
            '<(bison_exe)',
          ],
        },
      ],
    },
  ],
}
