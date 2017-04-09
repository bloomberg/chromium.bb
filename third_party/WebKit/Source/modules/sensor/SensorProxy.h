// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SensorProxy_h
#define SensorProxy_h

#include "core/dom/ExceptionCode.h"
#include "core/page/PageVisibilityObserver.h"
#include "device/generic_sensor/public/cpp/sensor_reading.h"
#include "device/generic_sensor/public/interfaces/sensor.mojom-blink.h"
#include "device/generic_sensor/public/interfaces/sensor_provider.mojom-blink.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"

namespace blink {

class SensorProviderProxy;
class SensorReading;
class SensorReadingUpdater;

// This class wraps 'Sensor' mojo interface and used by multiple
// JS sensor instances of the same type (within a single frame).
class SensorProxy final : public GarbageCollectedFinalized<SensorProxy>,
                          public device::mojom::blink::SensorClient,
                          public PageVisibilityObserver {
  USING_GARBAGE_COLLECTED_MIXIN(SensorProxy);
  USING_PRE_FINALIZER(SensorProxy, Dispose);
  WTF_MAKE_NONCOPYABLE(SensorProxy);

 public:
  class Observer : public GarbageCollectedMixin {
   public:
    // Has valid 'Sensor' binding, {add, remove}Configuration()
    // methods can be called.
    virtual void OnSensorInitialized() {}
    // Platfrom sensor reading has changed.
    virtual void OnSensorReadingChanged() {}
    // Observer should send 'onchange' event if needed.
    // The 'notifySensorChanged' calls are in sync with rAF.
    // Currently, we decide whether to send 'onchange' event based on the
    // time elapsed from the previous notification.
    // TODO: Reconsider this after https://github.com/w3c/sensors/issues/152
    // is resolved.
    // |timestamp| Reference timestamp in seconds of the moment when
    // sensor reading was updated from the buffer.
    // Note: |timestamp| values are only used to calculate elapsed time
    // between shared buffer readings. These values *do not* correspond
    // to sensor reading timestamps which are obtained on platform side.
    virtual void NotifySensorChanged(double timestamp) {}
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

  // Is watching new reading data (initialized, not suspended and has
  // configurations added).
  bool IsActive() const;

  void AddConfiguration(device::mojom::blink::SensorConfigurationPtr,
                        std::unique_ptr<Function<void(bool)>>);

  void RemoveConfiguration(device::mojom::blink::SensorConfigurationPtr);

  void Suspend();
  void Resume();

  device::mojom::blink::SensorType GetType() const { return type_; }
  device::mojom::blink::ReportingMode GetReportingMode() const { return mode_; }

  // Note: the returned value is reset after updateSensorReading() call.
  const device::SensorReading& Reading() const { return reading_; }

  const device::mojom::blink::SensorConfiguration* DefaultConfig() const;

  const std::pair<double, double>& FrequencyLimits() const {
    return frequency_limits_;
  }

  Document* GetDocument() const;
  const WTF::Vector<double>& FrequenciesUsed() const {
    return frequencies_used_;
  }

  DECLARE_VIRTUAL_TRACE();

 private:
  friend class SensorProviderProxy;
  friend class SensorReadingUpdaterContinuous;
  friend class SensorReadingUpdaterOnChange;
  SensorProxy(device::mojom::blink::SensorType, SensorProviderProxy*, Page*);

  // Updates sensor reading from shared buffer.
  void UpdateSensorReading();
  void NotifySensorChanged(double timestamp);

  // device::mojom::blink::SensorClient overrides.
  void RaiseError() override;
  void SensorReadingChanged() override;

  // PageVisibilityObserver overrides.
  void PageVisibilityChanged() override;

  // Generic handler for a fatal error.
  void HandleSensorError();

  // mojo call callbacks.
  void OnSensorCreated(device::mojom::blink::SensorInitParamsPtr,
                       device::mojom::blink::SensorClientRequest);
  void OnAddConfigurationCompleted(
      double frequency,
      std::unique_ptr<Function<void(bool)>> callback,
      bool result);
  void OnRemoveConfigurationCompleted(double frequency, bool result);

  bool TryReadFromBuffer(device::SensorReading& result);
  void OnAnimationFrame(double timestamp);

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
  bool suspended_;
  device::SensorReading reading_;
  std::pair<double, double> frequency_limits_;

  Member<SensorReadingUpdater> reading_updater_;
  WTF::Vector<double> frequencies_used_;
  double last_raf_timestamp_;

  using ReadingBuffer = device::SensorReadingSharedBuffer;
  static_assert(
      sizeof(ReadingBuffer) ==
          device::mojom::blink::SensorInitParams::kReadBufferSizeForTests,
      "Check reading buffer size for tests");
};

}  // namespace blink

#endif  // SensorProxy_h
