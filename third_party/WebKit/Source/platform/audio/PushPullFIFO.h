// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushPullFIFO_h
#define PushPullFIFO_h

#include "platform/audio/AudioBus.h"
#include "platform/wtf/Allocator.h"
#include "public/platform/WebCommon.h"

namespace blink {

// A configuration data container for PushPullFIFO unit test.
struct PushPullFIFOStateForTest {
  const size_t fifo_length;
  const unsigned number_of_channels;
  const size_t frames_available;
  const size_t index_read;
  const size_t index_write;
  const unsigned overflow_count;
  const unsigned underflow_count;
};

// PushPullFIFO class is an intermediate audio sample storage between
// Blink-WebAudio and the renderer. The renderer's hardware callback buffer size
// varies on the platform, but the WebAudio always renders 128 frames (render
// quantum, RQ) thus FIFO is needed to handle the general case.
class BLINK_PLATFORM_EXPORT PushPullFIFO {
  USING_FAST_MALLOC(PushPullFIFO);
  WTF_MAKE_NONCOPYABLE(PushPullFIFO);

 public:
  // Maximum FIFO length. (512 render quanta)
  static const size_t kMaxFIFOLength;

  // |fifoLength| cannot exceed |kMaxFIFOLength|. Otherwise it crashes.
  explicit PushPullFIFO(unsigned number_of_channels, size_t fifo_length);
  ~PushPullFIFO();

  // Pushes the rendered frames by WebAudio engine.
  //  - The |inputBus| length is 128 frames (1 render quantum), fixed.
  //  - In case of overflow (FIFO full while push), the existing frames in FIFO
  //    will be overwritten and |indexRead| will be forcibly moved to
  //    |indexWrite| to avoid reading overwritten frames.
  void Push(const AudioBus* input_bus);

  // Pulling |framesRequested| by the audio device thread.
  //  - If |framesRequested| is bigger than the length of |outputBus|, it
  //    violates SECURITY_CHECK().
  //  - If |framesRequested| is bigger than FIFO length, it violates
  //    SECURITY_CHECK().
  //  - In case of underflow (FIFO empty while pull), the remaining space in the
  //    requested output bus will be filled with silence. Thus it will fulfill
  //    the request from the consumer without causing error, but with a glitch.
  void Pull(AudioBus* output_bus, size_t frames_requested);

  size_t FramesAvailable() const { return frames_available_; }
  size_t length() const { return fifo_length_; }
  unsigned NumberOfChannels() const { return fifo_bus_->NumberOfChannels(); }
  AudioBus* Bus() const { return fifo_bus_.Get(); }

  // For unit test. Get the current configuration that consists of FIFO length,
  // number of channels, read/write index position and under/overflow count.
  const PushPullFIFOStateForTest GetStateForTest() const;

 private:
  // The size of the FIFO.
  const size_t fifo_length_ = 0;

  RefPtr<AudioBus> fifo_bus_;

  // The number of frames in the FIFO actually available for pulling.
  size_t frames_available_;

  size_t index_read_;
  size_t index_write_;

  unsigned overflow_count_;
  unsigned underflow_count_;
};

}  // namespace blink

#endif  // PushPullFIFO_h
