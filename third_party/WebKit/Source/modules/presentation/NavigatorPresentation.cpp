// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/NavigatorPresentation.h"

#include "core/frame/Navigator.h"
#include "modules/presentation/Presentation.h"

namespace blink {

NavigatorPresentation::NavigatorPresentation() {}

// static
const char* NavigatorPresentation::SupplementName() {
  return "NavigatorPresentation";
}

// static
NavigatorPresentation& NavigatorPresentation::From(Navigator& navigator) {
  NavigatorPresentation* supplement = static_cast<NavigatorPresentation*>(
      Supplement<Navigator>::From(navigator, SupplementName()));
  if (!supplement) {
    supplement = new NavigatorPresentation();
    ProvideTo(navigator, SupplementName(), supplement);
  }
  return *supplement;
}

// static
Presentation* NavigatorPresentation::presentation(Navigator& navigator) {
  NavigatorPresentation& self = NavigatorPresentation::From(navigator);
  if (!self.presentation_) {
    if (!navigator.GetFrame())
      return nullptr;
    self.presentation_ = Presentation::Create(navigator.GetFrame());
  }
  return self.presentation_;
}

void NavigatorPresentation::Trace(blink::Visitor* visitor) {
  visitor->Trace(presentation_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
