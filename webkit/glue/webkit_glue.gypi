# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
     # Disable narrowing-conversion-in-initialization-list warnings in that we
     # do not want to fix it in data file "webcursor_gtk_data.h".
     'cflags+': ['-Wno-narrowing'],
     'cflags_cc+': ['-Wno-narrowing'],
  },
  'targets': [
    {
      'target_name': 'webkit_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/webkit',
      },
      'actions': [
        {
          'action_name': 'webkit_resources',
          'variables': {
            'grit_grd_file': 'resources/webkit_resources.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
        {
          'action_name': 'webkit_chromium_resources',
          'variables': {
            'grit_grd_file': '../../third_party/WebKit/Source/WebKit/chromium/WebKit.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../build/grit_target.gypi' ],
      'direct_dependent_settings': {
        'include_dirs': [ '<(grit_out_dir)' ],
      },
    },
    {
      'target_name': 'webkit_strings',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/webkit',
      },
      'actions': [
        {
          'action_name': 'webkit_strings',
          'variables': {
            'grit_grd_file': 'webkit_strings.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../build/grit_target.gypi' ],
    },

    {
      'target_name': 'glue_child',
      'type': '<(component)',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'defines': [
        'WEBKIT_CHILD_IMPLEMENTATION',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/WebKit/public/blink.gyp:blink',
        '<(DEPTH)/ui/native_theme/native_theme.gyp:native_theme',
        '<(DEPTH)/ui/ui.gyp:ui',

        # TODO(scottmg): crbug.com/237249
        'glue',
      ],

      'include_dirs': [
        # For JNI generated header.
        '<(SHARED_INTERMEDIATE_DIR)/webkit',
      ],

      'sources': [
        '../child/fling_animator_impl_android.cc',
        '../child/fling_animator_impl_android.h',
        '../child/fling_curve_configuration.cc',
        '../child/fling_curve_configuration.h',
        '../child/touch_fling_gesture_curve.cc',
        '../child/touch_fling_gesture_curve.h',
        '../child/web_discardable_memory_impl.cc',
        '../child/web_discardable_memory_impl.h',
        '../child/webfallbackthemeengine_impl.cc',
        '../child/webfallbackthemeengine_impl.h',
        '../child/webkit_child_export.h',
        '../child/webkitplatformsupport_child_impl.cc',
        '../child/webkitplatformsupport_child_impl.h',
        '../child/webthemeengine_impl_android.cc',
        '../child/webthemeengine_impl_android.h',
        '../child/webthemeengine_impl_default.cc',
        '../child/webthemeengine_impl_default.h',
        '../child/webthemeengine_impl_mac.cc',
        '../child/webthemeengine_impl_mac.h',
        '../child/webthemeengine_impl_win.cc',
        '../child/webthemeengine_impl_win.h',
        '../child/webthread_impl.cc',
        '../child/webthread_impl.h',
        '../child/worker_task_runner.cc',
        '../child/worker_task_runner.h',
      ],

      'conditions': [
        ['use_default_render_theme==0', {
          'sources/': [
            ['exclude', 'webthemeengine_impl_default.cc'],
            ['exclude', 'webthemeengine_impl_default.h'],
          ],
        }],
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
            ],
          },
        }],
        ['OS=="android"', {
          'dependencies': [
            'overscroller_jni_headers',
          ],
        }],
      ],
    },

    {
      'target_name': 'glue_common',
      'type': '<(component)',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'defines': [
        'WEBKIT_COMMON_IMPLEMENTATION',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/WebKit/public/blink.gyp:blink',
        '<(DEPTH)/ui/ui.gyp:ui',
        '<(DEPTH)/ui/ui.gyp:ui_resources',
        '<(DEPTH)/url/url.gyp:url_lib',
      ],

      'sources': [
        '../common/webdropdata.cc',
        '../common/webdropdata.h',
        '../common/webkit_common_export.h',
        '../common/webmenuitem.cc',
        '../common/webmenuitem.h',
        '../common/webpreferences.cc',
        '../common/webpreferences.h',
        'multipart_response_delegate.cc',
        'multipart_response_delegate.h',
        'weburlrequest_extradata_impl.cc',
        'weburlrequest_extradata_impl.h',
        'weburlresponse_extradata_impl.cc',
        'weburlresponse_extradata_impl.h',
      ],

      'conditions': [
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '<(DEPTH)/build/linux/system.gyp:gtk',
          ],
          'sources/': [['exclude', '_x11\\.cc$']],
        }],
        ['OS!="mac"', {
          'sources/': [['exclude', '_mac\\.(cc|mm)$']],
        }, {  # else: OS=="mac"
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
            ],
          },
        }],
        ['OS!="win"', {
          'sources/': [['exclude', '_win\\.cc$']],
        }, {  # else: OS=="win"
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4800, 4267 ],
          'sources/': [['exclude', '_posix\\.cc$']],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
        }],
      ],
    },

    {
      'target_name': 'glue_renderer',
      'type': '<(component)',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'defines': [
        'WEBKIT_RENDERER_IMPLEMENTATION',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/WebKit/public/blink.gyp:blink',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
        '<(DEPTH)/ui/ui.gyp:ui',
        '<(DEPTH)/url/url.gyp:url_lib',
        'glue_common',
        'webkit_common',
      ],

      'sources': [
        '../renderer/cpp_bound_class.cc',
        '../renderer/cpp_bound_class.h',
        '../renderer/cpp_variant.cc',
        '../renderer/cpp_variant.h',
        '../renderer/clipboard_utils.cc',
        '../renderer/clipboard_utils.h',
        '../renderer/cursor_utils.cc',
        '../renderer/cursor_utils.h',
        '../renderer/webkit_renderer_export.h',
        '../renderer/webpreferences_renderer.cc',
        '../renderer/webpreferences_renderer.h',
      ],
    },

    {
      'target_name': 'glue',
      'type': '<(component)',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'defines': [
        'WEBKIT_EXTENSIONS_IMPLEMENTATION',
        'WEBKIT_GLUE_IMPLEMENTATION',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/base/base.gyp:base_static',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/gpu/gpu.gyp:gles2_c_lib',
        '<(DEPTH)/gpu/gpu.gyp:gles2_implementation',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/printing/printing.gyp:printing',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/WebKit/public/blink.gyp:blink',
        '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
        '<(DEPTH)/third_party/npapi/npapi.gyp:npapi',
        '<(DEPTH)/ui/gl/gl.gyp:gl',
        '<(DEPTH)/ui/ui.gyp:ui',
        '<(DEPTH)/ui/ui.gyp:ui_resources',
        '<(DEPTH)/url/url.gyp:url_lib',
        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
        '<(DEPTH)/webkit/base/webkit_base.gyp:webkit_base',
        '<(DEPTH)/webkit/common/user_agent/webkit_user_agent.gyp:user_agent',
        '<(DEPTH)/webkit/plugins/webkit_plugins.gyp:plugins_common',
        '<(DEPTH)/webkit/renderer/compositor_bindings/compositor_bindings.gyp:webkit_compositor_support',
        '<(DEPTH)/webkit/storage_browser.gyp:webkit_storage_browser',
        '<(DEPTH)/webkit/storage_common.gyp:webkit_storage_common',
        'glue_common',
        'plugins',
        'webkit_common',
        'webkit_media',
        'webkit_resources',
        'webkit_strings',
      ],
      'include_dirs': [
        '<(INTERMEDIATE_DIR)',
        '<(SHARED_INTERMEDIATE_DIR)/webkit',
        '<(SHARED_INTERMEDIATE_DIR)/ui',
      ],
      'sources': [
        'ftp_directory_listing_response_delegate.cc',
        'ftp_directory_listing_response_delegate.h',
        'glue_serialize_deprecated.cc',
        'glue_serialize_deprecated.h',
        'image_decoder.cc',
        'image_decoder.h',
        'network_list_observer.h',
        'resource_loader_bridge.cc',
        'resource_loader_bridge.h',
        'resource_request_body.cc',
        'resource_request_body.h',
        'resource_type.cc',
        'resource_type.h',
        'simple_webmimeregistry_impl.cc',
        'simple_webmimeregistry_impl.h',
        'webfileutilities_impl.cc',
        'webfileutilities_impl.h',
        'webkit_glue.cc',
        'webkit_glue.h',
        'webkit_glue_export.h',
        'webkitplatformsupport_impl.cc',
        'webkitplatformsupport_impl.h',
        'websocketstreamhandle_bridge.h',
        'websocketstreamhandle_delegate.h',
        'websocketstreamhandle_impl.cc',
        'websocketstreamhandle_impl.h',
        'weburlloader_impl.cc',
        'weburlloader_impl.h',
      ],
      # When glue is a dependency, it needs to be a hard dependency.
      # Dependents may rely on files generated by this target or one of its
      # own hard dependencies.
      'hard_dependency': 1,
      'conditions': [
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '<(DEPTH)/build/linux/system.gyp:gtk',
          ],
          'sources/': [['exclude', '_x11\\.cc$']],
        }],
        ['use_aura==1 and use_x11==1', {
          'link_settings': {
            'libraries': [ '-lXcursor', ],
          },
        }],
        ['OS=="win"', {
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4800, 4267 ],
          'conditions': [
            ['component=="shared_library"', {
              'dependencies': [
                 '<(DEPTH)/third_party/WebKit/public/blink.gyp:blink',
               ],
               'export_dependent_settings': [
                 '<(DEPTH)/third_party/WebKit/public/blink.gyp:blink',
                 '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
               ],
            }],
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['use_third_party_translations==1', {
      'targets': [
        {
          'target_name': 'inspector_strings',
          'type': 'none',
          'variables': {
            'grit_out_dir': '<(PRODUCT_DIR)/resources/inspector/l10n',
          },
          'actions': [
            {
              'action_name': 'inspector_strings',
              'variables': {
                'grit_grd_file': 'inspector_strings.grd',
              },
              'includes': [ '../../build/grit_action.gypi' ],
            },
          ],
          'includes': [ '../../build/grit_target.gypi' ],
        },
      ],
    }],
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'overscroller_jni_headers',
          'type': 'none',
          'variables': {
            'jni_gen_package': 'webkit',
            'input_java_class': 'android/widget/OverScroller.class',
          },
          'includes': [ '../../build/jar_file_jni_generator.gypi' ],
        },
      ],
    }],
  ],
}
