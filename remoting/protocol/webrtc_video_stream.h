// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_VIDEO_STREAM_H_
#define REMOTING_PROTOCOL_WEBRTC_VIDEO_STREAM_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/video_stream.h"

namespace webrtc {
class DesktopSize;
class DesktopCapturer;
class MediaStreamInterface;
class PeerConnectionInterface;
class PeerConnectionFactoryInterface;
class VideoTrackInterface;
}  // namespace webrtc

namespace remoting {
namespace protocol {

class WebrtcVideoCapturerAdapter;

class WebrtcVideoStream : public VideoStream {
 public:
  WebrtcVideoStream();
  ~WebrtcVideoStream() override;

  bool Start(std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer,
             scoped_refptr<webrtc::PeerConnectionInterface> connection,
             scoped_refptr<webrtc::PeerConnectionFactoryInterface>
                 peer_connection_factory);

  // VideoStream interface.
  void Pause(bool pause) override;
  void OnInputEventReceived(int64_t event_timestamp) override;
  void SetLosslessEncode(bool want_lossless) override;
  void SetLosslessColor(bool want_lossless) override;
  void SetSizeCallback(const SizeCallback& size_callback) override;

 private:
  scoped_refptr<webrtc::PeerConnectionInterface> connection_;
  scoped_refptr<webrtc::MediaStreamInterface> stream_;

  // Owned by the |stream_|.
  base::WeakPtr<WebrtcVideoCapturerAdapter> capturer_adapter_;

  DISALLOW_COPY_AND_ASSIGN(WebrtcVideoStream);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_VIDEO_STREAM_H_
