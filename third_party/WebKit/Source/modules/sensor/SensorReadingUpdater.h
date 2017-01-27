// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SensorReadingUpdater_h
#define SensorReadingUpdater_h

#include "device/generic_sensor/public/interfaces/sensor_provider.mojom-blink.h"
#include "platform/heap/Handle.h"

namespace blink {

class Document;
class SensorProxy;

// This class encapsulates sensor reading update notification logic.
class SensorReadingUpdater : public GarbageCollected<SensorReadingUpdater> {
 public:
  static SensorReadingUpdater* create(SensorProxy*,
                                      device::mojom::blink::ReportingMode);

  virtual void start();

  DECLARE_VIRTUAL_TRACE();

 protected:
  explicit SensorReadingUpdater(SensorProxy*);
  void enqueueAnimationFrameTask();
  virtual void onAnimationFrameInternal() = 0;

  Member<SensorProxy> m_sensorProxy;
  WeakMember<Document> m_document;
  bool m_hasPendingAnimationFrameTask;

 private:
  void onAnimationFrame();
};

}  // namespace blink

#endif  // SensorReadingUpdater_h
