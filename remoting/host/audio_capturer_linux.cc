// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/audio_capturer_linux.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/command_line.h"
#include "base/eintr_wrapper.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "remoting/proto/audio.pb.h"

namespace remoting {

namespace {

const char kAudioPipeOptionName[] = "audio-pipe-name";

// PulseAudio's module-pipe-sink must be configured to use the following
// parameters for the sink we read from.
const AudioPacket_SamplingRate kSamplingRate = AudioPacket::SAMPLING_RATE_44100;
const int kChannels = 2;
const int kBytesPerSample = 2;

// Read data from the pipe every 40ms.
const int kCapturingPeriodMs = 40;

#if !defined(F_SETPIPE_SZ)
// F_SETPIPE_SZ is supported only starting linux 2.6.35, but we want to be able
// to compile this code on machines with older kernel.
#define F_SETPIPE_SZ 1031
#endif  // defined(F_SETPIPE_SZ)

const int IsPacketOfSilence(const std::string& data) {
  const int64* int_buf = reinterpret_cast<const int64*>(data.data());
  for (size_t i = 0; i < data.size() / sizeof(int64); i++) {
    if (int_buf[i] != 0)
      return false;
  }
  for (size_t i = data.size() - data.size() % sizeof(int64);
       i < data.size(); i++) {
    if (data.data()[i] != 0)
      return false;
  }
  return true;
}

}  // namespace

AudioCapturerLinux::AudioCapturerLinux(const FilePath& pipe_name) {
  pipe_fd_ = HANDLE_EINTR(open(
      pipe_name.value().c_str(), O_RDONLY | O_NONBLOCK));
  if (pipe_fd_ < 0) {
    LOG(ERROR) << "Failed to open " << pipe_name.value();
    return;
  }

  // Set buffer size for the pipe to the double of what's required for samples
  // of each capturing period.
  int pipe_buffer_size = 2 * kCapturingPeriodMs * kSamplingRate * kChannels *
     kBytesPerSample / base::Time::kMillisecondsPerSecond;
  int result = HANDLE_EINTR(fcntl(pipe_fd_, F_SETPIPE_SZ, pipe_buffer_size));
  if (result < 0) {
    PLOG(ERROR) << "fcntl";
  }

  WaitForPipeReadable();
}

AudioCapturerLinux::~AudioCapturerLinux() {
}

bool AudioCapturerLinux::Start(const PacketCapturedCallback& callback) {
  if (pipe_fd_ < 0)
    return false;

  callback_ = callback;

  return true;
}

void AudioCapturerLinux::Stop() {
  callback_.Reset();
}

bool AudioCapturerLinux::IsStarted() {
  return !callback_.is_null();
}

void AudioCapturerLinux::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(fd, pipe_fd_);
  StartTimer();
}

void AudioCapturerLinux::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

void AudioCapturerLinux::StartTimer() {
  started_time_ = base::TimeTicks::Now();
  last_capture_samples_ = 0;
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kCapturingPeriodMs),
               this, &AudioCapturerLinux::DoCapture);
}

void AudioCapturerLinux::DoCapture() {
  DCHECK_GT(pipe_fd_, 0);

  // Calculate how much we need read from the pipe. Pulseaudio doesn't control
  // how much data it writes to the pipe, so we need to pace the stream, so
  // that we read the exact number of the samples per second we need.
  base::TimeDelta stream_position = base::TimeTicks::Now() - started_time_;
  int64 stream_position_samples = stream_position.InMilliseconds() *
      kSamplingRate / base::Time::kMillisecondsPerSecond;
  int64 samples_to_capture =
      stream_position_samples - last_capture_samples_;
  last_capture_samples_ = stream_position_samples;
  int64 read_size =
      samples_to_capture * kChannels * kBytesPerSample;

  std::string data = left_over_bytes_;
  int pos = data.size();
  left_over_bytes_.clear();
  data.resize(read_size);

  while (pos < read_size) {
    int read_result = HANDLE_EINTR(
       read(pipe_fd_, string_as_array(&data) + pos, read_size - pos));
    if (read_result >= 0) {
      pos += read_result;
    } else {
      if (errno != EWOULDBLOCK && errno != EAGAIN)
        PLOG(ERROR) << "read";
      break;
    }
  }

  if (pos == 0) {
    WaitForPipeReadable();
    return;
  }

  // Save any incomplete samples we've read for later. Each packet should
  // contain integer number of samples.
  int incomplete_samples_bytes = pos % (kChannels * kBytesPerSample);
  left_over_bytes_.assign(data, pos - incomplete_samples_bytes,
                          incomplete_samples_bytes);
  data.resize(pos - incomplete_samples_bytes);

  if (callback_.is_null())
    return;

  if (IsPacketOfSilence(data))
    return;

  scoped_ptr<AudioPacket> packet(new AudioPacket());
  packet->add_data(data);
  packet->set_encoding(AudioPacket::ENCODING_RAW);
  packet->set_sampling_rate(kSamplingRate);
  packet->set_bytes_per_sample(AudioPacket::BYTES_PER_SAMPLE_2);
  packet->set_channels(AudioPacket::CHANNELS_STEREO);
  callback_.Run(packet.Pass());
}

void AudioCapturerLinux::WaitForPipeReadable() {
  timer_.Stop();
  MessageLoopForIO::current()->WatchFileDescriptor(
      pipe_fd_, false, MessageLoopForIO::WATCH_READ,
      &file_descriptor_watcher_, this);
}

bool AudioCapturer::IsSupported() {
  CommandLine* cl = CommandLine::ForCurrentProcess();
  return !cl->GetSwitchValuePath(kAudioPipeOptionName).empty();
}

scoped_ptr<AudioCapturer> AudioCapturer::Create() {
  CommandLine* cl = CommandLine::ForCurrentProcess();
  FilePath path = cl->GetSwitchValuePath(kAudioPipeOptionName);
  if (path.empty())
    return scoped_ptr<AudioCapturer>();
  return scoped_ptr<AudioCapturer>(new AudioCapturerLinux(path));
}

}  // namespace remoting
