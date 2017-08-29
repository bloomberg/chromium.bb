// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaDevices_h
#define MediaDevices_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/SuspendableObject.h"
#include "core/dom/events/EventTarget.h"
#include "modules/EventTargetModules.h"
#include "modules/ModulesExport.h"
#include "platform/AsyncMethodRunner.h"
#include "platform/bindings/ActiveScriptWrappable.h"

namespace blink {

class MediaStreamConstraints;
class MediaTrackSupportedConstraints;
class ScriptState;
class UserMediaController;

class MODULES_EXPORT MediaDevices final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<MediaDevices>,
      public SuspendableObject {
  USING_GARBAGE_COLLECTED_MIXIN(MediaDevices);
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(MediaDevices, Dispose);

 public:
  static MediaDevices* Create(ExecutionContext*);
  ~MediaDevices() override;

  ScriptPromise enumerateDevices(ScriptState*);
  void getSupportedConstraints(MediaTrackSupportedConstraints& result) {}
  ScriptPromise getUserMedia(ScriptState*,
                             const MediaStreamConstraints&,
                             ExceptionState&);
  void DidChangeMediaDevices();

  // EventTarget overrides.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;
  void RemoveAllEventListeners() override;

  // ScriptWrappable
  bool HasPendingActivity() const override;

  // SuspendableObject overrides.
  void ContextDestroyed(ExecutionContext*) override;
  void Suspend() override;
  void Resume() override;

  DECLARE_VIRTUAL_TRACE();

  DEFINE_ATTRIBUTE_EVENT_LISTENER(devicechange);

 protected:
  // EventTarget overrides.
  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) override;
  void RemovedEventListener(const AtomicString& event_type,
                            const RegisteredEventListener&) override;

 private:
  explicit MediaDevices(ExecutionContext*);
  void ScheduleDispatchEvent(Event*);
  void DispatchScheduledEvent();
  void StartObserving();
  void StopObserving();
  UserMediaController* GetUserMediaController();
  void Dispose();

  bool observing_;
  bool stopped_;
  Member<AsyncMethodRunner<MediaDevices>> dispatch_scheduled_event_runner_;
  HeapVector<Member<Event>> scheduled_events_;
};

}  // namespace blink

#endif
