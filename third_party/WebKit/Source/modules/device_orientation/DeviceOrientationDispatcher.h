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

#ifndef DeviceOrientationDispatcher_h
#define DeviceOrientationDispatcher_h

#include "base/memory/scoped_refptr.h"
#include "core/frame/PlatformEventDispatcher.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebPlatformEventType.h"
#include "public/platform/modules/device_orientation/WebDeviceOrientationListener.h"

namespace device {
class OrientationData;
}

namespace blink {

class DeviceOrientationData;

// This class listens to device orientation data and notifies all registered
// controllers.
class DeviceOrientationDispatcher final
    : public GarbageCollectedFinalized<DeviceOrientationDispatcher>,
      public PlatformEventDispatcher,
      public WebDeviceOrientationListener {
  USING_GARBAGE_COLLECTED_MIXIN(DeviceOrientationDispatcher);

 public:
  static DeviceOrientationDispatcher& Instance(bool absolute);
  ~DeviceOrientationDispatcher() override;

  // Note that the returned object is owned by this class.
  // FIXME: make the return value const, see crbug.com/233174.
  DeviceOrientationData* LatestDeviceOrientationData();

  // Inherited from WebDeviceOrientationListener.
  void DidChangeDeviceOrientation(const device::OrientationData&) override;

  virtual void Trace(blink::Visitor*);

 private:
  explicit DeviceOrientationDispatcher(bool absolute);

  // Inherited from PlatformEventDispatcher.
  void StartListening() override;
  void StopListening() override;

  WebPlatformEventType GetWebPlatformEventType() const;

  const bool absolute_;
  Member<DeviceOrientationData> last_device_orientation_data_;
};

}  // namespace blink

#endif  // DeviceOrientationDispatcher_h
