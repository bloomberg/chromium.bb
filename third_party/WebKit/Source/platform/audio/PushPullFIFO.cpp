// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/audio/PushPullFIFO.h"

#include <memory>
#include "platform/audio/AudioUtilities.h"
#include "wtf/PtrUtil.h"

namespace blink {

namespace {

// Suppress the warning log if over/underflow happens more than 100 times.
const unsigned kMaxMessagesToLog = 100;
}

const size_t PushPullFIFO::kMaxFIFOLength = 65536;

PushPullFIFO::PushPullFIFO(unsigned numberOfChannels, size_t fifoLength)
    : m_fifoLength(fifoLength),
      m_framesAvailable(0),
      m_indexRead(0),
      m_indexWrite(0),
      m_overflowCount(0),
      m_underflowCount(0) {
  CHECK_LE(m_fifoLength, kMaxFIFOLength);
  m_fifoBus = AudioBus::create(numberOfChannels, m_fifoLength);
}

PushPullFIFO::~PushPullFIFO() {}

// Push the data from |inputBus| to FIFO. The size of push is determined by
// the length of |inputBus|.
void PushPullFIFO::push(const AudioBus* inputBus) {
  CHECK(inputBus);
  CHECK_EQ(inputBus->length(), AudioUtilities::kRenderQuantumFrames);
  SECURITY_CHECK(inputBus->length() <= m_fifoLength);
  SECURITY_CHECK(m_indexWrite < m_fifoLength);

  const size_t inputBusLength = inputBus->length();
  const size_t remainder = m_fifoLength - m_indexWrite;

  for (unsigned i = 0; i < m_fifoBus->numberOfChannels(); ++i) {
    float* fifoBusChannel = m_fifoBus->channel(i)->mutableData();
    const float* inputBusChannel = inputBus->channel(i)->data();
    if (remainder >= inputBusLength) {
      // The remainder is big enough for the input data.
      memcpy(fifoBusChannel + m_indexWrite, inputBusChannel,
             inputBusLength * sizeof(*fifoBusChannel));
    } else {
      // The input data overflows the remainder size. Wrap around the index.
      memcpy(fifoBusChannel + m_indexWrite, inputBusChannel,
             remainder * sizeof(*fifoBusChannel));
      memcpy(fifoBusChannel, inputBusChannel + remainder,
             (inputBusLength - remainder) * sizeof(*fifoBusChannel));
    }
  }

  // Update the write index; wrap it around if necessary.
  m_indexWrite = (m_indexWrite + inputBusLength) % m_fifoLength;

  // In case of overflow, move the |indexRead| to the updated |indexWrite| to
  // avoid reading overwritten frames by the next pull.
  if (inputBusLength > m_fifoLength - m_framesAvailable) {
    m_indexRead = m_indexWrite;
    if (++m_overflowCount < kMaxMessagesToLog) {
      LOG(WARNING) << "PushPullFIFO: overflow while pushing ("
                   << "overflowCount=" << m_overflowCount
                   << ", availableFrames=" << m_framesAvailable
                   << ", inputFrames=" << inputBusLength
                   << ", fifoLength=" << m_fifoLength << ")";
    }
  }

  // Update the number of frames available in FIFO.
  m_framesAvailable =
      std::min(m_framesAvailable + inputBusLength, m_fifoLength);
  DCHECK_EQ((m_indexRead + m_framesAvailable) % m_fifoLength, m_indexWrite);
}

// Pull the data out of FIFO to |outputBus|. If remaining frame in the FIFO
// is less than the frames to pull, provides remaining frame plus the silence.
void PushPullFIFO::pull(AudioBus* outputBus, size_t framesRequested) {
#if OS(ANDROID)
  if (!outputBus) {
    // Log when outputBus is invalid. (crbug.com/692423)
    LOG(WARNING) << "[WebAudio/PushPullFIFO::pull] |outputBus| is invalid.";
    // Silently return to avoid crash.
    return;
  }
#endif
  CHECK(outputBus);
  SECURITY_CHECK(framesRequested <= outputBus->length());
  SECURITY_CHECK(framesRequested <= m_fifoLength);
  SECURITY_CHECK(m_indexRead < m_fifoLength);

  const size_t remainder = m_fifoLength - m_indexRead;
  const size_t framesToFill = std::min(m_framesAvailable, framesRequested);

  for (unsigned i = 0; i < m_fifoBus->numberOfChannels(); ++i) {
    const float* fifoBusChannel = m_fifoBus->channel(i)->data();
    float* outputBusChannel = outputBus->channel(i)->mutableData();

    // Fill up the output bus with the available frames first.
    if (remainder >= framesToFill) {
      // The remainder is big enough for the frames to pull.
      memcpy(outputBusChannel, fifoBusChannel + m_indexRead,
             framesToFill * sizeof(*fifoBusChannel));
    } else {
      // The frames to pull is bigger than the remainder size.
      // Wrap around the index.
      memcpy(outputBusChannel, fifoBusChannel + m_indexRead,
             remainder * sizeof(*fifoBusChannel));
      memcpy(outputBusChannel + remainder, fifoBusChannel,
             (framesToFill - remainder) * sizeof(*fifoBusChannel));
    }

    // The frames available was not enough to fulfill the requested frames. Fill
    // the rest of the channel with silence.
    if (framesRequested > framesToFill) {
      memset(outputBusChannel + framesToFill, 0,
             (framesRequested - framesToFill) * sizeof(*outputBusChannel));
    }
  }

  // Update the read index; wrap it around if necessary.
  m_indexRead = (m_indexRead + framesToFill) % m_fifoLength;

  // In case of underflow, move the |indexWrite| to the updated |indexRead|.
  if (framesRequested > framesToFill) {
    m_indexWrite = m_indexRead;
    if (m_underflowCount++ < kMaxMessagesToLog) {
      LOG(WARNING) << "PushPullFIFO: underflow while pulling ("
                   << "underflowCount=" << m_underflowCount
                   << ", availableFrames=" << m_framesAvailable
                   << ", requestedFrames=" << framesRequested
                   << ", fifoLength=" << m_fifoLength << ")";
    }
  }

  // Update the number of frames in FIFO.
  m_framesAvailable -= framesToFill;
  DCHECK_EQ((m_indexRead + m_framesAvailable) % m_fifoLength, m_indexWrite);
}

const PushPullFIFOStateForTest PushPullFIFO::getStateForTest() const {
  return {length(),     numberOfChannels(), framesAvailable(), m_indexRead,
          m_indexWrite, m_overflowCount,    m_underflowCount};
}

}  // namespace blink
