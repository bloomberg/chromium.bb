// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/usb_key_code_conversion.h"

#include "build/build_config.h"

using WebKit::WebKeyboardEvent;

namespace webkit {
namespace ppapi {

#if !defined(OS_LINUX) && !defined(OS_MACOSX) && !defined(OS_WIN)

uint32_t UsbKeyCodeForKeyboardEvent(const WebKeyboardEvent& key_event) {
  return 0;
}

#endif

}  // namespace ppapi
}  // namespace webkit
