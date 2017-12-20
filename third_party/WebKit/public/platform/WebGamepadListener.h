// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGamepadListener_h
#define WebGamepadListener_h

#include "WebPlatformEventListener.h"

namespace device {
class Gamepad;
}

namespace blink {

class WebGamepadListener : public WebPlatformEventListener {
 public:
  virtual void DidConnectGamepad(unsigned index, const device::Gamepad&) = 0;
  virtual void DidDisconnectGamepad(unsigned index, const device::Gamepad&) = 0;

 protected:
  ~WebGamepadListener() override = default;
};

}  // namespace blink

#endif
