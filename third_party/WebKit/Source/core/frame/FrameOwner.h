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

namespace blink {

class Frame;

// Oilpan: all FrameOwner instances are GCed objects. FrameOwner additionally
// derives from GarbageCollectedMixin so that Member<FrameOwner> references can
// be kept (e.g., Frame::m_owner.)
class CORE_EXPORT FrameOwner : public GarbageCollectedMixin {
 public:
  virtual ~FrameOwner() {}
  virtual void Trace(blink::Visitor* visitor) {}

  virtual bool IsLocal() const = 0;
  virtual bool IsRemote() const = 0;

  virtual Frame* ContentFrame() const = 0;
  virtual void SetContentFrame(Frame&) = 0;
  virtual void ClearContentFrame() = 0;

  virtual SandboxFlags GetSandboxFlags() const = 0;
  virtual void DispatchLoad() = 0;

  // On load failure, a frame can ask its owner to render fallback content
  // which replaces the frame contents.
  virtual bool CanRenderFallbackContent() const = 0;
  virtual void RenderFallbackContent() = 0;

  // Returns the 'name' content attribute value of the browsing context
  // container.
  // https://html.spec.whatwg.org/multipage/browsers.html#browsing-context-container
  virtual AtomicString BrowsingContextContainerName() const = 0;
  virtual ScrollbarMode ScrollingMode() const = 0;
  virtual int MarginWidth() const = 0;
  virtual int MarginHeight() const = 0;
  virtual bool AllowFullscreen() const = 0;
  virtual bool AllowPaymentRequest() const = 0;
  virtual bool IsDisplayNone() const = 0;
  virtual AtomicString Csp() const = 0;
  virtual const WebParsedFeaturePolicy& ContainerPolicy() const = 0;
};

// TODO(dcheng): This class is an internal implementation detail of provisional
// frames. Move this into WebLocalFrameImpl.cpp and remove existing dependencies
// on it.
class CORE_EXPORT DummyFrameOwner
    : public GarbageCollectedFinalized<DummyFrameOwner>,
      public FrameOwner {
  USING_GARBAGE_COLLECTED_MIXIN(DummyFrameOwner);

 public:
  static DummyFrameOwner* Create() { return new DummyFrameOwner; }

  virtual void Trace(blink::Visitor* visitor) { FrameOwner::Trace(visitor); }

  // FrameOwner overrides:
  Frame* ContentFrame() const override { return nullptr; }
  void SetContentFrame(Frame&) override {}
  void ClearContentFrame() override {}
  SandboxFlags GetSandboxFlags() const override { return kSandboxNone; }
  void DispatchLoad() override {}
  bool CanRenderFallbackContent() const override { return false; }
  void RenderFallbackContent() override {}
  AtomicString BrowsingContextContainerName() const override {
    return AtomicString();
  }
  ScrollbarMode ScrollingMode() const override { return kScrollbarAuto; }
  int MarginWidth() const override { return -1; }
  int MarginHeight() const override { return -1; }
  bool AllowFullscreen() const override { return false; }
  bool AllowPaymentRequest() const override { return false; }
  bool IsDisplayNone() const override { return false; }
  AtomicString Csp() const override { return g_null_atom; }
  const WebParsedFeaturePolicy& ContainerPolicy() const override {
    DEFINE_STATIC_LOCAL(WebParsedFeaturePolicy, container_policy, ());
    return container_policy;
  }

 private:
  // Intentionally private to prevent redundant checks when the type is
  // already DummyFrameOwner.
  bool IsLocal() const override { return false; }
  bool IsRemote() const override { return false; }
};

}  // namespace blink

#endif  // FrameOwner_h
