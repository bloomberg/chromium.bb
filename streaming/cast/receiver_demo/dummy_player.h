// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STREAMING_CAST_RECEIVER_DEMO_DUMMY_PLAYER_H_
#define STREAMING_CAST_RECEIVER_DEMO_DUMMY_PLAYER_H_

#include <stdint.h>

#include <vector>

#include "platform/api/task_runner.h"
#include "platform/api/time.h"
#include "streaming/cast/receiver.h"

namespace openscreen {
namespace cast_streaming {

// Consumes frames from a Receiver, but does nothing other than OSP_LOG_INFO
// each one's FrameId, timestamp and size. This is only useful for confirming a
// Receiver is successfully receiving a stream, for platforms where
// SDLVideoPlayer cannot be built.
class DummyPlayer : public Receiver::Consumer {
 public:
  explicit DummyPlayer(Receiver* receiver);

  ~DummyPlayer() final;

 private:
  // Receiver::Consumer implementation.
  void OnFramesReady(int next_frame_buffer_size) final;

  Receiver* const receiver_;
  std::vector<uint8_t> buffer_;
};

}  // namespace cast_streaming
}  // namespace openscreen

#endif  // STREAMING_CAST_RECEIVER_DEMO_DUMMY_PLAYER_H_
