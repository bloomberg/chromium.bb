// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "streaming/cast/receiver_demo/sdl_glue.h"

#include "platform/api/task_runner.h"
#include "platform/api/time.h"
#include "util/logging.h"

using openscreen::platform::Clock;
using openscreen::platform::TaskRunner;

namespace openscreen {
namespace cast_streaming {

SDLEventLoopProcessor::SDLEventLoopProcessor(
    TaskRunner* task_runner,
    std::function<void()> quit_callback)
    : alarm_(&Clock::now, task_runner),
      quit_callback_(std::move(quit_callback)) {
  alarm_.Schedule([this] { ProcessPendingEvents(); }, {});
}

SDLEventLoopProcessor::~SDLEventLoopProcessor() = default;

void SDLEventLoopProcessor::ProcessPendingEvents() {
  // Process all pending events.
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      OSP_VLOG << "SDL_QUIT received, invoking quit callback...";
      if (quit_callback_) {
        quit_callback_();
      }
    }
  }

  // Schedule a task to come back and process more pending events.
  constexpr auto kEventPollPeriod = std::chrono::milliseconds(10);
  alarm_.Schedule([this] { ProcessPendingEvents(); },
                  Clock::now() + kEventPollPeriod);
}

}  // namespace cast_streaming
}  // namespace openscreen
