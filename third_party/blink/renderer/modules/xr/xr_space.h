// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SPACE_H_

#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class XRSession;

class XRSpace : public EventTargetWithInlineData {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit XRSpace(XRSession*);
  ~XRSpace() override;

  DOMFloat32Array* getTransformTo(XRSpace*) const;

  XRSession* session() const { return session_; }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(reset, kReset);

  // EventTarget overrides.
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  void Trace(blink::Visitor*) override;

 private:
  const Member<XRSession> session_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SPACE_H_
