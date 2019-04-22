/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDE_PERFETTO_TRACING_CORE_STARTUP_TRACE_WRITER_H_
#define INCLUDE_PERFETTO_TRACING_CORE_STARTUP_TRACE_WRITER_H_

#include <memory>
#include <mutex>
#include <set>
#include <vector>

#include "perfetto/base/export.h"
#include "perfetto/base/optional.h"
#include "perfetto/base/thread_checker.h"
#include "perfetto/protozero/message_handle.h"
#include "perfetto/protozero/scattered_heap_buffer.h"
#include "perfetto/protozero/scattered_stream_writer.h"
#include "perfetto/tracing/core/basic_types.h"
#include "perfetto/tracing/core/trace_writer.h"

namespace perfetto {

class SharedMemoryArbiterImpl;
class StartupTraceWriterRegistryHandle;

namespace protos {
namespace pbzero {
class TracePacket;
}  // namespace pbzero
}  // namespace protos

// Facilitates writing trace events in early phases of an application's startup
// when the perfetto service is not available yet.
//
// Until the service is available, producer threads instantiate an unbound
// StartupTraceWriter instance (via a StartupTraceWriterRegistry) and use it to
// emit trace events. Each writer will record the serialized trace events into a
// temporary local memory buffer.
//
// Once the service is available, the producer binds each StartupTraceWriter to
// the SMB by calling SharedMemoryArbiter::BindStartupTraceWriter(). The data in
// the writer's local buffer will then be copied into the SMB and the any future
// writes will proxy directly to a new SMB-backed TraceWriter.
//
// Writing to the temporary local trace buffer is guarded by a lock and flag to
// allow binding the writer from a different thread. When the writer starts
// writing data by calling NewTracePacket(), the writer thread acquires the lock
// to set a flag indicating that a write is in progress. Once the packet is
// finalized, the flag is reset. To bind the writer, the lock is acquired while
// the flag is unset and released only once binding completed, thereby blocking
// the writer thread from starting a write concurrently.
//
// While unbound, the writer thread should finalize each TracePacket as soon as
// possible to ensure that it doesn't block binding the writer.
class PERFETTO_EXPORT StartupTraceWriter
    : public TraceWriter,
      public protozero::MessageHandleBase::FinalizationListener {
 public:
  // Create a StartupTraceWriter bound to |trace_writer|. Should only be called
  // on the writer thread.
  explicit StartupTraceWriter(std::unique_ptr<TraceWriter> trace_writer);

  ~StartupTraceWriter() override;

  // TraceWriter implementation. These methods should only be called on the
  // writer thread.
  TracePacketHandle NewTracePacket() override;
  void Flush(std::function<void()> callback = {}) override;

  // Note that this will return 0 until the first TracePacket was started after
  // binding.
  WriterID writer_id() const override;

  uint64_t written() const override;

  // Returns |true| if the writer thread has observed that the writer was bound
  // to an SMB. Should only be called on the writer thread.
  //
  // The writer thread can use the return value to determine whether it needs to
  // finalize the current TracePacket as soon as possible. It is only safe for
  // the writer to batch data into a single TracePacket over a longer time
  // period when this returns |true|.
  bool was_bound() const {
    PERFETTO_DCHECK_THREAD(writer_thread_checker_);
    return was_bound_;
  }

  // Should only be called on the writer thread.
  size_t used_buffer_size();

 private:
  friend class StartupTraceWriterRegistry;
  friend class StartupTraceWriterTest;

  // Create an unbound StartupTraceWriter associated with the registry pointed
  // to by the handle. The writer can later be bound by calling
  // BindToTraceWriter(). The registry handle may be nullptr in tests.
  StartupTraceWriter(std::shared_ptr<StartupTraceWriterRegistryHandle>);

  StartupTraceWriter(const StartupTraceWriter&) = delete;
  StartupTraceWriter& operator=(const StartupTraceWriter&) = delete;

  // Bind this StartupTraceWriter to the provided SharedMemoryArbiterImpl.
  // Called by StartupTraceWriterRegistry::BindToArbiter().
  //
  // This method can be called on any thread. If any data was written locally
  // before the writer was bound, BindToArbiter() will copy this data into
  // chunks in the provided target buffer via the SMB. Any future packets will
  // be directly written into the SMB via a newly obtained TraceWriter from the
  // arbiter.
  //
  // Will fail and return |false| if a concurrent write is in progress. Returns
  // |true| if successfully bound and should then not be called again.
  bool BindToArbiter(SharedMemoryArbiterImpl*,
                     BufferID target_buffer) PERFETTO_WARN_UNUSED_RESULT;

  // protozero::MessageHandleBase::FinalizationListener implementation.
  void OnMessageFinalized(protozero::Message* message) override;

  void OnTracePacketCompleted();
  ChunkID CommitLocalBufferChunks(SharedMemoryArbiterImpl*, WriterID, BufferID);

  PERFETTO_THREAD_CHECKER(writer_thread_checker_)

  std::shared_ptr<StartupTraceWriterRegistryHandle> registry_handle_;

  // Only set and accessed from the writer thread. The writer thread flips this
  // bit when it sees that trace_writer_ is set (while holding the lock).
  // Caching this fact in this variable avoids the need to acquire the lock to
  // check on later calls to NewTracePacket().
  bool was_bound_ = false;

  // All variables below this point are protected by |lock_|.
  std::mutex lock_;

  // Never reset once it is changed from |nullptr|.
  std::unique_ptr<TraceWriter> trace_writer_ = nullptr;

  // Local memory buffer for trace packets written before the writer is bound.
  std::unique_ptr<protozero::ScatteredHeapBuffer> memory_buffer_;
  std::unique_ptr<protozero::ScatteredStreamWriter> memory_stream_writer_;

  std::vector<uint32_t> packet_sizes_;

  // Whether the writer thread is currently writing a TracePacket.
  bool write_in_progress_ = false;

  // The packet returned via NewTracePacket() while the writer is unbound. Reset
  // to |nullptr| once bound. Owned by this class, TracePacketHandle has just a
  // pointer to it.
  std::unique_ptr<protos::pbzero::TracePacket> cur_packet_;
};

}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_TRACING_CORE_STARTUP_TRACE_WRITER_H_
