// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemoteDOMWindow_h
#define RemoteDOMWindow_h

#include "core/frame/DOMWindow.h"
#include "core/frame/RemoteFrame.h"
#include "platform/wtf/Assertions.h"

namespace blink {

class RemoteDOMWindow final : public DOMWindow {
 public:
  static RemoteDOMWindow* Create(RemoteFrame& frame) {
    return new RemoteDOMWindow(frame);
  }

  RemoteFrame* GetFrame() const { return ToRemoteFrame(DOMWindow::GetFrame()); }

  // EventTarget overrides:
  ExecutionContext* GetExecutionContext() const override;

  // DOMWindow overrides:
  DECLARE_VIRTUAL_TRACE();
  void blur() override;

  void FrameDetached();

 protected:
  // Protected DOMWindow overrides:
  void SchedulePostMessage(MessageEvent*,
                           RefPtr<SecurityOrigin> target,
                           Document* source) override;

 private:
  explicit RemoteDOMWindow(RemoteFrame&);

  // Intentionally private to prevent redundant checks when the type is
  // already RemoteDOMWindow.
  bool IsLocalDOMWindow() const override { return false; }
  bool IsRemoteDOMWindow() const override { return true; }
};

DEFINE_TYPE_CASTS(RemoteDOMWindow,
                  DOMWindow,
                  x,
                  x->IsRemoteDOMWindow(),
                  x.IsRemoteDOMWindow());

}  // namespace blink

#endif  // DOMWindow_h
