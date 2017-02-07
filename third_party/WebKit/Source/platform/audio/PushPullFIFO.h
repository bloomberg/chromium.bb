// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushPullFIFO_h
#define PushPullFIFO_h

#include "platform/audio/AudioBus.h"
#include "public/platform/WebCommon.h"
#include "wtf/Allocator.h"

namespace blink {

// A configuration data container for PushPullFIFO unit test.
struct PushPullFIFOStateForTest {
  const size_t fifoLength;
  const unsigned numberOfChannels;
  const size_t framesAvailable;
  const size_t indexRead;
  const size_t indexWrite;
  const unsigned overflowCount;
  const unsigned underflowCount;
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
  explicit PushPullFIFO(unsigned numberOfChannels, size_t fifoLength);
  ~PushPullFIFO();

  // Pushes the rendered frames by WebAudio engine.
  //  - The |inputBus| length is 128 frames (1 render quantum), fixed.
  //  - In case of overflow (FIFO full while push), the existing frames in FIFO
  //    will be overwritten and |indexRead| will be forcibly moved to
  //    |indexWrite| to avoid reading overwritten frames.
  void push(const AudioBus* inputBus);

  // Pulling |framesRequested| by the audio device thread.
  //  - If |framesRequested| is bigger than the length of |outputBus|, it
  //    violates SECURITY_CHECK().
  //  - If |framesRequested| is bigger than FIFO length, it violates
  //    SECURITY_CHECK().
  //  - In case of underflow (FIFO empty while pull), the remaining space in the
  //    requested output bus will be filled with silence. Thus it will fulfill
  //    the request from the consumer without causing error, but with a glitch.
  void pull(AudioBus* outputBus, size_t framesRequested);

  size_t framesAvailable() const { return m_framesAvailable; }
  size_t length() const { return m_fifoLength; }
  unsigned numberOfChannels() const { return m_fifoBus->numberOfChannels(); }
  AudioBus* bus() const { return m_fifoBus.get(); }

  // For unit test. Get the current configuration that consists of FIFO length,
  // number of channels, read/write index position and under/overflow count.
  const PushPullFIFOStateForTest getStateForTest() const;

 private:
  // The size of the FIFO.
  const size_t m_fifoLength = 0;

  RefPtr<AudioBus> m_fifoBus;

  // The number of frames in the FIFO actually available for pulling.
  size_t m_framesAvailable;

  size_t m_indexRead;
  size_t m_indexWrite;

  unsigned m_overflowCount;
  unsigned m_underflowCount;
};

}  // namespace blink

#endif  // PushPullFIFO_h
