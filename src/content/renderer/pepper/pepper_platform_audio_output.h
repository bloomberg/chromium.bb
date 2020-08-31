// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_AUDIO_OUTPUT_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_AUDIO_OUTPUT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/audio/audio_output_ipc.h"

namespace media {
class AudioParameters;
}

namespace base {
class SingleThreadTaskRunner;
}

namespace content {
class AudioHelper;

class PepperPlatformAudioOutput
    : public media::AudioOutputIPCDelegate,
      public base::RefCountedThreadSafe<PepperPlatformAudioOutput> {
 public:
  // Factory function, returns NULL on failure. StreamCreated() will be called
  // when the stream is created.
  static PepperPlatformAudioOutput* Create(int sample_rate,
                                           int frames_per_buffer,
                                           int source_render_frame_id,
                                           AudioHelper* client);

  // The following three methods are all called on main thread.

  // Starts the playback. Returns false on error or if called before the
  // stream is created or after the stream is closed.
  bool StartPlayback();

  // Stops the playback. Returns false on error or if called before the stream
  // is created or after the stream is closed.
  bool StopPlayback();

  // Sets the volume. Returns false on error or if called before the stream
  // is created or after the stream is closed.
  bool SetVolume(double volume);

  // Closes the stream. Make sure to call this before the object is
  // destructed.
  void ShutDown();

  // media::AudioOutputIPCDelegate implementation.
  void OnError() override;
  void OnDeviceAuthorized(media::OutputDeviceStatus device_status,
                          const media::AudioParameters& output_params,
                          const std::string& matched_device_id) override;
  void OnStreamCreated(base::UnsafeSharedMemoryRegion shared_memory_region,
                       base::SyncSocket::ScopedHandle socket_handle,
                       bool playing_automatically) override;
  void OnIPCClosed() override;

 protected:
  ~PepperPlatformAudioOutput() override;

 private:
  friend class base::RefCountedThreadSafe<PepperPlatformAudioOutput>;

  PepperPlatformAudioOutput();

  bool Initialize(int sample_rate,
                  int frames_per_buffer,
                  int source_render_frame_id,
                  AudioHelper* client);

  // I/O thread backends to above functions.
  void InitializeOnIOThread(const media::AudioParameters& params);
  void StartPlaybackOnIOThread();
  void StopPlaybackOnIOThread();
  void SetVolumeOnIOThread(double volume);
  void ShutDownOnIOThread();

  // The client to notify when the stream is created. THIS MUST ONLY BE
  // ACCESSED ON THE MAIN THREAD.
  AudioHelper* client_;

  // Used to send/receive IPC. THIS MUST ONLY BE ACCESSED ON THE
  // I/O thread except to send messages and get the message loop.
  std::unique_ptr<media::AudioOutputIPC> ipc_;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(PepperPlatformAudioOutput);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_PLATFORM_AUDIO_OUTPUT_H_
