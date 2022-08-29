// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_DESKTOP_CAPTURER_H_
#define REMOTING_PROTOCOL_DESKTOP_CAPTURER_H_

#include "base/callback.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_metadata.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace remoting {

// An interface extension to make synchronous methods on webrtc::DesktopCapturer
// asynchronous by allowing the new wrapper methods to accept callbacks.
class DesktopCapturer : public webrtc::DesktopCapturer {
 public:
#if defined(WEBRTC_USE_GIO)
  virtual void GetMetadataAsync(
      base::OnceCallback<void(webrtc::DesktopCaptureMetadata)> callback) {}
#endif
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_DESKTOP_CAPTURER_H_
