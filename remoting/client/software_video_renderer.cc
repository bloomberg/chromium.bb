// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/software_video_renderer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "remoting/base/util.h"
#include "remoting/client/frame_consumer.h"
#include "remoting/codec/video_decoder.h"
#include "remoting/codec/video_decoder_verbatim.h"
#include "remoting/codec/video_decoder_vpx.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/session_config.h"
#include "third_party/libyuv/include/libyuv/convert_argb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

using remoting::protocol::ChannelConfig;
using remoting::protocol::SessionConfig;

namespace remoting {

namespace {

// This class wraps a VideoDecoder and byte-swaps the pixels for compatibility
// with the android.graphics.Bitmap class.
// TODO(lambroslambrou): Refactor so that the VideoDecoder produces data
// in the right byte-order, instead of swapping it here.
class RgbToBgrVideoDecoderFilter : public VideoDecoder {
 public:
  RgbToBgrVideoDecoderFilter(scoped_ptr<VideoDecoder> parent)
      : parent_(parent.Pass()) {
  }

  bool DecodePacket(const VideoPacket& packet,
                    webrtc::DesktopFrame* frame) override {
    if (!parent_->DecodePacket(packet, frame))
      return false;
    for (webrtc::DesktopRegion::Iterator i(frame->updated_region());
         !i.IsAtEnd(); i.Advance()) {
      webrtc::DesktopRect rect = i.rect();
      uint8_t* pixels = frame->data() + (rect.top() * frame->stride()) +
                        (rect.left() * webrtc::DesktopFrame::kBytesPerPixel);
      libyuv::ABGRToARGB(pixels, frame->stride(), pixels, frame->stride(),
                         rect.width(), rect.height());
    }

    return true;
  }

 private:
  scoped_ptr<VideoDecoder> parent_;
};

scoped_ptr<webrtc::DesktopFrame> DoDecodeFrame(
    VideoDecoder* decoder,
    scoped_ptr<VideoPacket> packet,
    scoped_ptr<webrtc::DesktopFrame> frame) {
  if (!decoder->DecodePacket(*packet, frame.get()))
    frame.reset();
  return frame.Pass();
}

}  // namespace

SoftwareVideoRenderer::SoftwareVideoRenderer(
    scoped_refptr<base::SingleThreadTaskRunner> decode_task_runner,
    FrameConsumer* consumer)
    : decode_task_runner_(decode_task_runner),
      consumer_(consumer),
      weak_factory_(this) {}

SoftwareVideoRenderer::~SoftwareVideoRenderer() {
  if (decoder_)
    decode_task_runner_->DeleteSoon(FROM_HERE, decoder_.release());
}

void SoftwareVideoRenderer::OnSessionConfig(
    const protocol::SessionConfig& config) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Initialize decoder based on the selected codec.
  ChannelConfig::Codec codec = config.video_config().codec;
  if (codec == ChannelConfig::CODEC_VERBATIM) {
    decoder_.reset(new VideoDecoderVerbatim());
  } else if (codec == ChannelConfig::CODEC_VP8) {
    decoder_ = VideoDecoderVpx::CreateForVP8();
  } else if (codec == ChannelConfig::CODEC_VP9) {
    decoder_ = VideoDecoderVpx::CreateForVP9();
  } else {
    NOTREACHED() << "Invalid Encoding found: " << codec;
  }

  if (consumer_->GetPixelFormat() == FrameConsumer::FORMAT_RGBA) {
    scoped_ptr<VideoDecoder> wrapper(
        new RgbToBgrVideoDecoderFilter(decoder_.Pass()));
    decoder_ = wrapper.Pass();
  }
}

ChromotingStats* SoftwareVideoRenderer::GetStats() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return &stats_;
}

protocol::VideoStub* SoftwareVideoRenderer::GetVideoStub() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return this;
}

void SoftwareVideoRenderer::ProcessVideoPacket(scoped_ptr<VideoPacket> packet,
                                               const base::Closure& done) {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::ScopedClosureRunner done_runner(done);

  stats_.RecordVideoPacketStats(*packet);

  // If the video packet is empty then drop it. Empty packets are used to
  // maintain activity on the network.
  if (!packet->has_data() || packet->data().size() == 0) {
    return;
  }

  if (packet->format().has_screen_width() &&
      packet->format().has_screen_height()) {
    source_size_.set(packet->format().screen_width(),
                     packet->format().screen_height());
  }

  if (packet->format().has_x_dpi() && packet->format().has_y_dpi()) {
    webrtc::DesktopVector source_dpi(packet->format().x_dpi(),
                                     packet->format().y_dpi());
    if (!source_dpi.equals(source_dpi_)) {
      source_dpi_ = source_dpi;
    }
  }

  if (source_size_.is_empty()) {
    LOG(ERROR) << "Received VideoPacket with unknown size.";
    return;
  }

  scoped_ptr<webrtc::DesktopFrame> frame =
      consumer_->AllocateFrame(source_size_);
  frame->set_dpi(source_dpi_);

  base::PostTaskAndReplyWithResult(
      decode_task_runner_.get(), FROM_HERE,
      base::Bind(&DoDecodeFrame, decoder_.get(), base::Passed(&packet),
                 base::Passed(&frame)),
      base::Bind(&SoftwareVideoRenderer::RenderFrame,
                 weak_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                 done_runner.Release()));
}

void SoftwareVideoRenderer::RenderFrame(
    base::TimeTicks decode_start_time,
    const base::Closure& done,
    scoped_ptr<webrtc::DesktopFrame> frame) {
  DCHECK(thread_checker_.CalledOnValidThread());

  stats_.RecordDecodeTime(
      (base::TimeTicks::Now() - decode_start_time).InMilliseconds());

  if (!frame) {
    if (!done.is_null())
      done.Run();
    return;
  }

  consumer_->DrawFrame(
      frame.Pass(),
      base::Bind(&SoftwareVideoRenderer::OnFrameRendered,
                 weak_factory_.GetWeakPtr(), base::TimeTicks::Now(), done));
}

void SoftwareVideoRenderer::OnFrameRendered(base::TimeTicks paint_start_time,
                                            const base::Closure& done) {
  DCHECK(thread_checker_.CalledOnValidThread());

  stats_.RecordPaintTime(
      (base::TimeTicks::Now() - paint_start_time).InMilliseconds());

  if (!done.is_null())
    done.Run();
}

}  // namespace remoting
