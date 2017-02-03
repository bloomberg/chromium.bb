// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/RemoteDOMWindow.h"

#include "core/dom/Document.h"
#include "core/frame/RemoteFrameClient.h"

namespace blink {

ExecutionContext* RemoteDOMWindow::getExecutionContext() const {
  return nullptr;
}

DEFINE_TRACE(RemoteDOMWindow) {
  DOMWindow::trace(visitor);
}

void RemoteDOMWindow::blur() {
  // FIXME: Implement.
}

RemoteDOMWindow::RemoteDOMWindow(RemoteFrame& frame) : DOMWindow(frame) {}

void RemoteDOMWindow::frameDetached() {
  disconnectFromFrame();
}

void RemoteDOMWindow::schedulePostMessage(MessageEvent* event,
                                          PassRefPtr<SecurityOrigin> target,
                                          Document* source) {
  frame()->client()->forwardPostMessage(event, std::move(target),
                                        source->frame());
}

}  // namespace blink
