// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaDevices_h
#define MediaDevices_h

#include "base/callback.h"
#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "core/dom/PausableObject.h"
#include "core/dom/events/EventTarget.h"
#include "modules/EventTargetModules.h"
#include "modules/ModulesExport.h"
#include "modules/mediastream/MediaDeviceInfo.h"
#include "platform/AsyncMethodRunner.h"
#include "platform/heap/HeapAllocator.h"
#include "public/platform/modules/mediastream/media_devices.mojom-blink.h"

namespace blink {

class LocalFrame;
class MediaStreamConstraints;
class MediaTrackSupportedConstraints;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;
class UserMediaController;

class MODULES_EXPORT MediaDevices final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<MediaDevices>,
      public PausableObject {
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

  // PausableObject overrides.
  void ContextDestroyed(ExecutionContext*) override;
  void Pause() override;
  void Unpause() override;

  // Callback for testing only.
  using EnumerateDevicesTestCallback =
      base::OnceCallback<void(const MediaDeviceInfoVector&)>;

  void SetDispatcherHostForTesting(mojom::blink::MediaDevicesDispatcherHostPtr);

  void SetEnumerateDevicesCallbackForTesting(
      EnumerateDevicesTestCallback test_callback) {
    enumerate_devices_test_callback_ = std::move(test_callback);
  }

  void SetConnectionErrorCallbackForTesting(base::OnceClosure test_callback) {
    connection_error_test_callback_ = std::move(test_callback);
  }

  virtual void Trace(blink::Visitor*);

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
  void DevicesEnumerated(ScriptPromiseResolver*,
                         Vector<Vector<mojom::blink::MediaDeviceInfoPtr>>);
  void OnDispatcherHostConnectionError();
  const mojom::blink::MediaDevicesDispatcherHostPtr& GetDispatcherHost(
      LocalFrame*);

  bool observing_;
  bool stopped_;
  // Async runner may be null when there is no valid execution context.
  // No async work may be posted in this scenario.
  Member<AsyncMethodRunner<MediaDevices>> dispatch_scheduled_event_runner_;
  HeapVector<Member<Event>> scheduled_events_;
  mojom::blink::MediaDevicesDispatcherHostPtr dispatcher_host_;
  HeapHashSet<Member<ScriptPromiseResolver>> requests_;

  EnumerateDevicesTestCallback enumerate_devices_test_callback_;
  base::OnceClosure connection_error_test_callback_;
};

}  // namespace blink

#endif
