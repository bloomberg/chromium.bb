// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_AURA_DESKTOP_CAPTURER_H_
#define REMOTING_HOST_CHROMEOS_AURA_DESKTOP_CAPTURER_H_

#include "base/memory/weak_ptr.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"

namespace cc {
class CopyOutputResult;
}  // namespace cc

namespace aura {
class Window;
}  // namespace aura

namespace remoting {

// A webrtc::DesktopCapturer that captures pixels from the root window of the
// Aura Shell.  This is implemented by requesting the layer and its substree to
// be rendered to a given data structure.  Start() and Capture() must be called
// on the Browser UI thread.
class AuraDesktopCapturer : public webrtc::DesktopCapturer {
 public:
  AuraDesktopCapturer();
  virtual ~AuraDesktopCapturer();

  // webrtc::DesktopCapturer implementation.
  virtual void Start(webrtc::DesktopCapturer::Callback* callback) OVERRIDE;
  virtual void Capture(const webrtc::DesktopRegion& region) OVERRIDE;

 private:
  friend class AuraDesktopCapturerTest;

  // Called when a copy of the layer is captured.
  void OnFrameCaptured(scoped_ptr<cc::CopyOutputResult> result);

  // Points to the callback passed to webrtc::DesktopCapturer::Start().
  webrtc::DesktopCapturer::Callback* callback_;

  // The root window of the Aura Shell.
  aura::Window* desktop_window_;

  base::WeakPtrFactory<AuraDesktopCapturer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AuraDesktopCapturer);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_AURA_DESKTOP_CAPTURER_H_
