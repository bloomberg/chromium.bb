// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_gamepad_shared.h"

#include "base/basictypes.h"

namespace ppapi {

void ConvertWebKitGamepadData(const WebKitGamepads& webkit_data,
                              PP_GamepadsSampleData* output_data) {
  output_data->length = webkit_data.length;
  for (unsigned i = 0; i < webkit_data.length; ++i) {
    PP_GamepadSampleData& output_pad = output_data->items[i];
    const WebKitGamepad& webkit_pad = webkit_data.items[i];
    output_pad.connected = webkit_pad.connected ? PP_TRUE : PP_FALSE;
    if (webkit_pad.connected) {
      COMPILE_ASSERT(sizeof(output_pad.id) == sizeof(webkit_pad.id),
                     id_size_does_not_match);
      COMPILE_ASSERT(sizeof(output_pad.axes) == sizeof(webkit_pad.axes),
                     axes_size_does_not_match);
      COMPILE_ASSERT(sizeof(output_pad.buttons) == sizeof(webkit_pad.buttons),
                     buttons_size_does_not_match);
      memcpy(output_pad.id, webkit_pad.id, sizeof(output_pad.id));
      output_pad.timestamp = webkit_pad.timestamp;
      output_pad.axes_length = webkit_pad.axes_length;
      memcpy(output_pad.axes, webkit_pad.axes, sizeof(output_pad.axes));
      output_pad.buttons_length = webkit_pad.buttons_length;
      memcpy(output_pad.buttons,
             webkit_pad.buttons,
             sizeof(output_pad.buttons));
    }
  }
}

}  // namespace ppapi
