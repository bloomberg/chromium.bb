// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_WAYLAND_DESKTOP_CAPTURER_H_
#define REMOTING_HOST_LINUX_WAYLAND_DESKTOP_CAPTURER_H_

#include "third_party/webrtc/modules/desktop_capture/desktop_capture_metadata.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/linux/wayland/base_capturer_pipewire.h"

namespace remoting {

class WaylandDesktopCapturer : public webrtc::DesktopCapturer,
                               public webrtc::ScreenCastPortal::PortalNotifier {
 public:
  explicit WaylandDesktopCapturer(const webrtc::DesktopCaptureOptions& options);
  WaylandDesktopCapturer(const WaylandDesktopCapturer&) = delete;
  WaylandDesktopCapturer& operator=(const WaylandDesktopCapturer&) = delete;
  ~WaylandDesktopCapturer() override;

  // DesktopCapturer interface.
  void Start(Callback* delegate) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;

  // PortalNotifier interface.
  void OnScreenCastRequestResult(webrtc::xdg_portal::RequestResponse result,
                                 uint32_t stream_node_id,
                                 int fd) override;
  void OnScreenCastSessionClosed() override;

#if defined(WEBRTC_USE_GIO)
  // Gets session related details in the metadata so that input injection
  // module can make use of the same remote desktop session to inject inputs
  // on the remote host. Valid metadata can only be populated after the
  // capturer has been started using call to `Start()`.
  webrtc::DesktopCaptureMetadata GetMetadata() override;
#endif

 private:
  webrtc::BaseCapturerPipeWire base_capturer_pipewire_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_WAYLAND_DESKTOP_CAPTURER_H_
