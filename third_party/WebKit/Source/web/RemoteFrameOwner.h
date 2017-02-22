// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef RemoteFrameOwner_h
#define RemoteFrameOwner_h

#include "core/frame/FrameOwner.h"
#include "platform/scroll/ScrollTypes.h"
#include "public/web/WebFrameOwnerProperties.h"

namespace blink {

// Helper class to bridge communication for a frame with a remote parent.
// Currently, it serves two purposes:
// 1. Allows the local frame's loader to retrieve sandbox flags associated with
//    its owner element in another process.
// 2. Trigger a load event on its owner element once it finishes a load.
class RemoteFrameOwner final
    : public GarbageCollectedFinalized<RemoteFrameOwner>,
      public FrameOwner {
  USING_GARBAGE_COLLECTED_MIXIN(RemoteFrameOwner);

 public:
  static RemoteFrameOwner* create(
      SandboxFlags flags,
      const WebFrameOwnerProperties& frameOwnerProperties) {
    return new RemoteFrameOwner(flags, frameOwnerProperties);
  }

  // FrameOwner overrides:
  Frame* contentFrame() const override { return m_frame.get(); }
  void setContentFrame(Frame&) override;
  void clearContentFrame() override;
  SandboxFlags getSandboxFlags() const override { return m_sandboxFlags; }
  void setSandboxFlags(SandboxFlags flags) { m_sandboxFlags = flags; }
  void dispatchLoad() override;
  // TODO(dcheng): Implement.
  bool canRenderFallbackContent() const override { return false; }
  void renderFallbackContent() override {}

  AtomicString browsingContextContainerName() const override {
    return m_browsingContextContainerName;
  }
  ScrollbarMode scrollingMode() const override { return m_scrolling; }
  int marginWidth() const override { return m_marginWidth; }
  int marginHeight() const override { return m_marginHeight; }
  bool allowFullscreen() const override { return m_allowFullscreen; }
  bool allowPaymentRequest() const override { return m_allowPaymentRequest; }
  AtomicString csp() const override { return m_csp; }
  const WebVector<mojom::blink::PermissionName>& delegatedPermissions()
      const override {
    return m_delegatedPermissions;
  }
  const WebVector<WebFeaturePolicyFeature>& allowedFeatures() const override {
    return m_allowedFeatures;
  }

  void setBrowsingContextContainerName(const WebString& name) {
    m_browsingContextContainerName = name;
  }
  void setScrollingMode(WebFrameOwnerProperties::ScrollingMode);
  void setMarginWidth(int marginWidth) { m_marginWidth = marginWidth; }
  void setMarginHeight(int marginHeight) { m_marginHeight = marginHeight; }
  void setAllowFullscreen(bool allowFullscreen) {
    m_allowFullscreen = allowFullscreen;
  }
  void setAllowPaymentRequest(bool allowPaymentRequest) {
    m_allowPaymentRequest = allowPaymentRequest;
  }
  void setCsp(const WebString& csp) { m_csp = csp; }
  void setDelegatedpermissions(
      const WebVector<mojom::blink::PermissionName>& delegatedPermissions) {
    m_delegatedPermissions = delegatedPermissions;
  }
  void setAllowedFeatures(
      const WebVector<WebFeaturePolicyFeature>& allowedFeatures) {
    m_allowedFeatures = allowedFeatures;
  }

  DECLARE_VIRTUAL_TRACE();

 private:
  RemoteFrameOwner(SandboxFlags, const WebFrameOwnerProperties&);

  // Intentionally private to prevent redundant checks when the type is
  // already HTMLFrameOwnerElement.
  bool isLocal() const override { return false; }
  bool isRemote() const override { return true; }

  Member<Frame> m_frame;
  SandboxFlags m_sandboxFlags;
  AtomicString m_browsingContextContainerName;
  ScrollbarMode m_scrolling;
  int m_marginWidth;
  int m_marginHeight;
  bool m_allowFullscreen;
  bool m_allowPaymentRequest;
  WebString m_csp;
  WebVector<mojom::blink::PermissionName> m_delegatedPermissions;
  WebVector<WebFeaturePolicyFeature> m_allowedFeatures;
};

DEFINE_TYPE_CASTS(RemoteFrameOwner,
                  FrameOwner,
                  owner,
                  owner->isRemote(),
                  owner.isRemote());

}  // namespace blink

#endif  // RemoteFrameOwner_h
