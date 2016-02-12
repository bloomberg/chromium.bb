// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_CAPTURER_PROXY_H_
#define REMOTING_HOST_DESKTOP_CAPTURER_PROXY_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

namespace protocol {
class CursorShapeInfo;
}  // namespace protocol

// DesktopCapturerProxy is responsible for calling webrtc::DesktopCapturer on
// the capturer thread and then returning results to the caller's thread.
class DesktopCapturerProxy : public webrtc::DesktopCapturer {
 public:
  DesktopCapturerProxy(
      scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
      scoped_ptr<webrtc::DesktopCapturer> capturer);
  ~DesktopCapturerProxy() override;

  // webrtc::DesktopCapturer interface.
  void Start(Callback* callback) override;
  void SetSharedMemoryFactory(rtc::scoped_ptr<webrtc::SharedMemoryFactory>
                                  shared_memory_factory) override;
  void Capture(const webrtc::DesktopRegion& rect) override;

 private:
  class Core;

  void OnFrameCaptured(scoped_ptr<webrtc::DesktopFrame> frame);

  base::ThreadChecker thread_checker_;

  scoped_ptr<Core> core_;
  scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;
  webrtc::DesktopCapturer::Callback* callback_;

  base::WeakPtrFactory<DesktopCapturerProxy> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DesktopCapturerProxy);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_CAPTURER_PROXY_H_
