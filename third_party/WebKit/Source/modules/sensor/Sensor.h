// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Sensor_h
#define Sensor_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/DOMHighResTimeStamp.h"
#include "core/dom/DOMTimeStamp.h"
#include "core/dom/SuspendableObject.h"
#include "core/frame/PlatformEventController.h"
#include "modules/EventTargetModules.h"
#include "modules/sensor/SensorOptions.h"
#include "modules/sensor/SensorProxy.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class SensorReading;

class Sensor : public EventTargetWithInlineData,
               public ActiveScriptWrappable<Sensor>,
               public ContextLifecycleObserver,
               public SensorProxy::Observer {
  USING_GARBAGE_COLLECTED_MIXIN(Sensor);
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum class SensorState { Unconnected, Activating, Activated, Idle, Errored };

  ~Sensor() override;

  void start();
  void stop();

  // EventTarget overrides.
  const AtomicString& interfaceName() const override {
    return EventTargetNames::Sensor;
  }
  ExecutionContext* getExecutionContext() const override {
    return ContextLifecycleObserver::getExecutionContext();
  }

  // Getters
  String state() const;
  DOMHighResTimeStamp timestamp(ScriptState*, bool& isNull) const;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(change);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(activate);

  // ActiveScriptWrappable overrides.
  bool hasPendingActivity() const override;

  DECLARE_VIRTUAL_TRACE();

 protected:
  Sensor(ExecutionContext*,
         const SensorOptions&,
         ExceptionState&,
         device::mojom::blink::SensorType);

  using SensorConfigurationPtr = device::mojom::blink::SensorConfigurationPtr;
  using SensorConfiguration = device::mojom::blink::SensorConfiguration;

  // The default implementation will init frequency configuration parameter,
  // concrete sensor implementations can override this method to handle other
  // parameters if needed.
  virtual SensorConfigurationPtr createSensorConfig();
  double readingValue(int index, bool& isNull) const;

 private:
  void initSensorProxyIfNeeded();

  // ContextLifecycleObserver overrides.
  void contextDestroyed(ExecutionContext*) override;

  // SensorController::Observer overrides.
  void onSensorInitialized() override;
  void onSensorReadingChanged(double timestamp) override;
  void onSensorError(ExceptionCode,
                     const String& sanitizedMessage,
                     const String& unsanitizedMessage) override;

  void onStartRequestCompleted(bool);
  void onStopRequestCompleted(bool);

  void startListening();
  void stopListening();

  void updateState(SensorState newState);
  void reportError(ExceptionCode = UnknownError,
                   const String& sanitizedMessage = String(),
                   const String& unsanitizedMessage = String());

  void notifySensorReadingChanged();
  void notifyOnActivate();
  void notifyError(DOMException* error);

  bool canReturnReadings() const;

 private:
  SensorOptions m_sensorOptions;
  device::mojom::blink::SensorType m_type;
  SensorState m_state;
  Member<SensorProxy> m_sensorProxy;
  device::SensorReading m_storedData;
  SensorConfigurationPtr m_configuration;
  double m_lastUpdateTimestamp;
};

}  // namespace blink

#endif  // Sensor_h
