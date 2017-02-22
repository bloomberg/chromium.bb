// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FrameOwner_h
#define FrameOwner_h

#include "core/CoreExport.h"
#include "core/dom/SandboxFlags.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollTypes.h"
#include "public/platform/WebFeaturePolicy.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/permissions/permission.mojom-blink.h"

namespace blink {

class Frame;

// Oilpan: all FrameOwner instances are GCed objects. FrameOwner additionally
// derives from GarbageCollectedMixin so that Member<FrameOwner> references can
// be kept (e.g., Frame::m_owner.)
class CORE_EXPORT FrameOwner : public GarbageCollectedMixin {
 public:
  virtual ~FrameOwner() {}
  DEFINE_INLINE_VIRTUAL_TRACE() {}

  virtual bool isLocal() const = 0;
  virtual bool isRemote() const = 0;

  virtual Frame* contentFrame() const = 0;
  virtual void setContentFrame(Frame&) = 0;
  virtual void clearContentFrame() = 0;

  virtual SandboxFlags getSandboxFlags() const = 0;
  virtual void dispatchLoad() = 0;

  // On load failure, a frame can ask its owner to render fallback content
  // which replaces the frame contents.
  virtual bool canRenderFallbackContent() const = 0;
  virtual void renderFallbackContent() = 0;

  // Returns the 'name' content attribute value of the browsing context
  // container.
  // https://html.spec.whatwg.org/multipage/browsers.html#browsing-context-container
  virtual AtomicString browsingContextContainerName() const = 0;
  virtual ScrollbarMode scrollingMode() const = 0;
  virtual int marginWidth() const = 0;
  virtual int marginHeight() const = 0;
  virtual bool allowFullscreen() const = 0;
  virtual bool allowPaymentRequest() const = 0;
  virtual AtomicString csp() const = 0;
  virtual const WebVector<mojom::blink::PermissionName>& delegatedPermissions()
      const = 0;
  virtual const WebVector<WebFeaturePolicyFeature>& allowedFeatures() const = 0;
};

// TODO(dcheng): This class is an internal implementation detail of provisional
// frames. Move this into WebLocalFrameImpl.cpp and remove existing dependencies
// on it.
class CORE_EXPORT DummyFrameOwner
    : public GarbageCollectedFinalized<DummyFrameOwner>,
      public FrameOwner {
  USING_GARBAGE_COLLECTED_MIXIN(DummyFrameOwner);

 public:
  static DummyFrameOwner* create() { return new DummyFrameOwner; }

  DEFINE_INLINE_VIRTUAL_TRACE() { FrameOwner::trace(visitor); }

  // FrameOwner overrides:
  Frame* contentFrame() const override { return nullptr; }
  void setContentFrame(Frame&) override {}
  void clearContentFrame() override {}
  SandboxFlags getSandboxFlags() const override { return SandboxNone; }
  void dispatchLoad() override {}
  bool canRenderFallbackContent() const override { return false; }
  void renderFallbackContent() override {}
  AtomicString browsingContextContainerName() const override {
    return AtomicString();
  }
  ScrollbarMode scrollingMode() const override { return ScrollbarAuto; }
  int marginWidth() const override { return -1; }
  int marginHeight() const override { return -1; }
  bool allowFullscreen() const override { return false; }
  bool allowPaymentRequest() const override { return false; }
  AtomicString csp() const override { return nullAtom; }
  const WebVector<mojom::blink::PermissionName>& delegatedPermissions()
      const override {
    DEFINE_STATIC_LOCAL(WebVector<mojom::blink::PermissionName>, permissions,
                        ());
    return permissions;
  }
  const WebVector<WebFeaturePolicyFeature>& allowedFeatures() const override {
    DEFINE_STATIC_LOCAL(WebVector<WebFeaturePolicyFeature>, features, ());
    return features;
  }

 private:
  // Intentionally private to prevent redundant checks when the type is
  // already DummyFrameOwner.
  bool isLocal() const override { return false; }
  bool isRemote() const override { return false; }
};

}  // namespace blink

#endif  // FrameOwner_h
