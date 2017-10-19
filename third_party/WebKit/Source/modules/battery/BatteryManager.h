// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BatteryManager_h
#define BatteryManager_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseProperty.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/SuspendableObject.h"
#include "core/frame/PlatformEventController.h"
#include "modules/EventTargetModules.h"
#include "modules/battery/battery_status.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class BatteryManager final : public EventTargetWithInlineData,
                             public ActiveScriptWrappable<BatteryManager>,
                             public SuspendableObject,
                             public PlatformEventController {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(BatteryManager);

 public:
  static BatteryManager* Create(ExecutionContext*);
  ~BatteryManager() override;

  // Returns a promise object that will be resolved with this BatteryManager.
  ScriptPromise StartRequest(ScriptState*);

  // EventTarget implementation.
  const WTF::AtomicString& InterfaceName() const override {
    return EventTargetNames::BatteryManager;
  }
  ExecutionContext* GetExecutionContext() const override {
    return ContextLifecycleObserver::GetExecutionContext();
  }

  bool charging();
  double chargingTime();
  double dischargingTime();
  double level();

  DEFINE_ATTRIBUTE_EVENT_LISTENER(chargingchange);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(chargingtimechange);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(dischargingtimechange);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(levelchange);

  // Inherited from PlatformEventController.
  void DidUpdateData() override;
  void RegisterWithDispatcher() override;
  void UnregisterWithDispatcher() override;
  bool HasLastData() override;

  // SuspendableObject implementation.
  void Suspend() override;
  void Resume() override;
  void ContextDestroyed(ExecutionContext*) override;

  // ScriptWrappable implementation.
  bool HasPendingActivity() const final;

  virtual void Trace(blink::Visitor*);

 private:
  explicit BatteryManager(ExecutionContext*);

  using BatteryProperty = ScriptPromiseProperty<Member<BatteryManager>,
                                                Member<BatteryManager>,
                                                Member<DOMException>>;
  Member<BatteryProperty> battery_property_;
  BatteryStatus battery_status_;
};

}  // namespace blink

#endif  // BatteryManager_h
