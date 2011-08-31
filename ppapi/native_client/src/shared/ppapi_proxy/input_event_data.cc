// Copyright 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/input_event_data.h"

namespace ppapi_proxy {

InputEventData::InputEventData()
    : event_type(PP_INPUTEVENT_TYPE_UNDEFINED),
      event_modifiers(0),
      event_time_stamp(0.0),
      mouse_button(PP_INPUTEVENT_MOUSEBUTTON_NONE),
      mouse_click_count(0),
      mouse_position(PP_MakePoint(0, 0)),
      mouse_movement(PP_MakePoint(0, 0)),
      wheel_delta(PP_MakeFloatPoint(0.0f, 0.0f)),
      wheel_ticks(PP_MakeFloatPoint(0.0f, 0.0f)),
      wheel_scroll_by_page(PP_FALSE),
      key_code(0) {
}

InputEventData::~InputEventData() {
}

}  // namespace ppapi_proxy
