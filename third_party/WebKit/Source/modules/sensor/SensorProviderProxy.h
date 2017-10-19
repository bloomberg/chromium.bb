// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SensorProviderProxy_h
#define SensorProviderProxy_h

#include "core/frame/LocalFrame.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "services/device/public/interfaces/sensor.mojom-blink.h"
#include "services/device/public/interfaces/sensor_provider.mojom-blink.h"

namespace blink {

class SensorProxy;

// This class wraps 'SensorProvider' mojo interface and it manages
// 'SensorProxy' instances.
class SensorProviderProxy final
    : public GarbageCollectedFinalized<SensorProviderProxy>,
      public Supplement<LocalFrame> {
  USING_GARBAGE_COLLECTED_MIXIN(SensorProviderProxy);
  WTF_MAKE_NONCOPYABLE(SensorProviderProxy);

 public:
  static SensorProviderProxy* From(LocalFrame*);

  ~SensorProviderProxy();

  SensorProxy* CreateSensorProxy(device::mojom::blink::SensorType, Page*);

  SensorProxy* GetSensorProxy(device::mojom::blink::SensorType);

  virtual void Trace(blink::Visitor*);

 private:
  friend class SensorProxy;  // To call getSensorProvider().

  explicit SensorProviderProxy(LocalFrame&);
  static const char* SupplementName();
  void InitializeIfNeeded();
  bool IsInitialized() const { return sensor_provider_.is_bound(); }

  device::mojom::blink::SensorProvider* GetSensorProvider() const {
    return sensor_provider_.get();
  }

  void OnSensorProviderConnectionError();

  using SensorsSet = HeapHashSet<WeakMember<SensorProxy>>;
  SensorsSet sensor_proxies_;

  device::mojom::blink::SensorProviderPtr sensor_provider_;
};

}  // namespace blink

#endif  // SensorProviderProxy_h
