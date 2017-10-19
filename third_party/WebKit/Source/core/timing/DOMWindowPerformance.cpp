// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/DOMWindowPerformance.h"

#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/timing/Performance.h"

namespace blink {

DOMWindowPerformance::DOMWindowPerformance(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

void DOMWindowPerformance::Trace(blink::Visitor* visitor) {
  visitor->Trace(performance_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

// static
const char* DOMWindowPerformance::SupplementName() {
  return "DOMWindowPerformance";
}

// static
DOMWindowPerformance& DOMWindowPerformance::From(LocalDOMWindow& window) {
  DOMWindowPerformance* supplement = static_cast<DOMWindowPerformance*>(
      Supplement<LocalDOMWindow>::From(window, SupplementName()));
  if (!supplement) {
    supplement = new DOMWindowPerformance(window);
    ProvideTo(window, SupplementName(), supplement);
  }
  return *supplement;
}

// static
Performance* DOMWindowPerformance::performance(LocalDOMWindow& window) {
  return From(window).performance();
}

Performance* DOMWindowPerformance::performance() {
  if (!performance_)
    performance_ = Performance::Create(GetSupplementable());
  return performance_.Get();
}

}  // namespace blink
