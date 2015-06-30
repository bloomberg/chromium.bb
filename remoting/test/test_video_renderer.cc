// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/test_video_renderer.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "remoting/codec/video_decoder.h"
#include "remoting/codec/video_decoder_verbatim.h"
#include "remoting/codec/video_decoder_vpx.h"
#include "remoting/proto/video.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {
namespace test {

// Implements video decoding functionality.
class TestVideoRenderer::Core {
 public:
  Core();
  ~Core();

  // Initializes the internal structures of the class.
  void Initialize();

  // Used to decode video packets.
  void ProcessVideoPacket(scoped_ptr<VideoPacket> packet,
                          const base::Closure& done);

  // Initialize a decoder to decode video packets.
  void SetCodecForDecoding(const protocol::ChannelConfig::Codec codec);

  // Returns a copy of the current buffer.
  scoped_ptr<webrtc::DesktopFrame> GetBufferForTest() const;

 private:
  // Used to ensure Core methods are called on the same thread.
  base::ThreadChecker thread_checker_;

  // Used to decode video packets.
  scoped_ptr<VideoDecoder> decoder_;

  // Updated region of the current desktop frame compared to previous one.
  webrtc::DesktopRegion updated_region_;

  // Screen size of the remote host.
  webrtc::DesktopSize screen_size_;

  // Used to post tasks back to main thread.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // Used to store decoded video frame.
  scoped_ptr<webrtc::DesktopFrame> buffer_;

  // Protects access to |buffer_|.
  mutable base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

TestVideoRenderer::Core::Core()
    : main_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  thread_checker_.DetachFromThread();
}

TestVideoRenderer::Core::~Core() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void TestVideoRenderer::Core::Initialize() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void TestVideoRenderer::Core::SetCodecForDecoding(
    const protocol::ChannelConfig::Codec codec) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (decoder_) {
    LOG(WARNING) << "Decoder is set more than once";
  }

  switch (codec) {
    case protocol::ChannelConfig::CODEC_VP8: {
      DVLOG(2) << "Test Video Renderer will use VP8 decoder";
      decoder_ = VideoDecoderVpx::CreateForVP8();
      break;
    }
    case protocol::ChannelConfig::CODEC_VP9: {
      DVLOG(2) << "Test Video Renderer will use VP9 decoder";
      decoder_ = VideoDecoderVpx::CreateForVP9();
      break;
    }
    case protocol::ChannelConfig::CODEC_VERBATIM: {
      DVLOG(2) << "Test Video Renderer will use VERBATIM decoder";
      decoder_.reset(new VideoDecoderVerbatim());
      break;
    }
    default: {
      NOTREACHED() << "Unsupported codec: " << codec;
    }
  }
}

scoped_ptr<webrtc::DesktopFrame>
    TestVideoRenderer::Core::GetBufferForTest() const {
  base::AutoLock auto_lock(lock_);
  DCHECK(buffer_);
  return make_scoped_ptr(webrtc::BasicDesktopFrame::CopyOf(*buffer_.get()));
}

void TestVideoRenderer::Core::ProcessVideoPacket(
    scoped_ptr<VideoPacket> packet, const base::Closure& done) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(decoder_);
  DCHECK(packet);

  DVLOG(2) << "TestVideoRenderer::Core::ProcessVideoPacket() Called";

  // Screen size is attached on the first packet as well as when the
  // host screen is resized.
  if (packet->format().has_screen_width() &&
      packet->format().has_screen_height()) {
    webrtc::DesktopSize source_size(packet->format().screen_width(),
                                    packet->format().screen_height());
    if (!screen_size_.equals(source_size)) {
      screen_size_ = source_size;
      decoder_->Initialize(screen_size_);
      buffer_.reset(new webrtc::BasicDesktopFrame(screen_size_));
    }
  }

  // To make life easier, assume that the desktop shape is a single rectangle.
  packet->clear_use_desktop_shape();
  if (!decoder_->DecodePacket(*packet.get())) {
    LOG(ERROR) << "Decoder::DecodePacket() failed.";
    return;
  }

  {
    base::AutoLock auto_lock(lock_);

    // Render the decoded packet and write results to the buffer.
    // Note that the |updated_region_| maintains the changed regions compared to
    // previous video frame.
    decoder_->RenderFrame(screen_size_,
                          webrtc::DesktopRect::MakeWH(screen_size_.width(),
                          screen_size_.height()), buffer_->data(),
                          buffer_->stride(), &updated_region_);
  }

  main_task_runner_->PostTask(FROM_HERE, done);
}

TestVideoRenderer::TestVideoRenderer()
    : video_decode_thread_(
        new base::Thread("TestVideoRendererVideoDecodingThread")),
      weak_factory_(this) {
  DCHECK(thread_checker_.CalledOnValidThread());

  core_.reset(new Core());
  if (!video_decode_thread_->Start()) {
    LOG(ERROR) << "Cannot start TestVideoRenderer";
  } else {
    video_decode_task_runner_ = video_decode_thread_->task_runner();
    video_decode_task_runner_->PostTask(FROM_HERE, base::Bind(&Core::Initialize,
                                        base::Unretained(core_.get())));
  }
}

TestVideoRenderer::~TestVideoRenderer() {
  DCHECK(thread_checker_.CalledOnValidThread());

  video_decode_task_runner_->DeleteSoon(FROM_HERE, core_.release());

  // The thread's message loop will run until it runs out of work.
  video_decode_thread_->Stop();
}

void TestVideoRenderer::OnSessionConfig(const protocol::SessionConfig& config) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DVLOG(2) << "TestVideoRenderer::OnSessionConfig() Called";
  protocol::ChannelConfig::Codec codec = config.video_config().codec;
  SetCodecForDecoding(codec);
}

ChromotingStats* TestVideoRenderer::GetStats() {
  DCHECK(thread_checker_.CalledOnValidThread());

  DVLOG(2) << "TestVideoRenderer::GetStats() Called";
  return nullptr;
}

protocol::VideoStub* TestVideoRenderer::GetVideoStub() {
  DCHECK(thread_checker_.CalledOnValidThread());

  DVLOG(2) << "TestVideoRenderer::GetVideoStub() Called";
  return this;
}

void TestVideoRenderer::ProcessVideoPacket(scoped_ptr<VideoPacket> video_packet,
                                           const base::Closure& done) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(video_decode_task_runner_) << "Failed to start video decode thread";

  if (video_packet->has_data() && video_packet->data().size() != 0) {
    DVLOG(2) << "process video packet is called!";

    // Post video process task to the video decode thread.
    base::Closure process_video_task = base::Bind(
        &TestVideoRenderer::Core::ProcessVideoPacket,
        base::Unretained(core_.get()), base::Passed(&video_packet), done);
    video_decode_task_runner_->PostTask(FROM_HERE, process_video_task);
  } else {
    // Log at a high verbosity level as we receive empty packets frequently and
    // they can clutter up the debug output if the level is set too low.
    DVLOG(3) << "Empty Video Packet received.";
    done.Run();
  }
}

void TestVideoRenderer::SetCodecForDecoding(
    const protocol::ChannelConfig::Codec codec) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DVLOG(2) << "TestVideoRenderer::SetDecoder() Called";
  video_decode_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Core::SetCodecForDecoding,
                            base::Unretained(core_.get()),
                            codec));
}

scoped_ptr<webrtc::DesktopFrame> TestVideoRenderer::GetBufferForTest() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  return core_->GetBufferForTest();
}

}  // namespace test
}  // namespace remoting
