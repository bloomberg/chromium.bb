/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DeviceMotionDispatcher_h
#define DeviceMotionDispatcher_h

#include "base/memory/scoped_refptr.h"
#include "core/frame/PlatformEventDispatcher.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/device_orientation/WebDeviceMotionListener.h"

namespace device {
class MotionData;
}

namespace blink {

class DeviceMotionData;

// This class listens to device motion data and notifies all registered
// controllers.
class DeviceMotionDispatcher final
    : public GarbageCollectedFinalized<DeviceMotionDispatcher>,
      public PlatformEventDispatcher,
      public WebDeviceMotionListener {
  USING_GARBAGE_COLLECTED_MIXIN(DeviceMotionDispatcher);

 public:
  static DeviceMotionDispatcher& Instance();
  ~DeviceMotionDispatcher() override;

  // Note that the returned object is owned by this class.
  const DeviceMotionData* LatestDeviceMotionData();

  // Inherited from WebDeviceMotionListener.
  void DidChangeDeviceMotion(const device::MotionData&) override;

  virtual void Trace(blink::Visitor*);

 private:
  DeviceMotionDispatcher();

  // Inherited from PlatformEventDispatcher.
  void StartListening() override;
  void StopListening() override;

  Member<DeviceMotionData> last_device_motion_data_;
};

}  // namespace blink

#endif  // DeviceMotionDispatcher_h
