// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_INPUT_SYNC_WRITER_H_
#define SERVICES_AUDIO_INPUT_SYNC_WRITER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/sync_socket.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "services/audio/input_controller.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

namespace audio {

// A InputController::SyncWriter implementation using SyncSocket. This
// is used by InputController to provide a low latency data source for
// transmitting audio packets between the browser process and the renderer
// process.
class InputSyncWriter final : public InputController::SyncWriter {
 public:
  // Maximum fifo size (|overflow_buses_| and |overflow_params_|) in number of
  // media::AudioBuses.
  enum { kMaxOverflowBusesSize = 100 };

  // Create() automatically initializes the InputSyncWriter correctly,
  // and should be strongly preferred over calling the constructor directly!
  InputSyncWriter(
      base::RepeatingCallback<void(const std::string&)> log_callback,
      base::MappedReadOnlyRegion shared_memory,
      std::unique_ptr<base::CancelableSyncSocket> socket,
      uint32_t shared_memory_segment_count,
      const media::AudioParameters& params);

  ~InputSyncWriter() final;

  static std::unique_ptr<InputSyncWriter> Create(
      base::RepeatingCallback<void(const std::string&)> log_callback,
      uint32_t shared_memory_segment_count,
      const media::AudioParameters& params,
      base::CancelableSyncSocket* foreign_socket);

  // Transfers shared memory region ownership to a caller. It shouldn't be
  // called more than once.
  base::ReadOnlySharedMemoryRegion TakeSharedMemoryRegion();

  size_t shared_memory_segment_count() const { return audio_buses_.size(); }

  // InputController::SyncWriter implementation.
  void Write(const media::AudioBus* data,
             double volume,
             bool key_pressed,
             base::TimeTicks capture_time) final;

  void Close() final;

 private:
  friend class InputSyncWriterTest;

  // Called by Write(). Checks the time since last called and if larger than a
  // threshold logs info about that.
  void CheckTimeSinceLastWrite();

  // Push |data| and metadata to |audio_buffer_fifo_|. Returns true if
  // successful. Logs error and returns false if the fifo already reached the
  // maximum size.
  bool PushDataToFifo(const media::AudioBus* data,
                      double volume,
                      bool key_pressed,
                      base::TimeTicks capture_time);

  // Writes as much data as possible from the fifo (|overflow_buses_|) to the
  // shared memory ring buffer. Returns true if all operations were successful,
  // otherwise false.
  bool WriteDataFromFifoToSharedMemory();

  // Write audio parameters to current segment in shared memory.
  void WriteParametersToCurrentSegment(double volume,
                                       bool key_pressed,
                                       base::TimeTicks capture_time);

  // Signals over the socket that data has been written to the current segment.
  // Updates counters and returns true if successful. Logs error and returns
  // false if failure.
  bool SignalDataWrittenAndUpdateCounters();

  const base::RepeatingCallback<void(const std::string&)> log_callback_;

  // Socket used to signal that audio data is ready.
  const std::unique_ptr<base::CancelableSyncSocket> socket_;

  // Shared memory for audio data and associated metadata.
  base::ReadOnlySharedMemoryRegion shared_memory_region_;
  const base::WritableSharedMemoryMapping shared_memory_mapping_;

  // The size in bytes of a single audio segment in the shared memory.
  const uint32_t shared_memory_segment_size_;

  // Index of next segment to write.
  uint32_t current_segment_id_ = 0;

  // The time of the creation of this object.
  base::TimeTicks creation_time_;

  // The time of the last Write call.
  base::TimeTicks last_write_time_;

  // Size in bytes of each audio bus.
  const int audio_bus_memory_size_;

  // Increasing ID used for checking audio buffers are in correct sequence at
  // read side.
  uint32_t next_buffer_id_ = 0;

  // Next expected audio buffer index to have been read at the other side. We
  // will get the index read at the other side over the socket. Note that this
  // index does not correspond to |next_buffer_id_|, it's two separate counters.
  uint32_t next_read_buffer_index_ = 0;

  // Keeps track of number of filled buffer segments in the ring buffer to
  // ensure the we don't overwrite data that hasn't been read yet.
  size_t number_of_filled_segments_ = 0;

  // Counts the total number of calls to Write().
  size_t write_count_ = 0;

  // Counts the number of writes to the fifo instead of to the shared memory.
  size_t write_to_fifo_count_ = 0;

  // Counts the number of errors that causes data to be dropped, due to either
  // the fifo or the socket buffer being full.
  size_t write_error_count_ = 0;

  // Denotes that the most recent socket error has been logged. Used to avoid
  // log spam.
  bool had_socket_error_ = false;

  // Counts the fifo writes and errors we get during renderer process teardown
  // so that we can account for that (subtract) when we calculate the overall
  // counts.
  size_t trailing_write_to_fifo_count_ = 0;
  size_t trailing_write_error_count_ = 0;

  // Vector of audio buses allocated during construction and deleted in the
  // destructor.
  std::vector<std::unique_ptr<media::AudioBus>> audio_buses_;

  // Fifo for audio that is used in case there isn't room in the shared memory.
  // This can for example happen under load when the consumer side is starved.
  // It should ideally be rare, but we need to guarantee that the data arrives
  // since audio processing such as echo cancelling requires that to perform
  // properly.
  struct OverflowData {
    OverflowData(double volume,
                 bool key_pressed,
                 base::TimeTicks capture_time,
                 std::unique_ptr<media::AudioBus> audio_bus);
    ~OverflowData();
    OverflowData(OverflowData&&);
    OverflowData& operator=(OverflowData&& other);
    double volume_;
    bool key_pressed_;
    base::TimeTicks capture_time_;
    std::unique_ptr<media::AudioBus> audio_bus_;

   private:
    DISALLOW_COPY_AND_ASSIGN(OverflowData);
  };

  std::vector<OverflowData> overflow_data_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(InputSyncWriter);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_INPUT_SYNC_WRITER_H_
