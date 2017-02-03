// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemoteDOMWindow_h
#define RemoteDOMWindow_h

#include "core/frame/DOMWindow.h"
#include "core/frame/RemoteFrame.h"
#include "wtf/Assertions.h"

namespace blink {

class RemoteDOMWindow final : public DOMWindow {
 public:
  static RemoteDOMWindow* create(RemoteFrame& frame) {
    return new RemoteDOMWindow(frame);
  }

  RemoteFrame* frame() const { return toRemoteFrame(DOMWindow::frame()); }

  // EventTarget overrides:
  ExecutionContext* getExecutionContext() const override;

  // DOMWindow overrides:
  DECLARE_VIRTUAL_TRACE();
  void blur() override;

  void frameDetached();

 protected:
  // Protected DOMWindow overrides:
  void schedulePostMessage(MessageEvent*,
                           PassRefPtr<SecurityOrigin> target,
                           Document* source) override;

 private:
  explicit RemoteDOMWindow(RemoteFrame&);

  // Intentionally private to prevent redundant checks when the type is
  // already RemoteDOMWindow.
  bool isLocalDOMWindow() const override { return false; }
  bool isRemoteDOMWindow() const override { return true; }
};

DEFINE_TYPE_CASTS(RemoteDOMWindow,
                  DOMWindow,
                  x,
                  x->isRemoteDOMWindow(),
                  x.isRemoteDOMWindow());

}  // namespace blink

#endif  // DOMWindow_h
