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
      'dummy_input_method_delegate.cc',
      'dummy_input_method_delegate.h',
      'input_method.h',
      'input_method_base.cc',
      'input_method_base.h',
      'input_method_delegate.h',
      'input_method_factory.cc',
      'input_method_factory.h',
      'input_method_ibus.cc',
      'input_method_ibus.h',
      'input_method_imm32.cc',
      'input_method_imm32.h',
      'input_method_initializer.h',
      'input_method_initializer.cc',
      'input_method_observer.h',
      'input_method_tsf.cc',
      'input_method_tsf.h',
      'input_method_win.cc',
      'input_method_win.h',
      'mock_input_method.cc',
      'mock_input_method.h',
      'fake_input_method.cc',
      'fake_input_method.h',
      'text_input_client.cc',
      'text_input_client.h',
      'text_input_type.h',
    ],
    'win_ime_files': [
      'win/imm32_manager.cc',
      'win/imm32_manager.h',
      'win/tsf_bridge.cc',
      'win/tsf_bridge.h',
      'win/tsf_event_router.cc',
      'win/tsf_event_router.h',
      'win/tsf_input_scope.cc',
      'win/tsf_input_scope.h',
      'win/tsf_text_store.cc',
      'win/tsf_text_store.h',
    ],
  },
  'sources': [
    '<@(ime_files)',
    '<@(win_ime_files)',
  ],
  'conditions': [
    ['use_aura==0 and OS!="win"', {
      'sources!': [
        '<@(ime_files)',
      ],
      'sources/': [
        # gtk_im_context_util* use ui::CompositionText.
        ['include', 'composition_text\\.(cc|h)$'],
        # Initializer code is platform neutral.
        ['include', 'input_method_initializer\\.(cc|h)$'],
        # native_textfield_views* use ui::TextInputClient.
        ['include', 'text_input_client\\.(cc|h)$'],
      ],
    }],
    ['chromeos==0 or use_x11==0', {
      'sources!': [
        'character_composer.cc',
        'character_composer.h',
        'input_method_ibus.cc',
        'input_method_ibus.h',
      ],
    }],
    ['chromeos==1', {
      'dependencies': [
        '<(DEPTH)/chromeos/chromeos.gyp:chromeos',
      ],
    }],
    ['OS!="win"', {
      'sources!': [
        '<@(win_ime_files)',
        'input_method_imm32.cc',
        'input_method_imm32.h',
        'input_method_tsf.cc',
        'input_method_tsf.h',
      ],
    }],
  ],
}
