// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_SYNC_READER_H_
#define SERVICES_AUDIO_SYNC_READER_H_

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/process/process.h"
#include "base/sync_socket.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/audio_bus.h"
#include "services/audio/output_controller.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

namespace audio {

// An OutputController::SyncReader implementation using SyncSocket. This is used
// by OutputController to provide a low latency data source for transmitting
// audio packets between the Audio Service process and the process where the
// mojo client runs.
class SyncReader : public OutputController::SyncReader {
 public:
  // Constructor: Creates and maps shared memory; and initializes a SyncSocket
  // pipe, assigning the client handle to |foreign_socket|. The caller must
  // confirm IsValid() returns true before any other methods are called.
  SyncReader(base::RepeatingCallback<void(const std::string&)> log_callback,
             const media::AudioParameters& params,
             base::CancelableSyncSocket* foreign_socket);

  ~SyncReader() override;

  // Returns true if the SyncReader initialized successfully, and
  // TakeSharedMemoryRegion() will return a valid region.
  bool IsValid() const;

  // Transfers shared memory region ownership to a caller. It shouldn't be
  // called more than once.
  base::UnsafeSharedMemoryRegion TakeSharedMemoryRegion();

  void set_max_wait_timeout_for_test(base::TimeDelta time) {
    maximum_wait_time_ = time;
  }

  // OutputController::SyncReader implementation.
  void RequestMoreData(base::TimeDelta delay,
                       base::TimeTicks delay_timestamp,
                       int prior_frames_skipped) override;
  void Read(media::AudioBus* dest) override;
  void Close() override;

 private:
  // Blocks until data is ready for reading or a timeout expires.  Returns false
  // if an error or timeout occurs.
  bool WaitUntilDataIsReady();

  const base::RepeatingCallback<void(const std::string&)> log_callback_;

  base::UnsafeSharedMemoryRegion shared_memory_region_;
  base::WritableSharedMemoryMapping shared_memory_mapping_;

  // Mutes all incoming samples. This is used to prevent audible sound
  // during automated testing.
  const bool mute_audio_for_testing_;

  // Denotes that the most recent socket error has been logged. Used to avoid
  // log spam.
  bool had_socket_error_;

  // Socket for transmitting audio data.
  base::CancelableSyncSocket socket_;

  const uint32_t output_bus_buffer_size_;

  // Shared memory wrapper used for transferring audio data to Read() callers.
  std::unique_ptr<media::AudioBus> output_bus_;

  // Track the number of times the renderer missed its real-time deadline and
  // report a UMA stat during destruction.
  size_t renderer_callback_count_;
  size_t renderer_missed_callback_count_;
  size_t trailing_renderer_missed_callback_count_;

  // The maximum amount of time to wait for data from the renderer.  Calculated
  // from the parameters given at construction.
  base::TimeDelta maximum_wait_time_;

  // The index of the audio buffer we're expecting to be sent from the renderer;
  // used to block with timeout for audio data.
  uint32_t buffer_index_;

  DISALLOW_COPY_AND_ASSIGN(SyncReader);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_SYNC_READER_H_
