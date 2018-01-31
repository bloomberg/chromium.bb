// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemoteFrameClientImpl_h
#define RemoteFrameClientImpl_h

#include "core/frame/RemoteFrameClient.h"

namespace blink {
class WebRemoteFrameImpl;

class RemoteFrameClientImpl final : public RemoteFrameClient {
 public:
  static RemoteFrameClientImpl* Create(WebRemoteFrameImpl*);

  virtual void Trace(blink::Visitor*);

  // FrameClient overrides:
  bool InShadowTree() const override;
  void Detached(FrameDetachType) override;
  Frame* Opener() const override;
  void SetOpener(Frame*) override;
  Frame* Parent() const override;
  Frame* Top() const override;
  Frame* NextSibling() const override;
  Frame* FirstChild() const override;
  void FrameFocused() const override;
  String GetDevToolsFrameToken() const override;

  // RemoteFrameClient overrides:
  void Navigate(const ResourceRequest&,
                bool should_replace_current_entry) override;
  void Reload(FrameLoadType, ClientRedirectPolicy) override;
  unsigned BackForwardLength() override;
  void ForwardPostMessage(MessageEvent*,
                          scoped_refptr<const SecurityOrigin> target,
                          LocalFrame* source) const override;
  void FrameRectsChanged(const IntRect& local_frame_rect,
                         const IntRect& screen_space_rect) override;
  void UpdateRemoteViewportIntersection(const IntRect&) override;
  void AdvanceFocus(WebFocusType, LocalFrame*) override;
  void VisibilityChanged(bool visible) override;
  void SetIsInert(bool) override;
  void UpdateRenderThrottlingStatus(bool is_throttled,
                                    bool subtree_throttled) override;

  WebRemoteFrameImpl* GetWebFrame() const { return web_frame_; }

 private:
  explicit RemoteFrameClientImpl(WebRemoteFrameImpl*);

  Member<WebRemoteFrameImpl> web_frame_;
};

}  // namespace blink

#endif  // RemoteFrameClientImpl_h
