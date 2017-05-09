// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/audio/PushPullFIFO.h"

#include <memory>
#include "platform/audio/AudioUtilities.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

namespace {

// Suppress the warning log if over/underflow happens more than 100 times.
const unsigned kMaxMessagesToLog = 100;
}

const size_t PushPullFIFO::kMaxFIFOLength = 65536;

PushPullFIFO::PushPullFIFO(unsigned number_of_channels, size_t fifo_length)
    : fifo_length_(fifo_length) {
  CHECK_LE(fifo_length_, kMaxFIFOLength);
  fifo_bus_ = AudioBus::Create(number_of_channels, fifo_length_);
}

PushPullFIFO::~PushPullFIFO() {}

// Push the data from |input_bus| to FIFO. The size of push is determined by
// the length of |input_bus|.
void PushPullFIFO::Push(const AudioBus* input_bus) {
  MutexLocker locker(lock_);

  CHECK(input_bus);
  CHECK_EQ(input_bus->length(), AudioUtilities::kRenderQuantumFrames);
  SECURITY_CHECK(input_bus->length() <= fifo_length_);
  SECURITY_CHECK(index_write_ < fifo_length_);

  const size_t input_bus_length = input_bus->length();
  const size_t remainder = fifo_length_ - index_write_;

  for (unsigned i = 0; i < fifo_bus_->NumberOfChannels(); ++i) {
    float* fifo_bus_channel = fifo_bus_->Channel(i)->MutableData();
    const float* input_bus_channel = input_bus->Channel(i)->Data();
    if (remainder >= input_bus_length) {
      // The remainder is big enough for the input data.
      memcpy(fifo_bus_channel + index_write_, input_bus_channel,
             input_bus_length * sizeof(*fifo_bus_channel));
    } else {
      // The input data overflows the remainder size. Wrap around the index.
      memcpy(fifo_bus_channel + index_write_, input_bus_channel,
             remainder * sizeof(*fifo_bus_channel));
      memcpy(fifo_bus_channel, input_bus_channel + remainder,
             (input_bus_length - remainder) * sizeof(*fifo_bus_channel));
    }
  }

  // Update the write index; wrap it around if necessary.
  index_write_ = (index_write_ + input_bus_length) % fifo_length_;

  // In case of overflow, move the |index_read_| to the updated |index_write_|
  // to avoid reading overwritten frames by the next pull.
  if (input_bus_length > fifo_length_ - frames_available_) {
    index_read_ = index_write_;
    if (++overflow_count_ < kMaxMessagesToLog) {
      LOG(WARNING) << "PushPullFIFO: overflow while pushing ("
                   << "overflowCount=" << overflow_count_
                   << ", availableFrames=" << frames_available_
                   << ", inputFrames=" << input_bus_length
                   << ", fifoLength=" << fifo_length_ << ")";
    }
  }

  // Update the number of frames available in FIFO.
  frames_available_ =
      std::min(frames_available_ + input_bus_length, fifo_length_);
  DCHECK_EQ((index_read_ + frames_available_) % fifo_length_, index_write_);
}

// Pull the data out of FIFO to |output_bus|. If remaining frame in the FIFO
// is less than the frames to pull, provides remaining frame plus the silence.
size_t PushPullFIFO::Pull(AudioBus* output_bus, size_t frames_requested) {
  MutexLocker locker(lock_);

#if OS(ANDROID)
  if (!output_bus) {
    // Log when outputBus or FIFO object is invalid. (crbug.com/692423)
    LOG(WARNING) << "[WebAudio/PushPullFIFO::pull <" << static_cast<void*>(this)
                 << ">] |outputBus| is invalid.";
    // Silently return to avoid crash.
    return 0;
  }

  // The following checks are in place to catch the inexplicable crash.
  // (crbug.com/692423)
  if (frames_requested > output_bus->length()) {
    LOG(WARNING) << "[WebAudio/PushPullFIFO::pull <" << static_cast<void*>(this)
                 << ">] framesRequested > outputBus->length() ("
                 << frames_requested << " > " << output_bus->length() << ")";
  }
  if (frames_requested > fifo_length_) {
    LOG(WARNING) << "[WebAudio/PushPullFIFO::pull <" << static_cast<void*>(this)
                 << ">] framesRequested > fifo_length_ (" << frames_requested
                 << " > " << fifo_length_ << ")";
  }
  if (index_read_ >= fifo_length_) {
    LOG(WARNING) << "[WebAudio/PushPullFIFO::pull <" << static_cast<void*>(this)
                 << ">] index_read_ >= fifo_length_ (" << index_read_
                 << " >= " << fifo_length_ << ")";
  }
#endif

  CHECK(output_bus);
  SECURITY_CHECK(frames_requested <= output_bus->length());
  SECURITY_CHECK(frames_requested <= fifo_length_);
  SECURITY_CHECK(index_read_ < fifo_length_);

  const size_t remainder = fifo_length_ - index_read_;
  const size_t frames_to_fill = std::min(frames_available_, frames_requested);

  for (unsigned i = 0; i < fifo_bus_->NumberOfChannels(); ++i) {
    const float* fifo_bus_channel = fifo_bus_->Channel(i)->Data();
    float* output_bus_channel = output_bus->Channel(i)->MutableData();

    // Fill up the output bus with the available frames first.
    if (remainder >= frames_to_fill) {
      // The remainder is big enough for the frames to pull.
      memcpy(output_bus_channel, fifo_bus_channel + index_read_,
             frames_to_fill * sizeof(*fifo_bus_channel));
    } else {
      // The frames to pull is bigger than the remainder size.
      // Wrap around the index.
      memcpy(output_bus_channel, fifo_bus_channel + index_read_,
             remainder * sizeof(*fifo_bus_channel));
      memcpy(output_bus_channel + remainder, fifo_bus_channel,
             (frames_to_fill - remainder) * sizeof(*fifo_bus_channel));
    }

    // The frames available was not enough to fulfill the requested frames. Fill
    // the rest of the channel with silence.
    if (frames_requested > frames_to_fill) {
      memset(output_bus_channel + frames_to_fill, 0,
             (frames_requested - frames_to_fill) * sizeof(*output_bus_channel));
    }
  }

  // Update the read index; wrap it around if necessary.
  index_read_ = (index_read_ + frames_to_fill) % fifo_length_;

  // In case of underflow, move the |indexWrite| to the updated |indexRead|.
  if (frames_requested > frames_to_fill) {
    index_write_ = index_read_;
    if (underflow_count_++ < kMaxMessagesToLog) {
      LOG(WARNING) << "PushPullFIFO: underflow while pulling ("
                   << "underflowCount=" << underflow_count_
                   << ", availableFrames=" << frames_available_
                   << ", requestedFrames=" << frames_requested
                   << ", fifoLength=" << fifo_length_ << ")";
    }
  }

  // Update the number of frames in FIFO.
  frames_available_ -= frames_to_fill;
  DCHECK_EQ((index_read_ + frames_available_) % fifo_length_, index_write_);

  // |frames_requested > frames_available_| means the frames in FIFO is not
  // enough to fulfill the requested frames from the audio device.
  return frames_requested > frames_available_
      ? frames_requested - frames_available_
      : 0;
}

const PushPullFIFOStateForTest PushPullFIFO::GetStateForTest() const {
  return {length(),     NumberOfChannels(), frames_available_, index_read_,
          index_write_, overflow_count_,    underflow_count_};
}

}  // namespace blink
