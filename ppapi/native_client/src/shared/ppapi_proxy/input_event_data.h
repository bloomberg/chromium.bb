// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_INPUT_EVENT_DATA_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_INPUT_EVENT_DATA_H_

#include "native_client/src/include/nacl_macros.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/ppb_input_event.h"

namespace ppapi_proxy {

// The data for a single input event.
struct InputEventData {
  InputEventData();
  ~InputEventData();

  PP_InputEvent_Type event_type;
  uint32_t event_modifiers;
  PP_TimeTicks event_time_stamp;

  PP_InputEvent_MouseButton mouse_button;
  int32_t mouse_click_count;
  PP_Point mouse_position;
  PP_Point mouse_movement;

  PP_FloatPoint wheel_delta;
  PP_FloatPoint wheel_ticks;
  PP_Bool wheel_scroll_by_page;

  uint32_t key_code;
};
// Make sure that the size is consistent across platforms, so the alignment is
// consistent. Note the fields above are carefully ordered to make sure it is
// consistent. New fields may require some adjustment to keep a consistent size
// and alignment. TODO(dmichael): This is only required because we don't pickle
// the type. As a short-cut, we memcpy it instead. It would be cleaner to
// pickle this struct.
PP_COMPILE_ASSERT_SIZE_IN_BYTES(InputEventData, 64);

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_INPUT_EVENT_DATA_H_
