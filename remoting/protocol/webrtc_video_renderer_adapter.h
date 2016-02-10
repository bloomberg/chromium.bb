// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_VIDEO_RENDERER_ADAPTER_H_
#define REMOTING_PROTOCOL_WEBRTC_VIDEO_RENDERER_ADAPTER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "third_party/webrtc/api/mediastreaminterface.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace remoting {
namespace protocol {

class FrameConsumer;

class WebrtcVideoRendererAdapter : public webrtc::VideoRendererInterface {
 public:
  WebrtcVideoRendererAdapter(
      scoped_refptr<webrtc::MediaStreamInterface> media_stream,
      FrameConsumer* frame_consumer);
  ~WebrtcVideoRendererAdapter() override;

  std::string label() const { return media_stream_->label(); }

  // webrtc::VideoRendererInterface implementation.
  void RenderFrame(const cricket::VideoFrame* frame) override;

 private:
  void DrawFrame(scoped_ptr<webrtc::DesktopFrame> frame);

  scoped_refptr<webrtc::MediaStreamInterface> media_stream_;
  FrameConsumer* frame_consumer_;
  uint32_t output_format_fourcc_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtrFactory<WebrtcVideoRendererAdapter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebrtcVideoRendererAdapter);
};

}  // namespace remoting
}  // namespace protocol

#endif  // REMOTING_PROTOCOL_WEBRTC_VIDEO_RENDERER_ADAPTER_H_
