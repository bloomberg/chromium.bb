// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_AUDIO_CAPTURER_LINUX_H_
#define REMOTING_HOST_AUDIO_CAPTURER_LINUX_H_

#include "remoting/host/audio_capturer.h"

#include "base/message_loop.h"
#include "base/time.h"
#include "base/timer.h"

class FilePath;

namespace remoting {

class AudioCapturerLinux : public AudioCapturer,
                           public MessageLoopForIO::Watcher {
 public:
  explicit AudioCapturerLinux(const FilePath& pipe_name);
  virtual ~AudioCapturerLinux();

  // AudioCapturer interface.
  virtual bool Start(const PacketCapturedCallback& callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual bool IsStarted() OVERRIDE;

  // MessageLoopForIO::Watcher interface.
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;

 private:
  void StartTimer();
  void DoCapture();
  void WaitForPipeReadable();

  int pipe_fd_;
  base::RepeatingTimer<AudioCapturerLinux> timer_;
  PacketCapturedCallback callback_;

  // Time when capturing was started.
  base::TimeTicks started_time_;

  // Stream position of the last capture.
  int64 last_capture_samples_;

  // Bytes left from the previous read.
  std::string left_over_bytes_;

  MessageLoopForIO::FileDescriptorWatcher file_descriptor_watcher_;

  DISALLOW_COPY_AND_ASSIGN(AudioCapturerLinux);
};

}  // namespace remoting

#endif  // REMOTING_HOST_AUDIO_CAPTURER_LINUX_H_
