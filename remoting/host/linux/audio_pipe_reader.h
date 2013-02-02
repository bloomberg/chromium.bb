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

namespace base {
class FilePath;
}

namespace remoting {

struct AudioPipeReaderTraits;

// AudioPipeReader class reads from a named pipe to which an audio server (e.g.
// pulseaudio) writes the sound that's being played back and then sends data to
// all registered observers.
class AudioPipeReader
  : public base::RefCountedThreadSafe<AudioPipeReader, AudioPipeReaderTraits>,
      public MessageLoopForIO::Watcher {
 public:
  class StreamObserver {
   public:
    virtual void OnDataRead(scoped_refptr<base::RefCountedString> data) = 0;
  };

  // |task_runner| specifies the IO thread to use to read data from the pipe.
  static scoped_refptr<AudioPipeReader> Create(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const base::FilePath& pipe_name);

  // Register or unregister an observer. Each observer receives data on the
  // thread on which it was registered and guaranteed not to be called after
  // RemoveObserver().
  void AddObserver(StreamObserver* observer);
  void RemoveObserver(StreamObserver* observer);

  // MessageLoopForIO::Watcher interface.
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;

 private:
  friend class base::DeleteHelper<AudioPipeReader>;
  friend class base::RefCountedThreadSafe<AudioPipeReader>;
  friend struct AudioPipeReaderTraits;

  AudioPipeReader(scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  virtual ~AudioPipeReader();

  void StartOnAudioThread(const base::FilePath& pipe_name);
  void StartTimer();
  void DoCapture();
  void WaitForPipeReadable();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  int pipe_fd_;
  base::RepeatingTimer<AudioPipeReader> timer_;
  scoped_refptr<ObserverListThreadSafe<StreamObserver> > observers_;

  // Time when capturing was started.
  base::TimeTicks started_time_;

  // Stream position of the last capture in bytes with zero position
  // corresponding to |started_time_|. Must always be a multiple of the sample
  // size.
  int64 last_capture_position_;

  // Bytes left from the previous read.
  std::string left_over_bytes_;

  MessageLoopForIO::FileDescriptorWatcher file_descriptor_watcher_;

  DISALLOW_COPY_AND_ASSIGN(AudioPipeReader);
};

// Destroys |audio_pipe_reader| on the audio thread.
struct AudioPipeReaderTraits {
  static void Destruct(const AudioPipeReader* audio_pipe_reader);
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_AUDIO_PIPE_READER_H_
