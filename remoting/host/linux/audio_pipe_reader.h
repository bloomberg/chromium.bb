// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_AUDIO_PIPE_READER_H_
#define REMOTING_HOST_LINUX_AUDIO_PIPE_READER_H_

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/observer_list_threadsafe.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/proto/audio.pb.h"

namespace remoting {

struct AudioPipeReaderTraits;

// AudioPipeReader class reads from a named pipe to which an audio server (e.g.
// pulseaudio) writes the sound that's being played back and then sends data to
// all registered observers.
class AudioPipeReader
    : public base::RefCountedThreadSafe<AudioPipeReader, AudioPipeReaderTraits>,
      public base::MessageLoopForIO::Watcher {
 public:
  // PulseAudio's module-pipe-sink must be configured to use the following
  // parameters for the sink we read from.
  static const AudioPacket_SamplingRate kSamplingRate =
      AudioPacket::SAMPLING_RATE_48000;
  static const AudioPacket_BytesPerSample kBytesPerSample =
      AudioPacket::BYTES_PER_SAMPLE_2;
  static const AudioPacket_Channels kChannels = AudioPacket::CHANNELS_STEREO;

  class StreamObserver {
   public:
    virtual void OnDataRead(scoped_refptr<base::RefCountedString> data) = 0;
  };

  // |task_runner| specifies the IO thread to use to read data from the pipe.
  static scoped_refptr<AudioPipeReader> Create(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const base::FilePath& pipe_path);

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

  AudioPipeReader(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                  const base::FilePath& pipe_path);
  virtual ~AudioPipeReader();

  void StartOnAudioThread();
  void OnDirectoryChanged(const base::FilePath& path, bool error);
  void TryOpenPipe();
  void StartTimer();
  void DoCapture();
  void WaitForPipeReadable();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::FilePath pipe_path_;

  // Watcher for the directory that contains audio pipe we are reading from, to
  // monitor when pulseaudio creates or deletes it.
  base::FilePathWatcher file_watcher_;

  base::File pipe_;
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

  base::MessageLoopForIO::FileDescriptorWatcher file_descriptor_watcher_;

  DISALLOW_COPY_AND_ASSIGN(AudioPipeReader);
};

// Destroys |audio_pipe_reader| on the audio thread.
struct AudioPipeReaderTraits {
  static void Destruct(const AudioPipeReader* audio_pipe_reader);
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_AUDIO_PIPE_READER_H_
