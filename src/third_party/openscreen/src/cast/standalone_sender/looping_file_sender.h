// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STANDALONE_SENDER_LOOPING_FILE_SENDER_H_
#define CAST_STANDALONE_SENDER_LOOPING_FILE_SENDER_H_

#include <algorithm>
#include <string>

#include "cast/standalone_sender/constants.h"
#include "cast/standalone_sender/simulated_capturer.h"
#include "cast/standalone_sender/streaming_opus_encoder.h"
#include "cast/standalone_sender/streaming_vp8_encoder.h"
#include "cast/streaming/sender_session.h"

namespace openscreen {
namespace cast {

// Plays the media file at a given path over and over again, transcoding and
// streaming its audio/video.
class LoopingFileSender final : public SimulatedAudioCapturer::Client,
                                public SimulatedVideoCapturer::Client {
 public:
  LoopingFileSender(TaskRunner* task_runner,
                    const char* path,
                    const IPEndpoint& remote_endpoint,
                    SenderSession::ConfiguredSenders senders,
                    int max_bitrate);

  ~LoopingFileSender() final;

 private:
  void UpdateEncoderBitrates();
  void ControlForNetworkCongestion();
  void SendFileAgain();

  // SimulatedAudioCapturer overrides.
  void OnAudioData(const float* interleaved_samples,
                   int num_samples,
                   Clock::time_point capture_time) final;

  // SimulatedVideoCapturer overrides;
  void OnVideoFrame(const AVFrame& av_frame,
                    Clock::time_point capture_time) final;

  void UpdateStatusOnConsole();

  // SimulatedCapturer overrides.
  void OnEndOfFile(SimulatedCapturer* capturer) final;
  void OnError(SimulatedCapturer* capturer, std::string message) final;

  const char* ToTrackName(SimulatedCapturer* capturer) const;

  // Holds the required injected dependencies (clock, task runner) used for Cast
  // Streaming, and owns the UDP socket over which all communications occur with
  // the remote's Receivers.
  Environment env_;

  // The path to the media file to stream over and over.
  const char* const path_;

  // The packet router allows both the Audio Sender and the Video Sender to
  // share the same UDP socket.
  SenderPacketRouter packet_router_;

  const int max_bitrate_;  // Passed by the user on the command line.
  int bandwidth_estimate_ = 0;
  int bandwidth_being_utilized_;

  StreamingOpusEncoder audio_encoder_;
  StreamingVp8Encoder video_encoder_;

  int num_capturers_running_ = 0;
  Clock::time_point capture_start_time_{};
  Clock::time_point latest_frame_time_{};
  absl::optional<SimulatedAudioCapturer> audio_capturer_;
  absl::optional<SimulatedVideoCapturer> video_capturer_;

  Alarm next_task_;
  Alarm console_update_task_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STANDALONE_SENDER_LOOPING_FILE_SENDER_H_
