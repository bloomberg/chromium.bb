// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemoteFrame_h
#define RemoteFrame_h

#include "core/CoreExport.h"
#include "core/dom/RemoteSecurityContext.h"
#include "core/frame/Frame.h"
#include "core/frame/RemoteFrameView.h"
#include "public/platform/WebFocusType.h"

namespace blink {

class LocalFrame;
class RemoteFrameClient;
class WebLayer;
struct FrameLoadRequest;

class CORE_EXPORT RemoteFrame final : public Frame {
 public:
  static RemoteFrame* Create(RemoteFrameClient*, Page&, FrameOwner*);

  ~RemoteFrame() override;

  // Frame overrides:
  DECLARE_VIRTUAL_TRACE();
  void Navigate(Document& origin_document,
                const KURL&,
                bool replace_current_item,
                UserGestureStatus) override;
  void Navigate(const FrameLoadRequest& passed_request) override;
  void Reload(FrameLoadType, ClientRedirectPolicy) override;
  void Detach(FrameDetachType) override;
  RemoteSecurityContext* GetSecurityContext() const override;
  void PrintNavigationErrorMessage(const Frame&, const char* reason) override {}
  void PrintNavigationWarning(const String&) override {}
  bool PrepareForCommit() override;
  bool ShouldClose() override;
  void SetIsInert(bool) override;

  void SetWebLayer(WebLayer*);
  WebLayer* GetWebLayer() const { return web_layer_; }

  void AdvanceFocus(WebFocusType, LocalFrame* source);

  void SetView(RemoteFrameView*);
  void CreateView();

  RemoteFrameView* View() const override;

  RemoteFrameClient* Client() const;

 private:
  RemoteFrame(RemoteFrameClient*, Page&, FrameOwner*);

  // Intentionally private to prevent redundant checks when the type is
  // already RemoteFrame.
  bool IsLocalFrame() const override { return false; }
  bool IsRemoteFrame() const override { return true; }

  void DetachChildren();

  Member<RemoteFrameView> view_;
  Member<RemoteSecurityContext> security_context_;
  WebLayer* web_layer_ = nullptr;
};

inline RemoteFrameView* RemoteFrame::View() const {
  return view_.Get();
}

DEFINE_TYPE_CASTS(RemoteFrame,
                  Frame,
                  remoteFrame,
                  remoteFrame->IsRemoteFrame(),
                  remoteFrame.IsRemoteFrame());

}  // namespace blink

#endif  // RemoteFrame_h
