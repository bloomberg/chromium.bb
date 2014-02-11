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
        'dom4_keycode_converter',
      ],
      'defines': [
        'EVENTS_BASE_IMPLEMENTATION',
      ],
      'sources': [
        'events_base_export.h',
        'event_switches.cc',
        'event_switches.h',
        'keycodes/keyboard_code_conversion.cc',
        'keycodes/keyboard_code_conversion.h',
        'keycodes/keyboard_code_conversion_android.cc',
        'keycodes/keyboard_code_conversion_android.h',
        'keycodes/keyboard_code_conversion_gtk.cc',
        'keycodes/keyboard_code_conversion_gtk.h',
        'keycodes/keyboard_code_conversion_mac.h',
        'keycodes/keyboard_code_conversion_mac.mm',
        'keycodes/keyboard_code_conversion_win.cc',
        'keycodes/keyboard_code_conversion_win.h',
        'keycodes/keyboard_code_conversion_x.cc',
        'keycodes/keyboard_code_conversion_x.h',
        'keycodes/keyboard_codes.h',
        'latency_info.cc',
        'latency_info.h',
        'x/device_list_cache_x.cc',
        'x/device_list_cache_x.h',
        'x/device_data_manager.cc',
        'x/device_data_manager.h',
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
        'event.cc',
        'event.h',
        'event_constants.h',
        'event_dispatcher.cc',
        'event_dispatcher.h',
        'event_handler.cc',
        'event_handler.h',
        'event_processor.cc',
        'event_processor.h',
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
        'gestures/gesture_types.cc',
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
        'win/events_win.cc',
        'x/events_x.cc',
      ],
      'conditions': [
        # We explicitly enumerate the platforms we _do_ provide native cracking
        # for here.
        ['OS=="win" or use_x11==1 or use_ozone==1', {
          'sources!': [
            'events_stub.cc',
          ],
        }],
        ['use_x11==1', {
          'dependencies': [
            '<(DEPTH)/build/linux/system.gyp:x11',
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
      ],
      'sources': [
        'event_dispatcher_unittest.cc',
        'event_processor_unittest.cc',
        'event_unittest.cc',
        'gestures/velocity_calculator_unittest.cc',
        'keycodes/dom4/keycode_converter_unittest.cc',
        'latency_info_unittest.cc',
        'ozone/evdev/key_event_converter_evdev_unittest.cc',
        'ozone/evdev/touch_event_converter_evdev_unittest.cc',
        'x/events_x_unittest.cc',
      ],
      'conditions': [
        ['OS=="linux" and linux_use_tcmalloc==1', {
          'dependencies': [
            '<(DEPTH)/base/allocator/allocator.gyp:allocator',
          ],
        }],
      ],
    },
  ],
}
