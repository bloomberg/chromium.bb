# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'forms',
      'type': '<(component)',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'defines': ['WEBKIT_FORMS_IMPLEMENTATION'],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/build/temp_gyp/googleurl.gyp:googleurl',
        '<(webkit_src_dir)/Source/WebKit/chromium/WebKit.gyp:webkit',
      ],
      'sources': [
        'form_data.cc',
        'form_data.h',
        'form_data_predictions.cc',
        'form_data_predictions.h',
        'form_field.cc',
        'form_field.h',
        'form_field_predictions.cc',
        'form_field_predictions.h',
        'password_form.cc',
        'password_form.h',
        'password_form_dom_manager.cc',
        'password_form_dom_manager.h',
        'webkit_forms_export.h',
      ],
      'conditions': [
        ['inside_chromium_build==0', {
          'dependencies': [
            '<(DEPTH)/webkit/support/setup_third_party.gyp:third_party_headers',
          ],
        }],
      ],
    },
  ],
}
