// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ContextLifecycleObserver.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"

namespace blink {

ContextClient::ContextClient(LocalFrame* frame)
    : m_executionContext(frame ? frame->document() : nullptr) {}

ExecutionContext* ContextClient::getExecutionContext() const {
  return m_executionContext && !m_executionContext->isContextDestroyed()
             ? m_executionContext
             : nullptr;
}

LocalFrame* ContextClient::frame() const {
  return m_executionContext && m_executionContext->isDocument()
             ? toDocument(m_executionContext)->frame()
             : nullptr;
}

DEFINE_TRACE(ContextClient) {
  visitor->trace(m_executionContext);
}

LocalFrame* ContextLifecycleObserver::frame() const {
  return getExecutionContext() && getExecutionContext()->isDocument()
             ? toDocument(getExecutionContext())->frame()
             : nullptr;
}
}
