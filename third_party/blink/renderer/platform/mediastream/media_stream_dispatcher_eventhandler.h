// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_DISPATCHER_EVENTHANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_DISPATCHER_EVENTHANDLER_H_

#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class PLATFORM_EXPORT MediaStreamDispatcherEventHandler {
 public:
  // A device has been stopped in the browser process.
  virtual void OnDeviceStopped(const MediaStreamDevice& device) = 0;

  // Switch to the new device within the working session.
  virtual void OnDeviceChanged(const MediaStreamDevice& old_device,
                               const MediaStreamDevice& new_device) = 0;

 protected:
  virtual ~MediaStreamDispatcherEventHandler() {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_DISPATCHER_EVENTHANDLER_H_
