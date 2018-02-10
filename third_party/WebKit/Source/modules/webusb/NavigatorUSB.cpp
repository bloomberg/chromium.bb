// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webusb/NavigatorUSB.h"

#include "core/frame/Navigator.h"
#include "modules/webusb/USB.h"

namespace blink {

NavigatorUSB& NavigatorUSB::From(Navigator& navigator) {
  NavigatorUSB* supplement =
      Supplement<Navigator>::From<NavigatorUSB>(navigator);
  if (!supplement) {
    supplement = new NavigatorUSB(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

USB* NavigatorUSB::usb(Navigator& navigator) {
  return NavigatorUSB::From(navigator).usb();
}

USB* NavigatorUSB::usb() {
  return usb_;
}

void NavigatorUSB::Trace(blink::Visitor* visitor) {
  visitor->Trace(usb_);
  Supplement<Navigator>::Trace(visitor);
}

NavigatorUSB::NavigatorUSB(Navigator& navigator) {
  if (navigator.GetFrame())
    usb_ = USB::Create(*navigator.GetFrame());
}

const char NavigatorUSB::kSupplementName[] = "NavigatorUSB";

}  // namespace blink
