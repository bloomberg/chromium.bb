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
const int kSamplesPerSecond = 48000;
const int kChannels = 2;
const int kBytesPerSample = 2;
const int kSampleBytesPerSecond =
    kSamplesPerSecond * kChannels * kBytesPerSample;

// Read data from the pipe every 40ms.
const int kCapturingPeriodMs = 40;

// Size of the pipe buffer in milliseconds.
const int kPipeBufferSizeMs = kCapturingPeriodMs * 2;

// Size of the pipe buffer in bytes.
const int kPipeBufferSizeBytes = kPipeBufferSizeMs * kSampleBytesPerSecond /
    base::Time::kMillisecondsPerSecond;

#if !defined(F_SETPIPE_SZ)
// F_SETPIPE_SZ is supported only starting linux 2.6.35, but we want to be able
// to compile this code on machines with older kernel.
#define F_SETPIPE_SZ 1031
#endif  // defined(F_SETPIPE_SZ)

}  // namespace

// static
scoped_refptr<AudioPipeReader> AudioPipeReader::Create(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const FilePath& pipe_name) {
  // Create a reference to the new AudioPipeReader before posting the
  // StartOnAudioThread task, otherwise it may be deleted on the audio
  // thread before we return.
  scoped_refptr<AudioPipeReader> pipe_reader =
      new AudioPipeReader(task_runner);
  task_runner->PostTask(FROM_HERE, base::Bind(
      &AudioPipeReader::StartOnAudioThread, pipe_reader, pipe_name));
  return pipe_reader;
}

void AudioPipeReader::StartOnAudioThread(const FilePath& pipe_name) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  pipe_fd_ = HANDLE_EINTR(open(
      pipe_name.value().c_str(), O_RDONLY | O_NONBLOCK));
  if (pipe_fd_ < 0) {
    LOG(ERROR) << "Failed to open " << pipe_name.value();
    return;
  }

  // Set buffer size for the pipe.
  int result = HANDLE_EINTR(
      fcntl(pipe_fd_, F_SETPIPE_SZ, kPipeBufferSizeBytes));
  if (result < 0) {
    PLOG(ERROR) << "fcntl";
  }

  WaitForPipeReadable();
}

AudioPipeReader::AudioPipeReader(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner),
      observers_(new ObserverListThreadSafe<StreamObserver>()) {
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
  last_capture_position_ = 0;
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
  int64 stream_position_bytes = stream_position.InMilliseconds() *
      kSampleBytesPerSecond / base::Time::kMillisecondsPerSecond;
  int64 bytes_to_read = stream_position_bytes - last_capture_position_;

  std::string data = left_over_bytes_;
  size_t pos = data.size();
  left_over_bytes_.clear();
  data.resize(pos + bytes_to_read);

  while (pos < data.size()) {
    int read_result = HANDLE_EINTR(
       read(pipe_fd_, string_as_array(&data) + pos, data.size() - pos));
    if (read_result > 0) {
      pos += read_result;
    } else {
      if (read_result < 0 && errno != EWOULDBLOCK && errno != EAGAIN)
        PLOG(ERROR) << "read";
      break;
    }
  }

  // Stop reading from the pipe if PulseAudio isn't writing anything.
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

  last_capture_position_ += data.size();
  // Normally PulseAudio will keep pipe buffer full, so we should always be able
  // to read |bytes_to_read| bytes, but in case it's misbehaving we need to make
  // sure that |stream_position_bytes| doesn't go out of sync with the current
  // stream position.
  if (stream_position_bytes - last_capture_position_ > kPipeBufferSizeBytes)
    last_capture_position_ = stream_position_bytes - kPipeBufferSizeBytes;
  DCHECK_LE(last_capture_position_, stream_position_bytes);

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

// static
void AudioPipeReaderTraits::Destruct(const AudioPipeReader* audio_pipe_reader) {
  audio_pipe_reader->task_runner_->DeleteSoon(FROM_HERE, audio_pipe_reader);
}

}  // namespace remoting
