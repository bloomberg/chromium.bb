// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_USB_KEY_CODE_CONVERSION_H_
#define WEBKIT_PLUGINS_PPAPI_USB_KEY_CODE_CONVERSION_H_

#include "ppapi/c/pp_stdint.h"

namespace WebKit {
class WebKeyboardEvent;
}  // namespace WebKit

namespace webkit {
namespace ppapi {

// Returns a 32-bit "USB Key Code" for the key identifier by the supplied
// WebKeyboardEvent. The supplied event must be a KeyDown or KeyUp.
// The code consists of the USB Page (in the high-order 16-bit word) and
// USB Usage Id of the key.  If no translation can be performed then zero
// is returned.
uint32_t UsbKeyCodeForKeyboardEvent(const WebKit::WebKeyboardEvent& key_event);

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_USB_KEY_CODE_CONVERSION_H_
