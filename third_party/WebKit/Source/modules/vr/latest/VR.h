// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VR_h
#define VR_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/events/EventTarget.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"

namespace blink {

class VR final : public EventTargetWithInlineData,
                 public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(VR);

 public:
  static VR* Create(LocalFrame& frame) { return new VR(frame); }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(deviceconnect);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(devicedisconnect);

  ScriptPromise getDevices(ScriptState*);

  // EventTarget overrides.
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  // ContextLifecycleObserver overrides.
  void ContextDestroyed(ExecutionContext*) override;

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit VR(LocalFrame& frame);
};

}  // namespace blink

#endif  // VR_h
