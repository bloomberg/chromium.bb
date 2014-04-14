# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'dom4_keycode_converter',
      'type': 'static_library',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
      ],
      'sources': [
        'keycodes/dom4/keycode_converter.cc',
        'keycodes/dom4/keycode_converter.h',
        'keycodes/dom4/keycode_converter_data.h',
      ],
    },
    {
      'target_name': 'events_base',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../gfx/gfx.gyp:gfx_geometry',
        'dom4_keycode_converter',
      ],
      'defines': [
        'EVENTS_BASE_IMPLEMENTATION',
      ],
      'sources': [
        'event_constants.h',
        'event_switches.cc',
        'event_switches.h',
        'events_base_export.h',
        'gesture_event_details.cc',
        'gesture_event_details.h',
        'keycodes/keyboard_code_conversion.cc',
        'keycodes/keyboard_code_conversion.h',
        'keycodes/keyboard_code_conversion_android.cc',
        'keycodes/keyboard_code_conversion_android.h',
        'keycodes/keyboard_code_conversion_mac.h',
        'keycodes/keyboard_code_conversion_mac.mm',
        'keycodes/keyboard_code_conversion_win.cc',
        'keycodes/keyboard_code_conversion_win.h',
        'keycodes/keyboard_code_conversion_x.cc',
        'keycodes/keyboard_code_conversion_x.h',
        'keycodes/keyboard_codes.h',
        'latency_info.cc',
        'latency_info.h',
        'x/device_data_manager.cc',
        'x/device_data_manager.h',
        'x/device_list_cache_x.cc',
        'x/device_list_cache_x.h',
        'x/touch_factory_x11.cc',
        'x/touch_factory_x11.h',
      ],
      'conditions': [
        ['use_x11==1', {
          'dependencies': [
            '<(DEPTH)/build/linux/system.gyp:x11',
          ],
        }],
      ],
    },
    {
      'target_name': 'events',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/skia/skia.gyp:skia',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
        'events_base',
      ],
      'defines': [
        'EVENTS_IMPLEMENTATION',
      ],
      'sources': [
        'cocoa/cocoa_event_utils.h',
        'cocoa/cocoa_event_utils.mm',
        'event.cc',
        'event.h',
        'event_dispatcher.cc',
        'event_dispatcher.h',
        'event_handler.cc',
        'event_handler.h',
        'event_processor.cc',
        'event_processor.h',
        'event_rewriter.h',
        'event_source.cc',
        'event_source.h',
        'event_target.cc',
        'event_target.h',
        'event_target_iterator.h',
        'event_targeter.cc',
        'event_targeter.h',
        'event_utils.cc',
        'event_utils.h',
        'events_export.h',
        'events_stub.cc',
        'gestures/gesture_configuration.cc',
        'gestures/gesture_configuration.h',
        'gestures/gesture_point.cc',
        'gestures/gesture_point.h',
        'gestures/gesture_recognizer.h',
        'gestures/gesture_recognizer_impl.cc',
        'gestures/gesture_recognizer_impl.h',
        'gestures/gesture_sequence.cc',
        'gestures/gesture_sequence.h',
        'gestures/gesture_types.h',
        'gestures/velocity_calculator.cc',
        'gestures/velocity_calculator.h',
        'ozone/evdev/device_manager_evdev.cc',
        'ozone/evdev/device_manager_evdev.h',
        'ozone/evdev/device_manager_udev.cc',
        'ozone/evdev/device_manager_udev.h',
        'ozone/evdev/event_converter_evdev.cc',
        'ozone/evdev/event_converter_evdev.h',
        'ozone/evdev/event_device_info.cc',
        'ozone/evdev/event_device_info.h',
        'ozone/evdev/event_factory_evdev.cc',
        'ozone/evdev/event_factory_evdev.h',
        'ozone/evdev/event_modifiers_evdev.cc',
        'ozone/evdev/event_modifiers_evdev.h',
        'ozone/evdev/key_event_converter_evdev.cc',
        'ozone/evdev/key_event_converter_evdev.h',
        'ozone/evdev/touch_event_converter_evdev.cc',
        'ozone/evdev/touch_event_converter_evdev.h',
        'ozone/event_factory_ozone.cc',
        'ozone/event_factory_ozone.h',
        'ozone/events_ozone.cc',
        'platform/platform_event_dispatcher.h',
        'platform/platform_event_observer.h',
        'platform/platform_event_source.cc',
        'platform/platform_event_source.h',
        'platform/platform_event_source_stub.cc',
        'platform/platform_event_types.h',
        'platform/scoped_event_dispatcher.cc',
        'platform/scoped_event_dispatcher.h',
        'platform/x11/x11_event_source.cc',
        'platform/x11/x11_event_source.h',
        'win/events_win.cc',
        'x/events_x.cc',
        'linux/text_edit_command_auralinux.cc',
        'linux/text_edit_command_auralinux.h',
        'linux/text_edit_key_bindings_delegate_auralinux.cc',
        'linux/text_edit_key_bindings_delegate_auralinux.h',
      ],
      'conditions': [
        # We explicitly enumerate the platforms we _do_ provide native cracking
        # for here.
        ['OS=="win" or use_x11==1 or use_ozone==1', {
          'sources!': [
            'events_stub.cc',
          ],
        }],
        ['chromeos==1', {
          'sources!': [
            'linux/text_edit_command_auralinux.cc',
            'linux/text_edit_command_auralinux.h',
            'linux/text_edit_key_bindings_delegate_auralinux.cc',
            'linux/text_edit_key_bindings_delegate_auralinux.h',
          ],
        }],
        ['use_x11==1', {
          'sources!': [
            'platform/platform_event_source_stub.cc',
          ],
          'dependencies': [
            '<(DEPTH)/build/linux/system.gyp:x11',
          ],
        }],
        ['use_glib==1', {
          'dependencies': [
            '../../build/linux/system.gyp:glib',
          ],
        }],
        ['use_ozone_evdev==1', {
          'defines': ['USE_OZONE_EVDEV=1'],
        }],
        ['use_ozone_evdev==1 and use_udev==1', {
          'dependencies': [
            '<(DEPTH)/build/linux/system.gyp:udev',
          ],
        }],
        ['use_udev==0', {
          'sources!': [
            'ozone/evdev/device_manager_udev.cc',
            'ozone/evdev/device_manager_udev.h',
          ],
        }],
      ],
    },
    {
      'target_name': 'gesture_detection',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
        'events_base',
      ],
      'defines': [
        'GESTURE_DETECTION_IMPLEMENTATION',
      ],
      'sources': [
        'gesture_detection/bitset_32.h',
        'gesture_detection/filtered_gesture_provider.cc',
        'gesture_detection/filtered_gesture_provider.h',
        'gesture_detection/gesture_detection_export.h',
        'gesture_detection/gesture_detector.cc',
        'gesture_detection/gesture_detector.h',
        'gesture_detection/gesture_event_data.cc',
        'gesture_detection/gesture_event_data.h',
        'gesture_detection/gesture_event_data_packet.cc',
        'gesture_detection/gesture_event_data_packet.h',
        'gesture_detection/gesture_config_helper.h',
        'gesture_detection/gesture_config_helper_aura.cc',
        'gesture_detection/gesture_config_helper_android.cc',
        'gesture_detection/gesture_provider.cc',
        'gesture_detection/gesture_provider.h',
        'gesture_detection/motion_event.h',
        'gesture_detection/scale_gesture_detector.cc',
        'gesture_detection/scale_gesture_detector.h',
        'gesture_detection/snap_scroll_controller.cc',
        'gesture_detection/snap_scroll_controller.h',
        'gesture_detection/touch_disposition_gesture_filter.cc',
        'gesture_detection/touch_disposition_gesture_filter.h',
        'gesture_detection/velocity_tracker_state.cc',
        'gesture_detection/velocity_tracker_state.h',
        'gesture_detection/velocity_tracker.cc',
        'gesture_detection/velocity_tracker.h',
      ],
      'conditions': [
        ['use_aura==1', {
          'dependencies': [
            'events'
          ],
        }],
        ['use_aura!=1 and OS!="android"', {
          'sources': [
            'gesture_detection/gesture_config_helper.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'events_test_support',
      'type': 'static_library',
      'dependencies': [
        'events',
        'events_base',
      ],
      'sources': [
        'test/cocoa_test_event_utils.h',
        'test/cocoa_test_event_utils.mm',
        'test/events_test_utils.cc',
        'test/events_test_utils.h',
        'test/events_test_utils_x11.cc',
        'test/events_test_utils_x11.h',
        'test/platform_event_waiter.cc',
        'test/platform_event_waiter.h',
        'test/test_event_handler.cc',
        'test/test_event_handler.h',
        'test/test_event_processor.cc',
        'test/test_event_processor.h',
        'test/test_event_target.cc',
        'test/test_event_target.h',
      ],
      'conditions': [
        ['use_x11==1', {
          'dependencies': [
            '<(DEPTH)/build/linux/system.gyp:x11',
          ],
        }],
        ['OS=="ios"', {
          # The cocoa files don't apply to iOS.
          'sources/': [['exclude', 'cocoa']],
        }],
      ],
    },
    {
      'target_name': 'events_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:run_all_unittests',
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '../gfx/gfx.gyp:gfx',
        '../gfx/gfx.gyp:gfx_geometry',
        'dom4_keycode_converter',
        'events',
        'events_base',
        'events_test_support',
        'gesture_detection'
      ],
      'sources': [
        'cocoa/cocoa_event_utils_unittest.mm',
        'event_dispatcher_unittest.cc',
        'event_processor_unittest.cc',
        'event_rewriter_unittest.cc',
        'event_unittest.cc',
        'gestures/velocity_calculator_unittest.cc',
        'gesture_detection/bitset_32_unittest.cc',
        'gesture_detection/gesture_provider_unittest.cc',
        'gesture_detection/mock_motion_event.h',
        'gesture_detection/mock_motion_event.cc',
        'gesture_detection/velocity_tracker_unittest.cc',
        'gesture_detection/touch_disposition_gesture_filter_unittest.cc',
        'keycodes/dom4/keycode_converter_unittest.cc',
        'latency_info_unittest.cc',
        'ozone/evdev/key_event_converter_evdev_unittest.cc',
        'ozone/evdev/touch_event_converter_evdev_unittest.cc',
        'platform/platform_event_source_unittest.cc',
        'x/events_x_unittest.cc',
      ],
      'conditions': [
        # TODO(dmikurube): Kill linux_use_tcmalloc. http://crbug.com/345554
        ['OS=="linux" and ((use_allocator!="none" and use_allocator!="see_use_tcmalloc") or (use_allocator=="see_use_tcmalloc" and linux_use_tcmalloc==1))', {
          'dependencies': [
            '<(DEPTH)/base/allocator/allocator.gyp:allocator',
          ],
        }],
        # Exclude tests that rely on event_utils.h for platforms that do not
        # provide native cracking, i.e., platforms that use events_stub.cc.
        ['OS!="win" and use_x11!=1 and use_ozone!=1', {
          'sources!': [
            'event_unittest.cc',
          ],
        }],
        ['OS == "android" and gtest_target_type == "shared_library"', {
          'dependencies': [
            '../../testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    # Special target to wrap a gtest_target_type==shared_library
    # ui_unittests into an android apk for execution.
    # See base.gyp for TODO(jrg)s about this strategy.
    ['OS == "android" and gtest_target_type == "shared_library"', {
      'targets': [
        {
          'target_name': 'events_unittests_apk',
          'type': 'none',
          'dependencies': [
            'events_unittests',
          ],
          'variables': {
            'test_suite_name': 'events_unittests',
            'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)events_unittests<(SHARED_LIB_SUFFIX)',
          },
          'includes': [ '../../build/apk_test.gypi' ],
        },
      ],
    }],
  ],
}
