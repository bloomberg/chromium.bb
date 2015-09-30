// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_VIDEO_FRAME_PUMP_H_
#define REMOTING_HOST_VIDEO_FRAME_PUMP_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/host/capture_scheduler.h"
#include "remoting/proto/video.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

namespace protocol {
class VideoFeedbackStub;
class VideoStub;
}  // namespace protocol

// Class responsible for scheduling frame captures from a screen capturer.,
// delivering them to a VideoEncoder to encode, and
// finally passing the encoded video packets to the specified VideoStub to send
// on the network.
//
// THREADING
//
// This class is supplied TaskRunners to use for capture, encode and network
// operations.  Capture, encode and network transmission tasks are interleaved
// as illustrated below:
//
// |       CAPTURE       ENCODE     NETWORK
// |    .............
// |    .  Capture  .
// |    .............
// |                  ............
// |                  .          .
// |    ............. .          .
// |    .  Capture  . .  Encode  .
// |    ............. .          .
// |                  .          .
// |                  ............
// |    ............. ............ ..........
// |    .  Capture  . .          . .  Send  .
// |    ............. .          . ..........
// |                  .  Encode  .
// |                  .          .
// |                  .          .
// |                  ............
// | Time
// v
//
// VideoFramePump would ideally schedule captures so as to saturate the slowest
// of the capture, encode and network processes.  However, it also needs to
// rate-limit captures to avoid overloading the host system, either by consuming
// too much CPU, or hogging the host's graphics subsystem.
class VideoFramePump : public webrtc::DesktopCapturer::Callback {
 public:
  // Enables timestamps for generated frames. Used for testing.
  static void EnableTimestampsForTests();

  // Creates a VideoFramePump running capture, encode and network tasks on the
  // supplied TaskRunners. Video will be pumped to |video_stub|, which must
  // outlive the pump..
  VideoFramePump(
      scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner,
      scoped_ptr<webrtc::DesktopCapturer> capturer,
      scoped_ptr<VideoEncoder> encoder,
      protocol::VideoStub* video_stub);
  ~VideoFramePump() override;

  // Pauses or resumes scheduling of frame captures.  Pausing/resuming captures
  // only affects capture scheduling and does not stop/start the capturer.
  void Pause(bool pause);

  // Called whenever input event is received.
  void OnInputEventReceived(int64_t event_timestamp);

  // Sets whether the video encoder should be requested to encode losslessly,
  // or to use a lossless color space (typically requiring higher bandwidth).
  void SetLosslessEncode(bool want_lossless);
  void SetLosslessColor(bool want_lossless);

  protocol::VideoFeedbackStub* video_feedback_stub() {
    return &capture_scheduler_;
  }

 private:
  struct FrameTimestamps {
    FrameTimestamps();
    ~FrameTimestamps();

    // The following two fields are set only for one frame after each incoming
    // input event. |input_event_client_timestamp| is event timestamp
    // received from the client. |input_event_received_time| is local time when
    // the event was received.
    int64_t input_event_client_timestamp = -1;
    base::TimeTicks input_event_received_time;

    base::TimeTicks capture_started_time;
    base::TimeTicks capture_ended_time;
    base::TimeTicks encode_started_time;
    base::TimeTicks encode_ended_time;
    base::TimeTicks can_send_time;
  };

  struct PacketWithTimestamps {
    PacketWithTimestamps(scoped_ptr<VideoPacket> packet,
                         scoped_ptr<FrameTimestamps> timestamps);
    ~PacketWithTimestamps();

    scoped_ptr<VideoPacket> packet;
    scoped_ptr<FrameTimestamps> timestamps;
  };

  // webrtc::DesktopCapturer::Callback interface.
  webrtc::SharedMemory* CreateSharedMemory(size_t size) override;
  void OnCaptureCompleted(webrtc::DesktopFrame* frame) override;

  // Callback for CaptureScheduler.
  void CaptureNextFrame();

  // Task running on the encoder thread to encode the |frame|.
  static scoped_ptr<PacketWithTimestamps> EncodeFrame(
      VideoEncoder* encoder,
      scoped_ptr<webrtc::DesktopFrame> frame,
      scoped_ptr<FrameTimestamps> timestamps);

  // Task called when a frame has finished encoding.
  void OnFrameEncoded(scoped_ptr<PacketWithTimestamps> packet);

  // Sends |packet| to the client.
  void SendPacket(scoped_ptr<PacketWithTimestamps> packet);

  // Helper called from SendPacket() to calculate timing fields in the |packet|
  // before sending it.
  void UpdateFrameTimers(VideoPacket* packet, FrameTimestamps* timestamps);

  // Callback passed to |video_stub_|.
  void OnVideoPacketSent();

  // Called by |keep_alive_timer_|.
  void SendKeepAlivePacket();

  // Callback for |video_stub_| called after a keep-alive packet is sent.
  void OnKeepAlivePacketSent();

  // Task runner used to run |encoder_|.
  scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner_;

  // Capturer used to capture the screen.
  scoped_ptr<webrtc::DesktopCapturer> capturer_;

  // Used to encode captured frames. Always accessed on the encode thread.
  scoped_ptr<VideoEncoder> encoder_;

  // Interface through which video frames are passed to the client.
  protocol::VideoStub* video_stub_;

  // Timer used to ensure that we send empty keep-alive frames to the client
  // even when the video stream is paused or encoder is busy.
  base::Timer keep_alive_timer_;

  // CaptureScheduler calls CaptureNextFrame() whenever a new frame needs to be
  // captured.
  CaptureScheduler capture_scheduler_;

  // Timestamps for the frame to be captured next.
  scoped_ptr<FrameTimestamps> next_frame_timestamps_;

  // Timestamps for the frame that's being captured.
  scoped_ptr<FrameTimestamps> captured_frame_timestamps_;

  bool send_pending_ = false;

  ScopedVector<PacketWithTimestamps> pending_packets_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<VideoFramePump> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoFramePump);
};

}  // namespace remoting

#endif  // REMOTING_HOST_VIDEO_FRAME_PUMP_H_
