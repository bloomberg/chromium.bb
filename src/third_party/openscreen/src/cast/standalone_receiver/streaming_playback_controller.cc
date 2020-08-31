// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_receiver/streaming_playback_controller.h"

#if defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)
#include "cast/standalone_receiver/sdl_audio_player.h"
#include "cast/standalone_receiver/sdl_glue.h"
#include "cast/standalone_receiver/sdl_video_player.h"
#else
#include "cast/standalone_receiver/dummy_player.h"
#endif  // defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)

#include "util/trace_logging.h"

namespace openscreen {
namespace cast {

#if defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)
StreamingPlaybackController::StreamingPlaybackController(
    TaskRunner* task_runner,
    StreamingPlaybackController::Client* client)
    : task_runner_(task_runner),
      client_(client),
      sdl_event_loop_(task_runner_, [this] {
        client_->OnPlaybackError(this,
                                 Error{Error::Code::kOperationCancelled,
                                       std::string("SDL event loop closed.")});
      }) {
  OSP_DCHECK(task_runner_ != nullptr);
  OSP_DCHECK(client_ != nullptr);
  constexpr int kDefaultWindowWidth = 1280;
  constexpr int kDefaultWindowHeight = 720;
  window_ = MakeUniqueSDLWindow(
      "Cast Streaming Receiver Demo",
      SDL_WINDOWPOS_UNDEFINED /* initial X position */,
      SDL_WINDOWPOS_UNDEFINED /* initial Y position */, kDefaultWindowWidth,
      kDefaultWindowHeight, SDL_WINDOW_RESIZABLE);
  OSP_CHECK(window_) << "Failed to create SDL window: " << SDL_GetError();
  renderer_ = MakeUniqueSDLRenderer(window_.get(), -1, 0);
  OSP_CHECK(renderer_) << "Failed to create SDL renderer: " << SDL_GetError();
}
#else
StreamingPlaybackController::StreamingPlaybackController(
    TaskRunner* task_runner,
    StreamingPlaybackController::Client* client)
    : task_runner_(task_runner), client_(client) {
  OSP_DCHECK(task_runner_ != nullptr);
  OSP_DCHECK(client_ != nullptr);
}
#endif  // defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)

// TODO(jophba): add async tracing to streaming implementation for exposing
// how long the OFFER/ANSWER and receiver startup takes.
void StreamingPlaybackController::OnNegotiated(
    const ReceiverSession* session,
    ReceiverSession::ConfiguredReceivers receivers) {
  TRACE_DEFAULT_SCOPED(TraceCategory::kStandaloneReceiver);
#if defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)
  if (receivers.audio) {
    audio_player_ = std::make_unique<SDLAudioPlayer>(
        &Clock::now, task_runner_, receivers.audio->receiver,
        receivers.audio->selected_stream.stream.codec_name, [this] {
          client_->OnPlaybackError(this, audio_player_->error_status());
        });
  }
  if (receivers.video) {
    video_player_ = std::make_unique<SDLVideoPlayer>(
        &Clock::now, task_runner_, receivers.video->receiver,
        receivers.video->selected_stream.stream.codec_name, renderer_.get(),
        [this] {
          client_->OnPlaybackError(this, video_player_->error_status());
        });
  }
#else
  if (receivers.audio) {
    audio_player_ = std::make_unique<DummyPlayer>(receivers.audio->receiver);
  }

  if (receivers.video) {
    video_player_ = std::make_unique<DummyPlayer>(receivers.video->receiver);
  }
#endif  // defined(CAST_STANDALONE_RECEIVER_HAVE_EXTERNAL_LIBS)
}

void StreamingPlaybackController::OnConfiguredReceiversDestroyed(
    const ReceiverSession* session) {
  audio_player_.reset();
  video_player_.reset();
}

void StreamingPlaybackController::OnError(const ReceiverSession* session,
                                          Error error) {
  client_->OnPlaybackError(this, error);
}

}  // namespace cast
}  // namespace openscreen
