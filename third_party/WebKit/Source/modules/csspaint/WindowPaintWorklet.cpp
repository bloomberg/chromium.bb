// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/csspaint/WindowPaintWorklet.h"

#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "modules/csspaint/PaintWorklet.h"

namespace blink {

WindowPaintWorklet::WindowPaintWorklet(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

const char* WindowPaintWorklet::SupplementName() {
  return "WindowPaintWorklet";
}

// static
WindowPaintWorklet& WindowPaintWorklet::From(LocalDOMWindow& window) {
  WindowPaintWorklet* supplement = static_cast<WindowPaintWorklet*>(
      Supplement<LocalDOMWindow>::From(window, SupplementName()));
  if (!supplement) {
    supplement = new WindowPaintWorklet(window);
    ProvideTo(window, SupplementName(), supplement);
  }
  return *supplement;
}

// static
Worklet* WindowPaintWorklet::paintWorklet(LocalDOMWindow& window) {
  return From(window).paintWorklet();
}

PaintWorklet* WindowPaintWorklet::paintWorklet() {
  if (!paint_worklet_ && GetSupplementable()->GetFrame())
    paint_worklet_ = PaintWorklet::Create(GetSupplementable()->GetFrame());
  return paint_worklet_.Get();
}

DEFINE_TRACE(WindowPaintWorklet) {
  visitor->Trace(paint_worklet_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

}  // namespace blink
