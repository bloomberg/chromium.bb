// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SensorProxy_h
#define SensorProxy_h

#include "core/dom/ExceptionCode.h"
#include "core/page/FocusChangedObserver.h"
#include "core/page/PageVisibilityObserver.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/Supplementable.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading_shared_buffer_reader.h"
#include "services/device/public/interfaces/sensor.mojom-blink.h"
#include "services/device/public/interfaces/sensor_provider.mojom-blink.h"

namespace blink {

class SensorProviderProxy;

// This class wraps 'Sensor' mojo interface and used by multiple
// JS sensor instances of the same type (within a single frame).
class SensorProxy final : public GarbageCollectedFinalized<SensorProxy>,
                          public device::mojom::blink::SensorClient,
                          public PageVisibilityObserver,
                          public FocusChangedObserver {
  USING_GARBAGE_COLLECTED_MIXIN(SensorProxy);
  USING_PRE_FINALIZER(SensorProxy, Dispose);
  WTF_MAKE_NONCOPYABLE(SensorProxy);

 public:
  class Observer : public GarbageCollectedMixin {
   public:
    // Has valid 'Sensor' binding, {add, remove}Configuration()
    // methods can be called.
    virtual void OnSensorInitialized() {}
    // Observer should update its cached reading and send 'onchange'
    // event if needed.
    virtual void OnSensorReadingChanged() {}
    // An error has occurred.
    virtual void OnSensorError(ExceptionCode,
                               const String& sanitized_message,
                               const String& unsanitized_message) {}
  };

  ~SensorProxy();

  void Dispose();

  void AddObserver(Observer*);
  void RemoveObserver(Observer*);

  void Initialize();

  bool IsInitializing() const { return state_ == kInitializing; }
  bool IsInitialized() const { return state_ == kInitialized; }

  void AddConfiguration(device::mojom::blink::SensorConfigurationPtr,
                        Function<void(bool)>);

  void RemoveConfiguration(device::mojom::blink::SensorConfigurationPtr);

  void Suspend();
  void Resume();

  device::mojom::blink::SensorType type() const { return type_; }

  // Note: the returned value is reset after updateSensorReading() call.
  const device::SensorReading& reading() const { return reading_; }

  const device::mojom::blink::SensorConfiguration* DefaultConfig() const;

  const std::pair<double, double>& FrequencyLimits() const {
    return frequency_limits_;
  }

  DECLARE_VIRTUAL_TRACE();

 private:
  friend class SensorProviderProxy;
  SensorProxy(device::mojom::blink::SensorType, SensorProviderProxy*, Page*);

  // Updates sensor reading from shared buffer.
  void UpdateSensorReading();
  void NotifySensorChanged(double timestamp);

  // device::mojom::blink::SensorClient overrides.
  void RaiseError() override;
  void SensorReadingChanged() override;

  // PageVisibilityObserver overrides.
  void PageVisibilityChanged() override;

  // FocusChangedObserver overrides.
  void FocusedFrameChanged() override;

  // Generic handler for a fatal error.
  void HandleSensorError();

  // mojo call callbacks.
  void OnSensorCreated(device::mojom::blink::SensorInitParamsPtr,
                       device::mojom::blink::SensorClientRequest);

  void OnPollingTimer(TimerBase*);

  // Returns 'true' if readings should be propagated to Observers
  // (i.e. proxy is initialized, not suspended and has active configurations);
  // returns 'false' otherwise.
  bool ShouldProcessReadings() const;

  // Starts or stops polling timer.
  void UpdatePollingStatus();

  // Suspends or resumes the wrapped sensor.
  void UpdateSuspendedStatus();

  void RemoveActiveFrequency(double frequency);
  void AddActiveFrequency(double frequency);

  device::mojom::blink::SensorType type_;
  device::mojom::blink::ReportingMode mode_;
  Member<SensorProviderProxy> provider_;
  using ObserversSet = HeapHashSet<WeakMember<Observer>>;
  ObserversSet observers_;

  device::mojom::blink::SensorPtr sensor_;
  device::mojom::blink::SensorConfigurationPtr default_config_;
  mojo::Binding<device::mojom::blink::SensorClient> client_binding_;

  enum State { kUninitialized, kInitializing, kInitialized };
  State state_;
  mojo::ScopedSharedBufferHandle shared_buffer_handle_;
  mojo::ScopedSharedBufferMapping shared_buffer_;
  std::unique_ptr<device::SensorReadingSharedBufferReader>
      shared_buffer_reader_;
  bool suspended_;
  device::SensorReading reading_;
  std::pair<double, double> frequency_limits_;

  WTF::Vector<double> active_frequencies_;
  TaskRunnerTimer<SensorProxy> polling_timer_;

  using ReadingBuffer = device::SensorReadingSharedBuffer;
  static_assert(
      sizeof(ReadingBuffer) ==
          device::mojom::blink::SensorInitParams::kReadBufferSizeForTests,
      "Check reading buffer size for tests");
};

}  // namespace blink

#endif  // SensorProxy_h
