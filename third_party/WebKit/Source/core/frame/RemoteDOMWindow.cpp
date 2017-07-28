// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/RemoteDOMWindow.h"

#include "core/dom/Document.h"
#include "core/frame/RemoteFrameClient.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

ExecutionContext* RemoteDOMWindow::GetExecutionContext() const {
  return nullptr;
}

DEFINE_TRACE(RemoteDOMWindow) {
  DOMWindow::Trace(visitor);
}

void RemoteDOMWindow::blur() {
  // FIXME: Implement.
}

RemoteDOMWindow::RemoteDOMWindow(RemoteFrame& frame) : DOMWindow(frame) {}

void RemoteDOMWindow::FrameDetached() {
  DisconnectFromFrame();
}

void RemoteDOMWindow::SchedulePostMessage(MessageEvent* event,
                                          PassRefPtr<SecurityOrigin> target,
                                          Document* source) {
  GetFrame()->Client()->ForwardPostMessage(event, std::move(target),
                                           source->GetFrame());
}

}  // namespace blink
