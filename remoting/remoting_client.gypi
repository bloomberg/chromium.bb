# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'remoting_client_plugin',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'defines': [
        'HAVE_STDINT_H',  # Required by on2_integer.h
      ],
      'dependencies': [
        '../net/net.gyp:net',
        '../ppapi/ppapi.gyp:ppapi_cpp_objects',
        '../third_party/webrtc/modules/modules.gyp:desktop_capture',
        '../ui/events/events.gyp:dom4_keycode_converter',
        'remoting_base',
        'remoting_client',
        'remoting_protocol',
      ],
      'sources': [
        '<@(remoting_client_plugin_sources)',
        'client/plugin/pepper_entrypoints.cc',
        'client/plugin/pepper_entrypoints.h',
      ],
      'conditions' : [
        [ 'chromeos==0', {
          'sources!': [
            'client/plugin/normalizing_input_filter_cros.cc',
          ],
        }],
      ],
    },  # end of target 'remoting_client_plugin'

    {
      'target_name': 'remoting_client',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        'remoting_base',
        'remoting_protocol',
        '../third_party/libyuv/libyuv.gyp:libyuv',
        '../third_party/webrtc/modules/modules.gyp:desktop_capture',
        '../third_party/libwebm/libwebm.gyp:libwebm',
      ],
      'sources': [
        '<@(remoting_client_sources)',
      ],
    },  # end of target 'remoting_client'

    {
      'target_name': 'remoting_webapp_html',
      'type': 'none',
      'actions': [
        {
          'action_name': 'Build Remoting Webapp main.html',
          'inputs': [
            'webapp/build-html.py',
            '<(remoting_webapp_template_main)',
            '<@(remoting_webapp_template_files)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/main.html',
          ],
          'action': [
            'python', 'webapp/build-html.py',
            '<(SHARED_INTERMEDIATE_DIR)/main.html',
            '<(remoting_webapp_template_main)',
            '--template', '<@(remoting_webapp_template_files)',
            '--js', '<@(remoting_webapp_main_html_js_files)',
          ],
        },
        {
          'action_name': 'Build Remoting Webapp wcs_sandbox.html',
          'inputs': [
            'webapp/build-html.py',
            '<(remoting_webapp_template_wcs_sandbox)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/wcs_sandbox.html',
          ],
          'action': [
            'python', 'webapp/build-html.py',
            '<(SHARED_INTERMEDIATE_DIR)/wcs_sandbox.html',
            '<(remoting_webapp_template_wcs_sandbox)',
            '--js', '<@(remoting_webapp_wcs_sandbox_html_js_files)',
          ],
        },
      ],
    },  # end of target 'remoting_webapp_html'

    {
      'target_name': 'remoting_webapp',
      'type': 'none',
      'dependencies': [
        'remoting_webapp_v1',
      ],
      'conditions': [
        ['OS != "win"', {
          'dependencies': [
            'remoting_webapp_v2',
          ],
        }],
      ],
    },  # end of target 'remoting_webapp'

    {
      'target_name': 'remoting_webapp_v1',
      'type': 'none',
      'variables': {
        'include_host_plugin': '<(enable_remoting_host)',
        'output_dir': '<(PRODUCT_DIR)/remoting/remoting.webapp',
        'zip_path': '<(PRODUCT_DIR)/remoting-webapp.zip',
      },
      'includes': [ 'remoting_webapp.gypi', ],
    },  # end of target 'remoting_webapp_v1'

  ],  # end of targets

  'conditions': [
    ['OS != "win"', {
      'targets': [
        {
          'target_name': 'remoting_webapp_v2',
          'type': 'none',
          'variables': {
            'output_dir': '<(PRODUCT_DIR)/remoting/remoting.webapp.v2',
            'zip_path': '<(PRODUCT_DIR)/remoting-webapp.v2.zip',
            'patch_files': [ 'webapp/appsv2.patch' ],
            'extra_files': [ 'webapp/background.js' ],
          },
          'includes': [ 'remoting_webapp.gypi', ],
        },  # end of target 'remoting_webapp_v2'
      ],
    }],
  ],
}
