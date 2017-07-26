// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRemoteFrameClient_h
#define WebRemoteFrameClient_h

#include "public/platform/WebFocusType.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/web/WebDOMMessageEvent.h"
#include "public/web/WebFrame.h"

namespace blink {
enum class ClientRedirectPolicy;
enum class WebFrameLoadType;
class WebURLRequest;
struct WebRect;

class WebRemoteFrameClient {
 public:
  // Specifies the reason for the detachment.
  enum class DetachType { kRemove, kSwap };

  // Notify the embedder that it should remove this frame from the frame tree
  // and release any resources associated with it.
  virtual void FrameDetached(DetachType) {}

  // Notifies the embedder that a postMessage was issued to a remote frame.
  virtual void ForwardPostMessage(WebLocalFrame* source_frame,
                                  WebRemoteFrame* target_frame,
                                  WebSecurityOrigin target_origin,
                                  WebDOMMessageEvent) {}

  // A remote frame was asked to start a navigation.
  virtual void Navigate(const WebURLRequest& request,
                        bool should_replace_current_entry) {}
  virtual void Reload(WebFrameLoadType, ClientRedirectPolicy) {}

  virtual void FrameRectsChanged(const WebRect&) {}

  virtual void UpdateRemoteViewportIntersection(
      const WebRect& viewport_intersection) {}

  virtual void VisibilityChanged(bool visible) {}

  // Set or clear the inert property on the remote frame.
  virtual void SetIsInert(bool) {}

  // This frame updated its opener to another frame.
  virtual void DidChangeOpener(WebFrame* opener) {}

  // Continue sequential focus navigation in this frame.  This is called when
  // the |source| frame is searching for the next focusable element (e.g., in
  // response to <tab>) and encounters a remote frame.
  virtual void AdvanceFocus(WebFocusType type, WebLocalFrame* source) {}

  // This frame was focused by another frame.
  virtual void FrameFocused() {}
};

}  // namespace blink

#endif  // WebRemoteFrameClient_h
