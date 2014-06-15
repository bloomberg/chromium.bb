// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/audio_pipe_reader.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/stl_util.h"

namespace remoting {

namespace {

const int kSampleBytesPerSecond = AudioPipeReader::kSamplingRate *
                                  AudioPipeReader::kChannels *
                                  AudioPipeReader::kBytesPerSample;

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
    const base::FilePath& pipe_path) {
  // Create a reference to the new AudioPipeReader before posting the
  // StartOnAudioThread task, otherwise it may be deleted on the audio
  // thread before we return.
  scoped_refptr<AudioPipeReader> pipe_reader =
      new AudioPipeReader(task_runner, pipe_path);
  task_runner->PostTask(
      FROM_HERE, base::Bind(&AudioPipeReader::StartOnAudioThread, pipe_reader));
  return pipe_reader;
}

AudioPipeReader::AudioPipeReader(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::FilePath& pipe_path)
    : task_runner_(task_runner),
      pipe_path_(pipe_path),
      observers_(new ObserverListThreadSafe<StreamObserver>()) {
}

AudioPipeReader::~AudioPipeReader() {}

void AudioPipeReader::AddObserver(StreamObserver* observer) {
  observers_->AddObserver(observer);
}
void AudioPipeReader::RemoveObserver(StreamObserver* observer) {
  observers_->RemoveObserver(observer);
}

void AudioPipeReader::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(fd, pipe_.GetPlatformFile());
  StartTimer();
}

void AudioPipeReader::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

void AudioPipeReader::StartOnAudioThread() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!file_watcher_.Watch(pipe_path_.DirName(), true,
                           base::Bind(&AudioPipeReader::OnDirectoryChanged,
                                      base::Unretained(this)))) {
    LOG(ERROR) << "Failed to watch pulseaudio directory "
               << pipe_path_.DirName().value();
  }

  TryOpenPipe();
}

void AudioPipeReader::OnDirectoryChanged(const base::FilePath& path,
                                         bool error) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (error) {
    LOG(ERROR) << "File watcher returned an error.";
    return;
  }

  TryOpenPipe();
}

void AudioPipeReader::TryOpenPipe() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::File new_pipe;
  new_pipe.Initialize(
      pipe_path_,
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_ASYNC);

  // If both |pipe_| and |new_pipe| are valid then compare inodes for the two
  // file descriptors. Don't need to do anything if inode hasn't changed.
  if (new_pipe.IsValid() && pipe_.IsValid()) {
    struct stat old_stat;
    struct stat new_stat;
    if (fstat(pipe_.GetPlatformFile(), &old_stat) == 0 &&
        fstat(new_pipe.GetPlatformFile(), &new_stat) == 0 &&
        old_stat.st_ino == new_stat.st_ino) {
      return;
    }
  }

  file_descriptor_watcher_.StopWatchingFileDescriptor();
  timer_.Stop();

  pipe_ = new_pipe.Pass();

  if (pipe_.IsValid()) {
    // Set O_NONBLOCK flag.
    if (HANDLE_EINTR(fcntl(pipe_.GetPlatformFile(), F_SETFL, O_NONBLOCK)) < 0) {
      PLOG(ERROR) << "fcntl";
      pipe_.Close();
      return;
    }

    // Set buffer size for the pipe.
    if (HANDLE_EINTR(fcntl(
            pipe_.GetPlatformFile(), F_SETPIPE_SZ, kPipeBufferSizeBytes)) < 0) {
      PLOG(ERROR) << "fcntl";
    }

    WaitForPipeReadable();
  }
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
  DCHECK(pipe_.IsValid());

  // Calculate how much we need read from the pipe. Pulseaudio doesn't control
  // how much data it writes to the pipe, so we need to pace the stream.
  base::TimeDelta stream_position = base::TimeTicks::Now() - started_time_;
  int64 stream_position_bytes = stream_position.InMilliseconds() *
      kSampleBytesPerSecond / base::Time::kMillisecondsPerSecond;
  int64 bytes_to_read = stream_position_bytes - last_capture_position_;

  std::string data = left_over_bytes_;
  size_t pos = data.size();
  left_over_bytes_.clear();
  data.resize(pos + bytes_to_read);

  while (pos < data.size()) {
    int read_result =
        pipe_.ReadAtCurrentPos(string_as_array(&data) + pos, data.size() - pos);
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
  base::MessageLoopForIO::current()->WatchFileDescriptor(
      pipe_.GetPlatformFile(), false, base::MessageLoopForIO::WATCH_READ,
      &file_descriptor_watcher_, this);
}

// static
void AudioPipeReaderTraits::Destruct(const AudioPipeReader* audio_pipe_reader) {
  audio_pipe_reader->task_runner_->DeleteSoon(FROM_HERE, audio_pipe_reader);
}

}  // namespace remoting
