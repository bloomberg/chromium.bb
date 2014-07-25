// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_VIDEO_SCHEDULER_H_
#define REMOTING_HOST_VIDEO_SCHEDULER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/codec/video_encoder.h"
#include "remoting/host/capture_scheduler.h"
#include "remoting/proto/video.pb.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {
class ScreenCapturer;
}  // namespace media

namespace remoting {

class CursorShapeInfo;

namespace protocol {
class CursorShapeInfo;
class CursorShapeStub;
class VideoStub;
}  // namespace protocol

// Class responsible for scheduling frame captures from a
// webrtc::ScreenCapturer, delivering them to a VideoEncoder to encode, and
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
// VideoScheduler would ideally schedule captures so as to saturate the slowest
// of the capture, encode and network processes.  However, it also needs to
// rate-limit captures to avoid overloading the host system, either by consuming
// too much CPU, or hogging the host's graphics subsystem.

class VideoScheduler : public base::RefCountedThreadSafe<VideoScheduler>,
                       public webrtc::DesktopCapturer::Callback,
                       public webrtc::ScreenCapturer::MouseShapeObserver {
 public:
  // Enables timestamps for generated frames. Used for testing.
  static void EnableTimestampsForTests();

  // Creates a VideoScheduler running capture, encode and network tasks on the
  // supplied TaskRunners.  Video and cursor shape updates will be pumped to
  // |video_stub| and |client_stub|, which must remain valid until Stop() is
  // called. |capturer| is used to capture frames.
  VideoScheduler(
      scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
      scoped_ptr<webrtc::ScreenCapturer> capturer,
      scoped_ptr<VideoEncoder> encoder,
      protocol::CursorShapeStub* cursor_stub,
      protocol::VideoStub* video_stub);

  // webrtc::DesktopCapturer::Callback implementation.
  virtual webrtc::SharedMemory* CreateSharedMemory(size_t size) OVERRIDE;
  virtual void OnCaptureCompleted(webrtc::DesktopFrame* frame) OVERRIDE;

  // webrtc::ScreenCapturer::MouseShapeObserver implementation.
  virtual void OnCursorShapeChanged(
      webrtc::MouseCursorShape* cursor_shape) OVERRIDE;

  // Starts scheduling frame captures.
  void Start();

  // Stop scheduling frame captures. This object cannot be re-used once
  // it has been stopped.
  void Stop();

  // Pauses or resumes scheduling of frame captures.  Pausing/resuming captures
  // only affects capture scheduling and does not stop/start the capturer.
  void Pause(bool pause);

  // Updates the sequence number embedded in VideoPackets.
  // Sequence numbers are used for performance measurements.
  void UpdateSequenceNumber(int64 sequence_number);

  // Sets whether the video encoder should be requested to encode losslessly,
  // or to use a lossless color space (typically requiring higher bandwidth).
  void SetLosslessEncode(bool want_lossless);
  void SetLosslessColor(bool want_lossless);

 private:
  friend class base::RefCountedThreadSafe<VideoScheduler>;
  virtual ~VideoScheduler();

  // Capturer thread ----------------------------------------------------------

  // Starts the capturer on the capture thread.
  void StartOnCaptureThread();

  // Stops scheduling frame captures on the capture thread.
  void StopOnCaptureThread();

  // Schedules the next call to CaptureNextFrame.
  void ScheduleNextCapture();

  // Starts the next frame capture, unless there are already too many pending.
  void CaptureNextFrame();

  // Called when a frame capture has been encoded & sent to the client.
  void FrameCaptureCompleted();

  // Network thread -----------------------------------------------------------

  // Send |packet| to the client, unless we are in the process of stopping.
  void SendVideoPacket(scoped_ptr<VideoPacket> packet);

  // Callback passed to |video_stub_| for the last packet in each frame, to
  // rate-limit frame captures to network throughput.
  void OnVideoPacketSent();

  // Called by |keep_alive_timer_|.
  void SendKeepAlivePacket();

  // Callback for |video_stub_| called after a keep-alive packet is sent.
  void OnKeepAlivePacketSent();

  // Send updated cursor shape to client.
  void SendCursorShape(scoped_ptr<protocol::CursorShapeInfo> cursor_shape);

  // Encoder thread -----------------------------------------------------------

  // Encode a frame, passing generated VideoPackets to SendVideoPacket().
  void EncodeFrame(scoped_ptr<webrtc::DesktopFrame> frame,
                   int64 sequence_number,
                   base::TimeTicks timestamp);

  void EncodedDataAvailableCallback(int64 sequence_number,
                                    scoped_ptr<VideoPacket> packet);

  // Task runners used by this class.
  scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> encode_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  // Used to capture frames. Always accessed on the capture thread.
  scoped_ptr<webrtc::ScreenCapturer> capturer_;

  // Used to encode captured frames. Always accessed on the encode thread.
  scoped_ptr<VideoEncoder> encoder_;

  // Interfaces through which video frames and cursor shapes are passed to the
  // client. These members are always accessed on the network thread.
  protocol::CursorShapeStub* cursor_stub_;
  protocol::VideoStub* video_stub_;

  // Timer used to schedule CaptureNextFrame().
  scoped_ptr<base::OneShotTimer<VideoScheduler> > capture_timer_;

  // Timer used to ensure that we send empty keep-alive frames to the client
  // even when the video stream is paused or encoder is busy.
  scoped_ptr<base::DelayTimer<VideoScheduler> > keep_alive_timer_;

  // The number of frames being processed, i.e. frames that we are currently
  // capturing, encoding or sending. The value is capped at 2 to minimize
  // latency.
  int pending_frames_;

  // Set when the capturer is capturing a frame.
  bool capture_pending_;

  // True if the previous scheduled capture was skipped.
  bool did_skip_frame_;

  // True if capture of video frames is paused.
  bool is_paused_;

  // Number updated by the caller to trace performance.
  int64 sequence_number_;

  // An object to schedule capturing.
  CaptureScheduler scheduler_;

  DISALLOW_COPY_AND_ASSIGN(VideoScheduler);
};

}  // namespace remoting

#endif  // REMOTING_HOST_VIDEO_SCHEDULER_H_
