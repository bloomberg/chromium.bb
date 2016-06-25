// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_VIDEO_RENDERER_ADAPTER_H_
#define REMOTING_PROTOCOL_WEBRTC_VIDEO_RENDERER_ADAPTER_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "third_party/webrtc/api/mediastreaminterface.h"
#include "third_party/webrtc/media/base/videosinkinterface.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace webrtc {
class DesktopFrame;
class VideoFrameBuffer;
}  // namespace webrtc

namespace remoting {
namespace protocol {

class FrameConsumer;
struct FrameStats;

class WebrtcVideoRendererAdapter
    : public rtc::VideoSinkInterface<cricket::VideoFrame> {
 public:
  WebrtcVideoRendererAdapter(
      scoped_refptr<webrtc::MediaStreamInterface> media_stream,
      FrameConsumer* frame_consumer);
  ~WebrtcVideoRendererAdapter() override;

  std::string label() const { return media_stream_->label(); }

  // rtc::VideoSinkInterface implementation.
  void OnFrame(const cricket::VideoFrame& frame) override;

 private:
  void HandleFrameOnMainThread(std::unique_ptr<FrameStats> stats,
                               scoped_refptr<webrtc::VideoFrameBuffer> frame);
  void DrawFrame(std::unique_ptr<FrameStats> stats,
                 std::unique_ptr<webrtc::DesktopFrame> frame);
  void FrameRendered(std::unique_ptr<FrameStats> stats);

  scoped_refptr<webrtc::MediaStreamInterface> media_stream_;
  FrameConsumer* frame_consumer_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtrFactory<WebrtcVideoRendererAdapter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebrtcVideoRendererAdapter);
};

}  // namespace remoting
}  // namespace protocol

#endif  // REMOTING_PROTOCOL_WEBRTC_VIDEO_RENDERER_ADAPTER_H_
