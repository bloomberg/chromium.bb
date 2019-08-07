// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/connection_manager.h"

#include "base/bind.h"
#include "base/json/string_escape.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "components/services/heap_profiling/json_exporter.h"
#include "components/services/heap_profiling/public/cpp/client.h"

namespace heap_profiling {

namespace {
const size_t kMinSizeThreshold = 16 * 1024;
const size_t kMinCountThreshold = 1024;
}  // namespace

ConnectionManager::DumpArgs::DumpArgs() = default;
ConnectionManager::DumpArgs::DumpArgs(DumpArgs&& other) noexcept
    : backtrace_storage_lock(std::move(other.backtrace_storage_lock)) {}
ConnectionManager::DumpArgs::~DumpArgs() = default;

// Tracking information for DumpProcessForTracing(). This struct is
// refcounted since there will be many background thread calls (one for each
// AllocationTracker) and the callback is only issued when each has
// responded.
//
// This class is not threadsafe, its members must only be accessed on the
// I/O thread.
struct ConnectionManager::DumpProcessesForTracingTracking
    : public ConnectionManager::DumpArgs,
      public base::RefCountedThreadSafe<DumpProcessesForTracingTracking> {
  DumpProcessesForTracingTracking() = default;

  // Number of processes we're still waiting on responses for. When this gets
  // to 0, the callback will be issued.
  size_t waiting_responses = 0;

  // Callback to issue when dumps are complete.
  DumpProcessesForTracingCallback callback;

  // Info about the request.
  VmRegions vm_regions;

  // Collects the results.
  std::vector<memory_instrumentation::mojom::SharedBufferWithSizePtr> results;

 private:
  friend class base::RefCountedThreadSafe<DumpProcessesForTracingTracking>;
  virtual ~DumpProcessesForTracingTracking() = default;
};

struct ConnectionManager::Connection {
  Connection(CompleteCallback complete_cb,
             mojom::ProfilingClientPtr client,
             mojom::ProcessType process_type,
             uint32_t sampling_rate,
             mojom::StackMode stack_mode)
      : client(std::move(client)),
        process_type(process_type),
        stack_mode(stack_mode),
        sampling_rate(sampling_rate) {
    this->client.set_connection_error_handler(std::move(complete_cb));
  }

  bool HeapDumpNeedsVmRegions() {
    return stack_mode == mojom::StackMode::NATIVE_WITHOUT_THREAD_NAMES ||
           stack_mode == mojom::StackMode::NATIVE_WITH_THREAD_NAMES ||
           stack_mode == mojom::StackMode::MIXED;
  }

  mojom::ProfilingClientPtr client;
  mojom::ProcessType process_type;
  mojom::StackMode stack_mode;

  // When sampling is enabled, allocations are recorded with probability (size /
  // sampling_rate) when size < sampling_rate. When size >= sampling_rate, the
  // aggregate probability of an allocation being recorded is 1.0, but the math
  // and details are tricky. See
  // https://bugs.chromium.org/p/chromium/issues/detail?id=810748#c4.
  // A |sampling_rate| of 1 is equivalent to recording all allocations.
  uint32_t sampling_rate = 1;
};

ConnectionManager::ConnectionManager() {
  metrics_timer_.Start(
      FROM_HERE, base::TimeDelta::FromHours(24),
      base::Bind(&ConnectionManager::ReportMetrics, base::Unretained(this)));
}
ConnectionManager::~ConnectionManager() = default;

void ConnectionManager::OnNewConnection(base::ProcessId pid,
                                        mojom::ProfilingClientPtr client,
                                        mojom::ProcessType process_type,
                                        mojom::ProfilingParamsPtr params) {
  base::AutoLock lock(connections_lock_);

  // Attempting to start profiling on an already profiled processs should have
  // no effect.
  if (connections_.find(pid) != connections_.end())
    return;

  // It's theoretically possible that we started profiling a process, the
  // profiling was stopped [e.g. by hitting the 10-s timeout], and then we tried
  // to start profiling again. The ProfilingClient will refuse to start again.
  // But the ConnectionManager will not be able to distinguish this
  // never-started ProfilingClient from a brand new ProfilingClient that happens
  // to share the same pid. This is a rare condition which should only happen
  // when the user is attempting to manually start profiling for processes, so
  // we ignore this edge case.

  CompleteCallback complete_cb =
      base::BindOnce(&ConnectionManager::OnConnectionComplete,
                     weak_factory_.GetWeakPtr(), pid);

  auto connection = std::make_unique<Connection>(
      std::move(complete_cb), std::move(client), process_type,
      params->sampling_rate, params->stack_mode);
  connection->client->StartProfiling(std::move(params));
  connections_[pid] = std::move(connection);
}

std::vector<base::ProcessId> ConnectionManager::GetConnectionPids() {
  base::AutoLock lock(connections_lock_);
  std::vector<base::ProcessId> results;
  results.reserve(connections_.size());
  for (const auto& pair : connections_)
    results.push_back(pair.first);
  return results;
}

std::vector<base::ProcessId>
ConnectionManager::GetConnectionPidsThatNeedVmRegions() {
  base::AutoLock lock(connections_lock_);
  std::vector<base::ProcessId> results;
  results.reserve(connections_.size());
  for (const auto& pair : connections_) {
    if (pair.second->HeapDumpNeedsVmRegions())
      results.push_back(pair.first);
  }
  return results;
}

void ConnectionManager::OnConnectionComplete(base::ProcessId pid) {
  base::AutoLock lock(connections_lock_);
  auto found = connections_.find(pid);
  CHECK(found != connections_.end());
  connections_.erase(found);
}

void ConnectionManager::ReportMetrics() {
  base::AutoLock lock(connections_lock_);
  for (auto& pair : connections_) {
    UMA_HISTOGRAM_ENUMERATION("OutOfProcessHeapProfiling.ProfiledProcess.Type",
                              pair.second->process_type,
                              static_cast<int>(mojom::ProcessType::LAST) + 1);
  }
}

void ConnectionManager::DumpProcessesForTracing(
    bool strip_path_from_mapped_files,
    DumpProcessesForTracingCallback callback,
    VmRegions vm_regions) {
  base::AutoLock lock(connections_lock_);

  // Early out if there are no connections.
  if (connections_.empty()) {
    std::move(callback).Run(
        std::vector<memory_instrumentation::mojom::SharedBufferWithSizePtr>());
    return;
  }

  auto tracking = base::MakeRefCounted<DumpProcessesForTracingTracking>();
  tracking->backtrace_storage_lock =
      BacktraceStorage::Lock(&backtrace_storage_);
  tracking->waiting_responses = connections_.size();
  tracking->callback = std::move(callback);
  tracking->vm_regions = std::move(vm_regions);
  tracking->results.reserve(connections_.size());

  for (auto& it : connections_) {
    base::ProcessId pid = it.first;
    Connection* connection = it.second.get();
    connection->client->RetrieveHeapProfile(base::BindOnce(
        &ConnectionManager::HeapProfileRetrieved, weak_factory_.GetWeakPtr(),
        tracking, pid, connection->process_type, strip_path_from_mapped_files,
        connection->sampling_rate));
  }
}

void ConnectionManager::HeapProfileRetrieved(
    scoped_refptr<DumpProcessesForTracingTracking> tracking,
    base::ProcessId pid,
    mojom::ProcessType process_type,
    bool strip_path_from_mapped_files,
    uint32_t sampling_rate,
    mojom::HeapProfilePtr profile) {
  AllocationCountMap counts;
  ContextMap context_map;
  AddressToStringMap string_map;
  BacktraceStorage backtrace_storage;
  BacktraceStorage::Lock backtrace_storage_lock(&backtrace_storage);

  bool success = true;
  for (const mojom::HeapProfileSamplePtr& sample : profile->samples) {
    int context_id = 0;
    if (sample->context_id) {
      auto it = profile->strings.find(sample->context_id);
      if (it == profile->strings.end()) {
        success = false;
        break;
      }
      const std::string& context = it->second;
      // Escape the strings early, to simplify exporting a heap dump.
      std::string escaped_context;
      base::EscapeJSONString(context, false /* put_in_quotes */,
                             &escaped_context);
      context_id = context_map
                       .emplace(std::move(escaped_context),
                                static_cast<int>(context_map.size() + 1))
                       .first->second;
    }
    const Backtrace* backtrace = backtrace_storage.Insert(
        std::vector<Address>(sample->stack.begin(), sample->stack.end()));
    AllocationEvent alloc(sample->allocator, Address(0), sample->size,
                          backtrace, context_id);
    ++counts[alloc];
  }

  for (const auto& str : profile->strings) {
    std::string quoted_string;
    // Escape the strings before saving them, to simplify exporting a heap dump.
    base::EscapeJSONString(str.second, false /* put_in_quotes */,
                           &quoted_string);
    string_map.emplace(str.first, std::move(quoted_string));
  }

  DCHECK(success);
  DoDumpOneProcessForTracing(std::move(tracking), pid, process_type,
                             strip_path_from_mapped_files, sampling_rate,
                             success, std::move(counts), std::move(context_map),
                             std::move(string_map));
}

void ConnectionManager::DoDumpOneProcessForTracing(
    scoped_refptr<DumpProcessesForTracingTracking> tracking,
    base::ProcessId pid,
    mojom::ProcessType process_type,
    bool strip_path_from_mapped_files,
    uint32_t sampling_rate,
    bool success,
    AllocationCountMap counts,
    ContextMap context,
    AddressToStringMap mapped_strings) {
  // All code paths through here must issue the callback when waiting_responses
  // is 0 or the browser will wait forever for the dump.
  DCHECK(tracking->waiting_responses > 0);

  if (!success) {
    tracking->waiting_responses--;
    if (tracking->waiting_responses == 0)
      std::move(tracking->callback).Run(std::move(tracking->results));
    return;
  }

  CHECK(tracking->backtrace_storage_lock.IsLocked());
  ExportParams params;
  params.allocs = std::move(counts);

  auto it = tracking->vm_regions.find(pid);
  if (it != tracking->vm_regions.end()) {
    params.maps = std::move(it->second);
  }

  params.context_map = std::move(context);
  params.mapped_strings = std::move(mapped_strings);
  params.process_type = process_type;
  params.min_size_threshold = kMinSizeThreshold;
  params.min_count_threshold = kMinCountThreshold;
  params.strip_path_from_mapped_files = strip_path_from_mapped_files;
  params.next_id = next_id_;
  params.sampling_rate = sampling_rate;

  std::ostringstream oss;
  ExportMemoryMapsAndV2StackTraceToJSON(&params, oss);
  std::string reply = oss.str();
  size_t reply_size = reply.size();
  next_id_ = params.next_id;

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          [](size_t size) {
            // This call sends a synchronous IPC to the browser process.
            return mojo::SharedBufferHandle::Create(size);
          },
          reply_size),
      base::BindOnce(
          [](std::string reply, base::ProcessId pid,
             scoped_refptr<DumpProcessesForTracingTracking> tracking,
             mojo::ScopedSharedBufferHandle buffer) {
            if (!buffer.is_valid()) {
              DLOG(ERROR) << "Could not create Mojo shared buffer";
            } else {
              mojo::ScopedSharedBufferMapping mapping =
                  buffer->Map(reply.size());
              if (!mapping) {
                DLOG(ERROR) << "Could not map Mojo shared buffer";
              } else {
                memcpy(mapping.get(), reply.c_str(), reply.size());

                memory_instrumentation::mojom::SharedBufferWithSizePtr result =
                    memory_instrumentation::mojom::SharedBufferWithSize::New();
                result->buffer = std::move(buffer);
                result->size = reply.size();
                result->pid = pid;
                tracking->results.push_back(std::move(result));
              }
            }

            // When all responses complete, issue done callback.
            if (--tracking->waiting_responses == 0)
              std::move(tracking->callback).Run(std::move(tracking->results));
          },
          std::move(reply), pid, std::move(tracking)));
}

}  // namespace heap_profiling
