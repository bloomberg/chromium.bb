// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_AUDIO_CAPTURER_LINUX_H_
#define REMOTING_HOST_AUDIO_CAPTURER_LINUX_H_

#include "base/memory/ref_counted.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/audio_silence_detector.h"
#include "remoting/host/linux/audio_pipe_reader.h"

class FilePath;

namespace remoting {

// Linux implementation of AudioCapturer interface which captures audio by
// reading samples from a Pulseaudio "pipe" sink.
class AudioCapturerLinux : public AudioCapturer,
                           public AudioPipeReader::StreamObserver {
 public:
  // Must be called to configure the capturer before the first capturer instance
  // is created. |task_runner| is an IO thread that is passed to AudioPipeReader
  // to read from the pipe.
  static void InitializePipeReader(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const FilePath& pipe_name);

  explicit AudioCapturerLinux(
      scoped_refptr<AudioPipeReader> pipe_reader);
  virtual ~AudioCapturerLinux();

  // AudioCapturer interface.
  virtual bool Start(const PacketCapturedCallback& callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual bool IsStarted() OVERRIDE;

  // AudioPipeReader::StreamObserver interface.
  virtual void OnDataRead(scoped_refptr<base::RefCountedString> data) OVERRIDE;

 private:
  scoped_refptr<AudioPipeReader> pipe_reader_;
  PacketCapturedCallback callback_;

  AudioSilenceDetector silence_detector_;

  DISALLOW_COPY_AND_ASSIGN(AudioCapturerLinux);
};

}  // namespace remoting

#endif  // REMOTING_HOST_AUDIO_CAPTURER_LINUX_H_
