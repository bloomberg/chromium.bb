// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_LACROS_VIDEO_FRAME_HANDLER_PROXY_LACROS_H_
#define SERVICES_VIDEO_CAPTURE_LACROS_VIDEO_FRAME_HANDLER_PROXY_LACROS_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "chromeos/crosapi/mojom/video_capture.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"

namespace video_capture {

// A proxy which is used for communication between the actual handler in
// Lacros-Chrome and the video_capture::Device in Ash-Chrome. Since we
// have simplified some structures in crosapi video capture interface to reduce
// dependencies to other components, this class should also be responsible for
// translating those structures between the interfaces.
class VideoFrameHandlerProxyLacros : public crosapi::mojom::VideoFrameHandler {
 public:
  VideoFrameHandlerProxyLacros(
      mojo::PendingReceiver<crosapi::mojom::VideoFrameHandler> proxy_receiver,
      mojo::PendingRemote<mojom::VideoFrameHandler> handler_remote);
  VideoFrameHandlerProxyLacros(const VideoFrameHandlerProxyLacros&) = delete;
  VideoFrameHandlerProxyLacros& operator=(const VideoFrameHandlerProxyLacros&) =
      delete;
  ~VideoFrameHandlerProxyLacros() override;

 private:
  class AccessPermissionProxyMap;
  class VideoFrameAccessHandlerProxy;

  // crosapi::mojom::VideoFrameHandler implementation.
  void OnNewBuffer(int buffer_id,
                   crosapi::mojom::VideoBufferHandlePtr buffer_handle) override;
  void OnFrameReadyInBuffer(crosapi::mojom::ReadyFrameInBufferPtr buffer,
                            std::vector<crosapi::mojom::ReadyFrameInBufferPtr>
                                scaled_buffers) override;
  void OnBufferRetired(int buffer_id) override;
  void OnError(media::VideoCaptureError error) override;
  void OnFrameDropped(media::VideoCaptureFrameDropReason reason) override;
  void OnLog(const std::string& message) override;
  void OnStarted() override;
  void OnStartedUsingGpuDecode() override;
  void OnStopped() override;

  mojo::Receiver<crosapi::mojom::VideoFrameHandler> receiver_{this};
  mojo::Remote<mojom::VideoFrameHandler> handler_;
  scoped_refptr<AccessPermissionProxyMap> access_permission_proxy_map_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_LACROS_VIDEO_FRAME_HANDLER_PROXY_LACROS_H_
