// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ContextLifecycleObserver.h"

#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"

namespace blink {

ContextClient::ContextClient(ExecutionContext* executionContext)
    : m_executionContext(executionContext) {}

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

DOMWindowClient::DOMWindowClient(LocalDOMWindow* window)
    : m_domWindow(window) {}

DOMWindowClient::DOMWindowClient(LocalFrame* frame)
    : m_domWindow(frame ? frame->domWindow() : nullptr) {}

LocalDOMWindow* DOMWindowClient::domWindow() const {
  return m_domWindow && m_domWindow->frame() ? m_domWindow : nullptr;
}

LocalFrame* DOMWindowClient::frame() const {
  return m_domWindow ? m_domWindow->frame() : nullptr;
}

DEFINE_TRACE(DOMWindowClient) {
  visitor->trace(m_domWindow);
}
}
