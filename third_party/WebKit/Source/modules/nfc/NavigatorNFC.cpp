// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/nfc/NavigatorNFC.h"

#include "core/frame/Navigator.h"
#include "modules/nfc/NFC.h"

namespace blink {

NavigatorNFC::NavigatorNFC(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

const char NavigatorNFC::kSupplementName[] = "NavigatorNFC";

NavigatorNFC& NavigatorNFC::From(Navigator& navigator) {
  NavigatorNFC* supplement =
      Supplement<Navigator>::From<NavigatorNFC>(navigator);
  if (!supplement) {
    supplement = new NavigatorNFC(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

NFC* NavigatorNFC::nfc(Navigator& navigator) {
  NavigatorNFC& self = NavigatorNFC::From(navigator);
  if (!self.nfc_) {
    if (!navigator.GetFrame())
      return nullptr;
    self.nfc_ = NFC::Create(navigator.GetFrame());
  }
  return self.nfc_.Get();
}

void NavigatorNFC::Trace(blink::Visitor* visitor) {
  visitor->Trace(nfc_);
  Supplement<Navigator>::Trace(visitor);
}

void NavigatorNFC::TraceWrappers(const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(nfc_);
  Supplement<Navigator>::TraceWrappers(visitor);
}

}  // namespace blink
