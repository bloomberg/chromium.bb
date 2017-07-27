// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.#ifndef WebViewBase_h

#ifndef WebRemoteFrameBase_h
#define WebRemoteFrameBase_h

#include "core/CoreExport.h"
#include "public/web/WebRemoteFrame.h"

namespace blink {

class FrameOwner;
class Page;
class RemoteFrame;
class WebRemoteFrameClient;

class WebRemoteFrameBase : public GarbageCollectedFinalized<WebRemoteFrameBase>,
                           public WebRemoteFrame {
 public:
  virtual void InitializeCoreFrame(Page&,
                                   FrameOwner*,
                                   const AtomicString& name) = 0;
  virtual RemoteFrame* GetFrame() const = 0;
  virtual void SetCoreFrame(RemoteFrame*) = 0;
  virtual WebRemoteFrameClient* Client() const = 0;

  DEFINE_INLINE_VIRTUAL_TRACE() {}

 protected:
  explicit WebRemoteFrameBase(WebTreeScopeType scope) : WebRemoteFrame(scope) {}
};

DEFINE_TYPE_CASTS(WebRemoteFrameBase,
                  WebFrame,
                  frame,
                  frame->IsWebRemoteFrame(),
                  frame.IsWebRemoteFrame());
}  // namespace blink

#endif  // WebRemoteFrameBase_h
