// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_AUDIO_PIPE_READER_H_
#define REMOTING_HOST_LINUX_AUDIO_PIPE_READER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop.h"
#include "base/observer_list_threadsafe.h"
#include "base/time.h"
#include "base/timer.h"

class FilePath;

namespace remoting {

// AudioPipeReader class reads from a named pipe to which an audio server (e.g.
// pulseaudio) writes the sound that's being played back and then sends data to
// all registered observers.
class AudioPipeReader
    : public base::RefCountedThreadSafe<AudioPipeReader>,
      public MessageLoopForIO::Watcher {
 public:
  class StreamObserver {
   public:
    virtual void OnDataRead(scoped_refptr<base::RefCountedString> data) = 0;
  };

  // |task_runner| defines the IO thread on which the object will be reading
  // data from the pipe.
  AudioPipeReader(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const FilePath& pipe_name);

  // Register or unregister an observer. Each observer receives data on the
  // thread on which it was registered and guaranteed not to be called after
  // RemoveObserver().
  void AddObserver(StreamObserver* observer);
  void RemoveObserver(StreamObserver* observer);

  // MessageLoopForIO::Watcher interface.
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<AudioPipeReader>;
  virtual ~AudioPipeReader();

  void StartOnAudioThread(const FilePath& pipe_name);
  void StartTimer();
  void DoCapture();
  void WaitForPipeReadable();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  int pipe_fd_;
  base::RepeatingTimer<AudioPipeReader> timer_;
  scoped_refptr<ObserverListThreadSafe<StreamObserver> > observers_;

  // Time when capturing was started.
  base::TimeTicks started_time_;

  // Stream position of the last capture.
  int64 last_capture_samples_;

  // Bytes left from the previous read.
  std::string left_over_bytes_;

  MessageLoopForIO::FileDescriptorWatcher file_descriptor_watcher_;

  DISALLOW_COPY_AND_ASSIGN(AudioPipeReader);
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_AUDIO_PIPE_READER_H_
