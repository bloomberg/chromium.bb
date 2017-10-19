// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptedIdleTaskController_h
#define ScriptedIdleTaskController_h

#include "bindings/core/v8/v8_idle_request_callback.h"
#include "core/dom/IdleDeadline.h"
#include "core/dom/SuspendableObject.h"
#include "platform/Timer.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"

namespace blink {
namespace internal {
class IdleRequestCallbackWrapper;
}

class ExecutionContext;
class IdleRequestOptions;

class CORE_EXPORT ScriptedIdleTaskController
    : public GarbageCollectedFinalized<ScriptedIdleTaskController>,
      public SuspendableObject,
      public TraceWrapperBase {
  USING_GARBAGE_COLLECTED_MIXIN(ScriptedIdleTaskController);

 public:
  static ScriptedIdleTaskController* Create(ExecutionContext* context) {
    return new ScriptedIdleTaskController(context);
  }
  ~ScriptedIdleTaskController();

  void Trace(blink::Visitor*);
  void TraceWrappers(const ScriptWrappableVisitor*) const;

  using CallbackId = int;

  // |IdleTask| is an interface type which generalizes tasks which are invoked
  // on idle. The tasks need to define what to do on idle in |invoke|.
  class IdleTask : public GarbageCollectedFinalized<IdleTask>,
                   public TraceWrapperBase {
   public:
    virtual void Trace(blink::Visitor* visitor) {}
    virtual void TraceWrappers(const ScriptWrappableVisitor* visitor) const {}
    virtual ~IdleTask() = default;
    virtual void invoke(IdleDeadline*) = 0;
  };

  // |V8IdleTask| is the adapter class for the conversion from
  // |V8IdleRequestCallback| to |IdleTask|.
  class V8IdleTask : public IdleTask {
   public:
    static V8IdleTask* Create(V8IdleRequestCallback* callback) {
      return new V8IdleTask(callback);
    }
    ~V8IdleTask() = default;
    void invoke(IdleDeadline*) override;
    void Trace(blink::Visitor*);
    void TraceWrappers(const ScriptWrappableVisitor*) const;

   private:
    explicit V8IdleTask(V8IdleRequestCallback*);
    TraceWrapperMember<V8IdleRequestCallback> callback_;
  };

  int RegisterCallback(IdleTask*, const IdleRequestOptions&);
  void CancelCallback(CallbackId);

  // SuspendableObject interface.
  void ContextDestroyed(ExecutionContext*) override;
  void Suspend() override;
  void Resume() override;

  void CallbackFired(CallbackId,
                     double deadline_seconds,
                     IdleDeadline::CallbackType);

 private:
  friend class internal::IdleRequestCallbackWrapper;
  explicit ScriptedIdleTaskController(ExecutionContext*);

  void ScheduleCallback(RefPtr<internal::IdleRequestCallbackWrapper>,
                        long long timeout_millis);

  int NextCallbackId();

  bool IsValidCallbackId(int id) {
    using Traits = HashTraits<CallbackId>;
    return !Traits::IsDeletedValue(id) &&
           !WTF::IsHashTraitsEmptyValue<Traits, CallbackId>(id);
  }

  void RunCallback(CallbackId,
                   double deadline_seconds,
                   IdleDeadline::CallbackType);

  WebScheduler* scheduler_;  // Not owned.
  HeapHashMap<CallbackId, TraceWrapperMember<IdleTask>> idle_tasks_;
  Vector<CallbackId> pending_timeouts_;
  CallbackId next_callback_id_;
  bool suspended_;
};

}  // namespace blink

#endif  // ScriptedIdleTaskController_h
