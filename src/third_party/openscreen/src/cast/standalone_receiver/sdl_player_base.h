// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STANDALONE_RECEIVER_SDL_PLAYER_BASE_H_
#define CAST_STANDALONE_RECEIVER_SDL_PLAYER_BASE_H_

#include <stdint.h>

#include <functional>
#include <map>
#include <string>

#include "cast/standalone_receiver/decoder.h"
#include "cast/standalone_receiver/sdl_glue.h"
#include "cast/streaming/receiver.h"
#include "platform/api/task_runner.h"
#include "platform/api/time.h"
#include "platform/base/error.h"

namespace cast {
namespace streaming {

// Common base class that consumes frames from a Receiver, decodes them, and
// plays them out via the appropriate SDL subsystem. Subclasses implement the
// specifics, based on the type of media (audio or video).
class SDLPlayerBase : public Receiver::Consumer, public Decoder::Client {
 public:
  ~SDLPlayerBase() override;

  // Returns OK unless a fatal error has occurred.
  const openscreen::Error& error_status() const { return error_status_; }

 protected:
  // Current player state, which is used to determine what to render/present,
  // and how frequently.
  enum PlayerState {
    kWaitingForFirstFrame,  // Render silent "blue splash" screen at idle FPS.
    kScheduledToPresent,    // Present new content at an exact time point.
    kPresented,             // Continue presenting same content at idle FPS.
    kError,                 // Render silent "red splash" screen at idle FPS.
  };

  // A decoded frame and its target presentation time.
  struct PresentableFrame {
    openscreen::platform::Clock::time_point presentation_time;
    AVFrameUniquePtr decoded_frame;

    PresentableFrame();
    ~PresentableFrame();
    PresentableFrame(PresentableFrame&& other) noexcept;
    PresentableFrame& operator=(PresentableFrame&& other) noexcept;
  };

  // |error_callback| is run only if a fatal error occurs, at which point the
  // player has halted and set |error_status()|. |media_type| should be "audio"
  // or "video" (only used when logging).
  SDLPlayerBase(openscreen::platform::ClockNowFunctionPtr now_function,
                openscreen::platform::TaskRunner* task_runner,
                Receiver* receiver,
                std::function<void()> error_callback,
                const char* media_type);

  PlayerState state() const { return state_; }

  // Called back from either |decoder_| or a player subclass to handle a fatal
  // error event.
  void OnFatalError(std::string message) final;

  // Renders the |frame| and returns its [possibly adjusted] presentation time.
  virtual openscreen::ErrorOr<openscreen::platform::Clock::time_point>
  RenderNextFrame(const PresentableFrame& frame) = 0;

  // Called to render when the player has no new content, and returns true if a
  // Present() is necessary. |frame| may be null, if it is not available. This
  // method can be called before the first frame, after any frame, or after a
  // fatal error has occurred.
  virtual bool RenderWhileIdle(const PresentableFrame* frame) = 0;

  // Presents the rendering from the last call to RenderNextFrame() or
  // RenderWhileIdle().
  virtual void Present() = 0;

 private:
  // Receiver::Consumer implementation.
  void OnFramesReady(int next_frame_buffer_size) final;

  // AVCodecDecoder::Client implementation. These are called-back from
  // |decoder_| to provide results.
  void OnFrameDecoded(FrameId frame_id, const AVFrame& frame) final;
  void OnDecodeError(FrameId frame_id, std::string message) final;

  // Calls RenderNextFrame() on the next available decoded frame, and schedules
  // its presentation. If no decoded frame is available, RenderWhileIdle() is
  // called instead.
  void RenderAndSchedulePresentation();

  // Schedules an explicit check to see if more frames are ready for
  // consumption. Normally, the Receiver will notify this Consumer when more
  // frames are ready. However, there are cases where prior notifications were
  // ignored because there were too many frames in the player's pipeline. Thus,
  // whenever frames are removed from the pipeline, this method should be
  // called.
  void ResumeDecoding();

  // Called whenever a frame has been decoded, presentation of a prior frame has
  // completed, and/or the player has encountered a state change that might
  // require rendering/presenting a different output.
  void ResumeRendering();

  const openscreen::platform::ClockNowFunctionPtr now_;
  Receiver* const receiver_;
  std::function<void()> error_callback_;  // Run once by OnFatalError().
  const char* const media_type_;          // For logging only.

  // Set to the error code that placed the player in a fatal error state.
  openscreen::Error error_status_;

  // Current player state, which is used to determine what to render/present,
  // and how frequently.
  PlayerState state_ = kWaitingForFirstFrame;

  // Queue of frames currently being decoded and decoded frames awaiting
  // rendering.
  struct PendingFrame : public PresentableFrame {
    openscreen::platform::Clock::time_point start_time;

    PendingFrame();
    ~PendingFrame();
    PendingFrame(PendingFrame&& other) noexcept;
    PendingFrame& operator=(PendingFrame&& other) noexcept;
  };
  std::map<FrameId, PendingFrame> frames_to_render_;

  // Buffer for holding EncodedFrame::data.
  Decoder::Buffer buffer_;

  // Associates a RTP timestamp with a local clock time point. This is updated
  // whenever the media (RTP) timestamps drift too much away from the rate at
  // which the local clock ticks. This is important for A/V synchronization.
  RtpTimeTicks last_sync_rtp_timestamp_{};
  openscreen::platform::Clock::time_point last_sync_reference_time_{};

  Decoder decoder_;

  // The decoded frame to be rendered/presented.
  PendingFrame current_frame_;

  // A cumulative moving average of recent single-frame processing times
  // (consume + decode + render). This is passed to the Cast Receiver so that it
  // can determine when to drop late frames.
  openscreen::platform::Clock::duration recent_processing_time_{};

  // Alarms that execute the various stages of the player pipeline at certain
  // times.
  openscreen::Alarm decode_alarm_;
  openscreen::Alarm render_alarm_;
  openscreen::Alarm presentation_alarm_;

  // Maximum number of frames in the decode/render pipeline. This limit is about
  // making sure the player uses resources efficiently: It is better for frames
  // to remain in the Receiver's queue until this player is ready to process
  // them.
  static constexpr int kMaxFramesInPipeline = 8;
};

}  // namespace streaming
}  // namespace cast

#endif  // CAST_STANDALONE_RECEIVER_SDL_PLAYER_BASE_H_
