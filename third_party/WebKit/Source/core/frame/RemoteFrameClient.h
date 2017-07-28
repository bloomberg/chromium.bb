// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemoteFrameClient_h
#define RemoteFrameClient_h

#include "core/frame/FrameClient.h"
#include "core/frame/FrameTypes.h"
#include "core/loader/FrameLoaderTypes.h"
#include "public/platform/WebFocusType.h"

namespace blink {

class IntRect;
class LocalFrame;
class MessageEvent;
class ResourceRequest;
class SecurityOrigin;

class RemoteFrameClient : public FrameClient {
 public:
  ~RemoteFrameClient() override {}

  virtual void Navigate(const ResourceRequest&,
                        bool should_replace_current_entry) = 0;
  virtual void Reload(FrameLoadType, ClientRedirectPolicy) = 0;
  virtual unsigned BackForwardLength() = 0;

  // Forwards a postMessage for a remote frame.
  virtual void ForwardPostMessage(MessageEvent*,
                                  PassRefPtr<SecurityOrigin> target,
                                  LocalFrame* source_frame) const = 0;

  virtual void FrameRectsChanged(const IntRect& frame_rect) = 0;

  virtual void UpdateRemoteViewportIntersection(
      const IntRect& viewport_intersection) = 0;

  virtual void AdvanceFocus(WebFocusType, LocalFrame* source) = 0;

  virtual void VisibilityChanged(bool visible) = 0;

  virtual void SetIsInert(bool) = 0;
};

}  // namespace blink

#endif  // RemoteFrameClient_h
