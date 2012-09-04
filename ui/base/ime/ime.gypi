# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'ime_files': [
      'character_composer.cc',
      'character_composer.h',
      'composition_text.cc',
      'composition_text.h',
      'composition_underline.h',
      'ibus_client.cc',
      'ibus_client.h',
      'input_method.h',
      'input_method_base.cc',
      'input_method_base.h',
      'input_method_delegate.h',
      'input_method_factory.cc',
      'input_method_factory.h',
      'input_method_ibus.cc',
      'input_method_ibus.h',
      'mock_input_method.cc',
      'mock_input_method.h',
      'text_input_client.cc',
      'text_input_client.h',
      'text_input_type.h',
    ],
  },
  'sources': [
    '<@(ime_files)',
  ],
  'conditions': [
    ['use_aura==0', {
      'sources!': [
        '<@(ime_files)',
      ],
      'sources/': [
        # gtk_im_context_util* use ui::CompositionText.
        ['include', 'composition_text\\.(cc|h)$'],
        # native_textfield_views* use ui::TextInputClient.
        ['include', 'text_input_client\\.(cc|h)$'],
      ],
    }],
    ['chromeos==0', {
      'sources!': [
        'character_composer.cc',
        'character_composer.h',
        'ibus_client.cc',
        'ibus_client.h',
        'input_method_ibus.cc',
        'input_method_ibus.h',
      ],
    }, {
      'dependencies': [
        '<(DEPTH)/chromeos/chromeos.gyp:chromeos',
      ],
    }],
  ],
}
