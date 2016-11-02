// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SensorReading_h
#define SensorReading_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMHighResTimeStamp.h"
#include "core/dom/DOMTimeStamp.h"
#include "modules/sensor/SensorProxy.h"

namespace blink {

class ScriptState;

class SensorReading : public GarbageCollectedFinalized<SensorReading>,
                      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DECLARE_VIRTUAL_TRACE();

  DOMHighResTimeStamp timeStamp(ScriptState*) const;

  // Returns 'true' if the current reading value is different than the given
  // previous one; otherwise returns 'false'.
  virtual bool isReadingUpdated(const SensorProxy::Reading& previous) const = 0;

  virtual ~SensorReading();

 protected:
  explicit SensorReading(SensorProxy*);

 protected:
  Member<SensorProxy> m_sensorProxy;
};

}  // namepsace blink

#endif  // SensorReading_h
