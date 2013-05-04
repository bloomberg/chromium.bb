// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/media/audio_decoder.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>

#include "base/callback.h"
#include "base/file_descriptor_posix.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/shared_memory.h"
#include "media/base/android/webaudio_media_codec_info.h"
#include "media/base/audio_bus.h"
#include "media/base/limits.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebAudioBus.h"

namespace webkit_media {

class AudioDecoderIO {
 public:
  AudioDecoderIO(const char* data, size_t data_size);
  ~AudioDecoderIO();
  bool ShareEncodedToProcess(base::SharedMemoryHandle* handle);

  // Returns true if AudioDecoderIO was successfully created.
  bool IsValid() const;

  int read_fd() const { return read_fd_; }
  int write_fd() const { return write_fd_; }

 private:
  // Shared memory that will hold the encoded audio data.  This is
  // used by MediaCodec for decoding.
  base::SharedMemory encoded_shared_memory_;

  // A pipe used to communicate with MediaCodec.  MediaCodec owns
  // write_fd_ and writes to it.
  int read_fd_;
  int write_fd_;

  DISALLOW_COPY_AND_ASSIGN(AudioDecoderIO);
};

AudioDecoderIO::AudioDecoderIO(const char* data, size_t data_size)
    : read_fd_(-1),
      write_fd_(-1) {

  if (!data || !data_size || data_size > 0x80000000)
    return;

  // Create the shared memory and copy our data to it so that
  // MediaCodec can access it.
  encoded_shared_memory_.CreateAndMapAnonymous(data_size);

  if (!encoded_shared_memory_.memory())
    return;

  memcpy(encoded_shared_memory_.memory(), data, data_size);

  // Create a pipe for reading/writing the decoded PCM data
  int pipefd[2];

  if (pipe(pipefd))
    return;

  read_fd_ = pipefd[0];
  write_fd_ = pipefd[1];
}

AudioDecoderIO::~AudioDecoderIO() {
  // Close the read end of the pipe.  The write end should have been
  // closed by MediaCodec.
  if (read_fd_ >= 0 && close(read_fd_)) {
    DVLOG(1) << "Cannot close read fd " << read_fd_
             << ": " << strerror(errno);
  }
}

bool AudioDecoderIO::IsValid() const {
  return read_fd_ >= 0 && write_fd_ >= 0 &&
      encoded_shared_memory_.memory();
}

bool AudioDecoderIO::ShareEncodedToProcess(base::SharedMemoryHandle* handle) {
  return encoded_shared_memory_.ShareToProcess(
      base::Process::Current().handle(),
      handle);
}

static float ConvertSampleToFloat(int16_t sample) {
  const float kMaxScale = 1.0f / std::numeric_limits<int16_t>::max();
  const float kMinScale = -1.0f / std::numeric_limits<int16_t>::min();

  return sample * (sample < 0 ? kMinScale : kMaxScale);
}

// The number of frames is known so preallocate the destination
// bus and copy the pcm data to the destination bus as it's being
// received.
static void CopyPcmDataToBus(int input_fd,
                             WebKit::WebAudioBus* destination_bus,
                             size_t number_of_frames,
                             unsigned number_of_channels,
                             double file_sample_rate) {
  destination_bus->initialize(number_of_channels,
                              number_of_frames,
                              file_sample_rate);

  int16_t pipe_data[PIPE_BUF / sizeof(int16_t)];
  size_t decoded_frames = 0;
  ssize_t nread;

  while ((nread = HANDLE_EINTR(read(input_fd, pipe_data, sizeof(pipe_data)))) >
         0) {
    size_t samples_in_pipe = nread / sizeof(int16_t);
    for (size_t m = 0; m < samples_in_pipe; m += number_of_channels) {
      if (decoded_frames >= number_of_frames)
        break;

      for (size_t k = 0; k < number_of_channels; ++k) {
        int16_t sample = pipe_data[m + k];
        destination_bus->channelData(k)[decoded_frames] =
            ConvertSampleToFloat(sample);
      }
      ++decoded_frames;
    }
  }
}

// The number of frames is unknown, so keep reading and buffering
// until there's no more data and then copy the data to the
// destination bus.
static void BufferAndCopyPcmDataToBus(int input_fd,
                                      WebKit::WebAudioBus* destination_bus,
                                      unsigned number_of_channels,
                                      double file_sample_rate) {
  int16_t pipe_data[PIPE_BUF / sizeof(int16_t)];
  std::vector<int16_t> decoded_samples;
  ssize_t nread;

  while ((nread = HANDLE_EINTR(read(input_fd, pipe_data, sizeof(pipe_data)))) >
         0) {
    size_t samples_in_pipe = nread / sizeof(int16_t);
    if (decoded_samples.size() + samples_in_pipe > decoded_samples.capacity()) {
      decoded_samples.reserve(std::max(samples_in_pipe,
                                       2 * decoded_samples.capacity()));
    }
    std::copy(pipe_data,
              pipe_data + samples_in_pipe,
              back_inserter(decoded_samples));
  }

  DVLOG(1) << "Total samples read = " << decoded_samples.size();

  // Convert the samples and save them in the audio bus.
  size_t number_of_samples = decoded_samples.size();
  size_t number_of_frames = decoded_samples.size() / number_of_channels;
  size_t decoded_frames = 0;

  destination_bus->initialize(number_of_channels,
                              number_of_frames,
                              file_sample_rate);

  for (size_t m = 0; m < number_of_samples; m += number_of_channels) {
    for (size_t k = 0; k < number_of_channels; ++k) {
      int16_t sample = decoded_samples[m + k];
      destination_bus->channelData(k)[decoded_frames] =
          ConvertSampleToFloat(sample);
    }
    ++decoded_frames;
  }
}

// To decode audio data, we want to use the Android MediaCodec class.
// But this can't run in a sandboxed process so we need initiate the
// request to MediaCodec in the browser.  To do this, we create a
// shared memory buffer that holds the audio data.  We send a message
// to the browser to start the decoder using this buffer and one end
// of a pipe.  The MediaCodec class will decode the data from the
// shared memory and write the PCM samples back to us over a pipe.
bool DecodeAudioFileData(WebKit::WebAudioBus* destination_bus, const char* data,
                         size_t data_size, double sample_rate,
                         const WebAudioMediaCodecRunner& runner) {
  AudioDecoderIO audio_decoder(data, data_size);

  if (!audio_decoder.IsValid())
    return false;

  base::SharedMemoryHandle encoded_data_handle;
  audio_decoder.ShareEncodedToProcess(&encoded_data_handle);
  base::FileDescriptor fd(audio_decoder.write_fd(), true);

  DVLOG(1) << "DecodeAudioFileData: Starting MediaCodec";

  // Start MediaCodec processing in the browser which will read from
  // encoded_data_handle for our shared memory and write the decoded
  // PCM samples (16-bit integer) to our pipe.

  runner.Run(encoded_data_handle, fd, data_size);

  // First, read the number of channels, the sample rate, and the
  // number of frames and a flag indicating if the file is an
  // ogg/vorbis file.  This must be coordinated with
  // WebAudioMediaCodecBridge!
  //
  // TODO(rtoy): If we know the number of samples, we can create the
  // destination bus directly and do the conversion directly to the
  // bus instead of buffering up everything before saving the data to
  // the bus.

  int input_fd = audio_decoder.read_fd();
  struct media::WebAudioMediaCodecInfo info;

  DVLOG(1) << "Reading audio file info from fd " << input_fd;
  ssize_t nread = HANDLE_EINTR(read(input_fd, &info, sizeof(info)));
  DVLOG(1) << "read:  " << nread << " bytes:\n"
           << " 0: number of channels = " << info.channel_count << "\n"
           << " 1: sample rate        = " << info.sample_rate << "\n"
           << " 2: number of frames   = " << info.number_of_frames << "\n";

  if (nread != sizeof(info))
    return false;

  unsigned number_of_channels = info.channel_count;
  double file_sample_rate = static_cast<double>(info.sample_rate);
  size_t number_of_frames = info.number_of_frames;

  // Sanity checks
  if (!number_of_channels ||
      number_of_channels > media::limits::kMaxChannels ||
      file_sample_rate < media::limits::kMinSampleRate ||
      file_sample_rate > media::limits::kMaxSampleRate) {
    return false;
  }

  if (number_of_frames > 0) {
    CopyPcmDataToBus(input_fd,
                     destination_bus,
                     number_of_frames,
                     number_of_channels,
                     file_sample_rate);
  } else {
    BufferAndCopyPcmDataToBus(input_fd,
                              destination_bus,
                              number_of_channels,
                              file_sample_rate);
  }

  return true;
}

}  // namespace webkit_media
