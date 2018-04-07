// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SENSOR_SENSOR_PROVIDER_PROXY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SENSOR_SENSOR_PROVIDER_PROXY_H_

#include "services/device/public/mojom/sensor.mojom-blink.h"
#include "services/device/public/mojom/sensor_provider.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

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
  static const char kSupplementName[];

  static SensorProviderProxy* From(LocalFrame*);

  ~SensorProviderProxy();

  SensorProxy* CreateSensorProxy(device::mojom::blink::SensorType, Page*);

  SensorProxy* GetSensorProxy(device::mojom::blink::SensorType);

  virtual void Trace(blink::Visitor*);

 private:
  friend class SensorProxy;  // To call getSensorProvider().

  explicit SensorProviderProxy(LocalFrame&);
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

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SENSOR_SENSOR_PROVIDER_PROXY_H_
