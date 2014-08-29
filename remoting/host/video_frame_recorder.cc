// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/video_frame_recorder.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/proto/video.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

static int64_t FrameContentSize(const webrtc::DesktopFrame* frame) {
  DCHECK_GT(frame->stride(), 0);
  return frame->stride() * frame->size().height();
}

// VideoEncoder wrapper used to intercept frames passed to a real VideoEncoder.
class VideoFrameRecorder::RecordingVideoEncoder : public VideoEncoder {
 public:
  RecordingVideoEncoder(scoped_ptr<VideoEncoder> encoder,
                        scoped_refptr<base::TaskRunner> recorder_task_runner,
                        base::WeakPtr<VideoFrameRecorder> recorder);

  base::WeakPtr<RecordingVideoEncoder> AsWeakPtr();

  void set_enable_recording(bool enable_recording) {
    DCHECK(!encoder_task_runner_.get() ||
           encoder_task_runner_->BelongsToCurrentThread());
    enable_recording_ = enable_recording;
  }

  // remoting::VideoEncoder interface.
  virtual void SetLosslessEncode(bool want_lossless) OVERRIDE;
  virtual void SetLosslessColor(bool want_lossless) OVERRIDE;
  virtual scoped_ptr<VideoPacket> Encode(
      const webrtc::DesktopFrame& frame) OVERRIDE;

 private:
  scoped_ptr<VideoEncoder> encoder_;
  scoped_refptr<base::TaskRunner> recorder_task_runner_;
  base::WeakPtr<VideoFrameRecorder> recorder_;

  bool enable_recording_;
  scoped_refptr<base::SingleThreadTaskRunner> encoder_task_runner_;

  base::WeakPtrFactory<RecordingVideoEncoder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RecordingVideoEncoder);
};

VideoFrameRecorder::RecordingVideoEncoder::RecordingVideoEncoder(
    scoped_ptr<VideoEncoder> encoder,
    scoped_refptr<base::TaskRunner> recorder_task_runner,
    base::WeakPtr<VideoFrameRecorder> recorder)
    : encoder_(encoder.Pass()),
      recorder_task_runner_(recorder_task_runner),
      recorder_(recorder),
      enable_recording_(false),
      weak_factory_(this) {
  DCHECK(encoder_);
  DCHECK(recorder_task_runner_.get());
}

base::WeakPtr<VideoFrameRecorder::RecordingVideoEncoder>
VideoFrameRecorder::RecordingVideoEncoder::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void VideoFrameRecorder::RecordingVideoEncoder::SetLosslessEncode(
    bool want_lossless) {
  encoder_->SetLosslessEncode(want_lossless);
}

void VideoFrameRecorder::RecordingVideoEncoder::SetLosslessColor(
    bool want_lossless) {
  encoder_->SetLosslessColor(want_lossless);
}

scoped_ptr<VideoPacket> VideoFrameRecorder::RecordingVideoEncoder::Encode(
    const webrtc::DesktopFrame& frame) {
  // If this is the first Encode() then store the TaskRunner and inform the
  // VideoFrameRecorder so it can post set_enable_recording() on it.
  if (!encoder_task_runner_.get()) {
    encoder_task_runner_ = base::ThreadTaskRunnerHandle::Get();
    recorder_task_runner_->PostTask(FROM_HERE,
        base::Bind(&VideoFrameRecorder::SetEncoderTaskRunner,
                   recorder_,
                   encoder_task_runner_));
  }

  DCHECK(encoder_task_runner_->BelongsToCurrentThread());

  if (enable_recording_) {
    // Copy the frame and post it to the VideoFrameRecorder to store.
    scoped_ptr<webrtc::DesktopFrame> frame_copy(
        new webrtc::BasicDesktopFrame(frame.size()));
    *frame_copy->mutable_updated_region() = frame.updated_region();
    frame_copy->set_dpi(frame.dpi());
    frame_copy->CopyPixelsFrom(frame.data(),
                               frame.stride(),
                               webrtc::DesktopRect::MakeSize(frame.size()));
    recorder_task_runner_->PostTask(FROM_HERE,
        base::Bind(&VideoFrameRecorder::RecordFrame,
                   recorder_,
                   base::Passed(&frame_copy)));
  }

  return encoder_->Encode(frame);
}

VideoFrameRecorder::VideoFrameRecorder()
    : content_bytes_(0),
      max_content_bytes_(0),
      enable_recording_(false),
      weak_factory_(this) {
}

VideoFrameRecorder::~VideoFrameRecorder() {
  DetachVideoEncoderWrapper();
}

scoped_ptr<VideoEncoder> VideoFrameRecorder::WrapVideoEncoder(
    scoped_ptr<VideoEncoder> encoder) {
  DCHECK(!encoder_task_runner_.get());
  DCHECK(!caller_task_runner_.get());
  caller_task_runner_ = base::ThreadTaskRunnerHandle::Get();

  scoped_ptr<RecordingVideoEncoder> recording_encoder(
      new RecordingVideoEncoder(encoder.Pass(),
                                caller_task_runner_,
                                weak_factory_.GetWeakPtr()));
  recording_encoder_ = recording_encoder->AsWeakPtr();

  return recording_encoder.PassAs<VideoEncoder>();
}

void VideoFrameRecorder::DetachVideoEncoderWrapper() {
  DCHECK(!caller_task_runner_.get() ||
         caller_task_runner_->BelongsToCurrentThread());

  // Immediately detach the wrapper from this recorder.
  weak_factory_.InvalidateWeakPtrs();

  // Clean up any pending recorded frames.
  STLDeleteElements(&recorded_frames_);
  content_bytes_ = 0;

  // Tell the wrapper to stop recording and posting frames to us.
  if (encoder_task_runner_.get()) {
    encoder_task_runner_->PostTask(FROM_HERE,
        base::Bind(&RecordingVideoEncoder::set_enable_recording,
                   recording_encoder_, false));
  }

  // Detach this recorder from the calling and encode threads.
  caller_task_runner_ = NULL;
  encoder_task_runner_ = NULL;
}

void VideoFrameRecorder::SetEnableRecording(bool enable_recording) {
  DCHECK(!caller_task_runner_.get() ||
         caller_task_runner_->BelongsToCurrentThread());

  if (enable_recording_ == enable_recording) {
    return;
  }
  enable_recording_ = enable_recording;

  if (encoder_task_runner_.get()) {
    encoder_task_runner_->PostTask(FROM_HERE,
        base::Bind(&RecordingVideoEncoder::set_enable_recording,
                   recording_encoder_,
                   enable_recording_));
  }
}

void VideoFrameRecorder::SetMaxContentBytes(int64_t max_content_bytes) {
  DCHECK(!caller_task_runner_.get() ||
         caller_task_runner_->BelongsToCurrentThread());
  DCHECK_GE(max_content_bytes, 0);

  max_content_bytes_ = max_content_bytes;
}

scoped_ptr<webrtc::DesktopFrame> VideoFrameRecorder::NextFrame() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  scoped_ptr<webrtc::DesktopFrame> frame;
  if (!recorded_frames_.empty()) {
    frame.reset(recorded_frames_.front());
    recorded_frames_.pop_front();
    content_bytes_ -= FrameContentSize(frame.get());
    DCHECK_GE(content_bytes_, 0);
  }

  return frame.Pass();
}

void VideoFrameRecorder::SetEncoderTaskRunner(
    scoped_refptr<base::TaskRunner> task_runner) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!encoder_task_runner_.get());
  DCHECK(task_runner.get());

  encoder_task_runner_ = task_runner;

  // If the caller already enabled recording, inform the recording encoder.
  if (enable_recording_ && encoder_task_runner_.get()) {
    encoder_task_runner_->PostTask(FROM_HERE,
        base::Bind(&RecordingVideoEncoder::set_enable_recording,
                   recording_encoder_,
                   enable_recording_));
  }
}

void VideoFrameRecorder::RecordFrame(scoped_ptr<webrtc::DesktopFrame> frame) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  int64_t frame_bytes = FrameContentSize(frame.get());
  DCHECK_GE(frame_bytes, 0);

  // Purge existing frames until there is space for the new one.
  while (content_bytes_ + frame_bytes > max_content_bytes_ &&
         !recorded_frames_.empty()) {
    scoped_ptr<webrtc::DesktopFrame> drop_frame(recorded_frames_.front());
    recorded_frames_.pop_front();
    content_bytes_ -= FrameContentSize(drop_frame.get());
    DCHECK_GE(content_bytes_, 0);
  }

  // If the frame is still too big, ignore it.
  if (content_bytes_ + frame_bytes > max_content_bytes_) {
    return;
  }

  // Store the frame and update the content byte count.
  recorded_frames_.push_back(frame.release());
  content_bytes_ += frame_bytes;
}

}  // namespace remoting
