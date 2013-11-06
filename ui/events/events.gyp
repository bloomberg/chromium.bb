# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'events_base',
      'type': '<(component)',
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
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
        'event_target.cc',
        'event_target.h',
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
        'gestures/gesture_util.cc',
        'gestures/gesture_util.h',
        'gestures/velocity_calculator.cc',
        'gestures/velocity_calculator.h',
        'ozone/evdev/event_factory.cc',
        'ozone/evdev/event_factory.h',
        'ozone/evdev/event_modifiers.cc',
        'ozone/evdev/event_modifiers.h',
        'ozone/evdev/key_event_converter.cc',
        'ozone/evdev/key_event_converter.h',
        'ozone/evdev/touch_event_converter.cc',
        'ozone/evdev/touch_event_converter.h',
        'ozone/event_converter_ozone.cc',
        'ozone/event_converter_ozone.h',
        'ozone/event_factory_ozone.cc',
        'ozone/event_factory_ozone.h',
        'ozone/events_ozone.cc',
        'win/events_win.cc',
        'x/device_data_manager.cc',
        'x/device_data_manager.h',
        'x/device_list_cache_x.cc',
        'x/device_list_cache_x.h',
        'x/events_x.cc',
        'x/events_x_utils.cc',
        'x/events_x_utils.h',
        'x/touch_factory_x11.cc',
        'x/touch_factory_x11.h',
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
      ],
    }
  ],
}
