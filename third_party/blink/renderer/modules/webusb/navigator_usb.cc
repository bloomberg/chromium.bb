// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webusb/navigator_usb.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/webusb/usb.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

NavigatorUSB& NavigatorUSB::From(Navigator& navigator) {
  NavigatorUSB* supplement =
      Supplement<Navigator>::From<NavigatorUSB>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorUSB>(navigator);
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

void NavigatorUSB::Trace(Visitor* visitor) {
  visitor->Trace(usb_);
  Supplement<Navigator>::Trace(visitor);
}

NavigatorUSB::NavigatorUSB(Navigator& navigator) {
  if (navigator.DomWindow())
    usb_ = MakeGarbageCollected<USB>(*navigator.DomWindow());
}

const char NavigatorUSB::kSupplementName[] = "NavigatorUSB";

}  // namespace blink
