// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_VIDEO_RENDERER_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_VIDEO_RENDERER_H_

#include "remoting/client/video_renderer.h"

namespace webrtc {
class DesktopSize;
class DesktopVector;
class DesktopRegion;
}  // namespace webrtc

namespace pp {
class Instance;
class View;
}  // namespace pp

namespace remoting {

class ClientContext;

namespace protocol {
class PerformanceTracker;
}  // namespace protocol

// Interface for video renderers that render video in pepper plugin.
class PepperVideoRenderer : public VideoRenderer {
 public:
  class EventHandler {
   public:
    EventHandler() {}
    virtual ~EventHandler() {}

    // Called if video decoding fails, for any reason.
    virtual void OnVideoDecodeError() = 0;

    // Called when the first frame is received.
    virtual void OnVideoFirstFrameReceived() = 0;

    // Called when stream size changes.
    virtual void OnVideoSize(const webrtc::DesktopSize& size,
                             const webrtc::DesktopVector& dpi) = 0;

    // Called when desktop shape changes. |shape| should be NULL if frames are
    // un-shaped.
    virtual void OnVideoShape(const webrtc::DesktopRegion* shape) = 0;

    // Called with each frame's updated region, if EnableDebugDirtyRegion(true)
    // was called.
    virtual void OnVideoFrameDirtyRegion(
        const webrtc::DesktopRegion& dirty_region) = 0;
  };

  // Initializes the renderer. |instance| and |event_handler| must outlive the
  // renderer. Returns false if the renderer cannot be initialized.
  virtual bool Initialize(pp::Instance* instance,
                          const ClientContext& context,
                          EventHandler* event_handler,
                          protocol::PerformanceTracker* perf_tracker) = 0;

  // Must be called whenever the plugin view changes.
  virtual void OnViewChanged(const pp::View& view) = 0;

  // Enables or disables delivery of dirty region information to the
  // EventHandler, for debugging purposes.
  virtual void EnableDebugDirtyRegion(bool enable) = 0;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_VIDEO_RENDERER_H_
