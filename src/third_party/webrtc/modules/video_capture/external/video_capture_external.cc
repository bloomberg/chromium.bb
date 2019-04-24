/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/scoped_refptr.h"
#include "modules/video_capture/video_capture.h"
#include "modules/video_capture/video_capture_impl.h"
#include "rtc_base/ref_counted_object.h"

namespace webrtc {

namespace videocapturemodule {

rtc::scoped_refptr<VideoCaptureModule> VideoCaptureImpl::Create(
    const char* deviceUniqueIdUTF8) {
  return new rtc::RefCountedObject<VideoCaptureImpl>();
}

}  // namespace videocapturemodule

}  // namespace webrtc
