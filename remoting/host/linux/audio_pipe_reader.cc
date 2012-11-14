// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/audio_pipe_reader.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/stl_util.h"

namespace remoting {

namespace {

// PulseAudio's module-pipe-sink must be configured to use the following
// parameters for the sink we read from.
const int kSamplingRate = 48000;
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

AudioPipeReader::AudioPipeReader(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const FilePath& pipe_name)
    : task_runner_(task_runner),
      observers_(new ObserverListThreadSafe<StreamObserver>()) {
  task_runner_->PostTask(FROM_HERE, base::Bind(
      &AudioPipeReader::StartOnAudioThread, this, pipe_name));
}

void AudioPipeReader::StartOnAudioThread(const FilePath& pipe_name) {
  DCHECK(task_runner_->BelongsToCurrentThread());

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

AudioPipeReader::~AudioPipeReader() {
}

void AudioPipeReader::AddObserver(StreamObserver* observer) {
  observers_->AddObserver(observer);
}
void AudioPipeReader::RemoveObserver(StreamObserver* observer) {
  observers_->RemoveObserver(observer);
}

void AudioPipeReader::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(fd, pipe_fd_);
  StartTimer();
}

void AudioPipeReader::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

void AudioPipeReader::StartTimer() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  started_time_ = base::TimeTicks::Now();
  last_capture_samples_ = 0;
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kCapturingPeriodMs),
               this, &AudioPipeReader::DoCapture);
}

void AudioPipeReader::DoCapture() {
  DCHECK(task_runner_->BelongsToCurrentThread());
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

  if (IsPacketOfSilence(data))
    return;

  // Dispatch asynchronous notification to the stream observers.
  scoped_refptr<base::RefCountedString> data_ref =
      base::RefCountedString::TakeString(&data);
  observers_->Notify(&StreamObserver::OnDataRead, data_ref);
}

void AudioPipeReader::WaitForPipeReadable() {
  timer_.Stop();
  MessageLoopForIO::current()->WatchFileDescriptor(
      pipe_fd_, false, MessageLoopForIO::WATCH_READ,
      &file_descriptor_watcher_, this);
}

}  // namespace remoting
