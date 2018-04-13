// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/remote_dom_window.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/remote_frame_client.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

ExecutionContext* RemoteDOMWindow::GetExecutionContext() const {
  return nullptr;
}

void RemoteDOMWindow::Trace(blink::Visitor* visitor) {
  DOMWindow::Trace(visitor);
}

void RemoteDOMWindow::blur() {
  // FIXME: Implement.
}

RemoteDOMWindow::RemoteDOMWindow(RemoteFrame& frame) : DOMWindow(frame) {}

void RemoteDOMWindow::FrameDetached() {
  DisconnectFromFrame();
}

void RemoteDOMWindow::SchedulePostMessage(
    MessageEvent* event,
    scoped_refptr<const SecurityOrigin> target,
    Document* source) {
  GetFrame()->Client()->ForwardPostMessage(event, std::move(target),
                                           source->GetFrame());
}

}  // namespace blink
