// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/V0CustomElementMicrotaskQueueBase.h"

#include "core/dom/custom/V0CustomElementProcessingStack.h"

namespace blink {

void V0CustomElementMicrotaskQueueBase::dispatch() {
  CHECK(!m_inDispatch);
  m_inDispatch = true;
  doDispatch();
  m_inDispatch = false;
}

DEFINE_TRACE(V0CustomElementMicrotaskQueueBase) {
  visitor->trace(m_queue);
}

#if !defined(NDEBUG)
void V0CustomElementMicrotaskQueueBase::show(unsigned indent) {
  for (const auto& step : m_queue) {
    if (step)
      step->show(indent);
    else
      fprintf(stderr, "%*snull\n", indent, "");
  }
}
#endif

}  // namespace blink
