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

const char* WindowPaintWorklet::supplementName() {
  return "WindowPaintWorklet";
}

// static
WindowPaintWorklet& WindowPaintWorklet::from(LocalDOMWindow& window) {
  WindowPaintWorklet* supplement = static_cast<WindowPaintWorklet*>(
      Supplement<LocalDOMWindow>::from(window, supplementName()));
  if (!supplement) {
    supplement = new WindowPaintWorklet(window);
    provideTo(window, supplementName(), supplement);
  }
  return *supplement;
}

// static
Worklet* WindowPaintWorklet::paintWorklet(LocalDOMWindow& window) {
  return from(window).paintWorklet();
}

PaintWorklet* WindowPaintWorklet::paintWorklet() {
  if (!m_paintWorklet && supplementable()->frame())
    m_paintWorklet = PaintWorklet::create(supplementable()->frame());
  return m_paintWorklet.get();
}

DEFINE_TRACE(WindowPaintWorklet) {
  visitor->trace(m_paintWorklet);
  Supplement<LocalDOMWindow>::trace(visitor);
}

}  // namespace blink
