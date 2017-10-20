// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/queued_request_dispatcher.h"

#include <inttypes.h>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/pattern.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/mac/mac_util.h"
#endif

using base::trace_event::MemoryDumpType;

namespace memory_instrumentation {

namespace {

// Returns the private memory footprint calculated from given |os_dump|.
//
// See design docs linked in the bugs for the rationale of the computation:
// - Linux/Android: https://crbug.com/707019 .
// - Mac OS: https://crbug.com/707021 .
// - Win: https://crbug.com/707022 .
uint32_t CalculatePrivateFootprintKb(const mojom::RawOSMemDump& os_dump) {
  DCHECK(os_dump.platform_private_footprint);
#if defined(OS_LINUX) || defined(OS_ANDROID)
  uint64_t rss_anon_bytes = os_dump.platform_private_footprint->rss_anon_bytes;
  uint64_t vm_swap_bytes = os_dump.platform_private_footprint->vm_swap_bytes;
  return (rss_anon_bytes + vm_swap_bytes) / 1024;
#elif defined(OS_MACOSX)
  // TODO(erikchen): This calculation is close, but not fully accurate. It
  // overcounts by anonymous shared memory.
  if (base::mac::IsAtLeastOS10_12()) {
    uint64_t phys_footprint_bytes =
        os_dump.platform_private_footprint->phys_footprint_bytes;
    return phys_footprint_bytes / 1024;
  } else {
    uint64_t internal_bytes =
        os_dump.platform_private_footprint->internal_bytes;
    uint64_t compressed_bytes =
        os_dump.platform_private_footprint->compressed_bytes;
    return (internal_bytes + compressed_bytes) / 1024;
  }
#elif defined(OS_WIN)
  return os_dump.platform_private_footprint->private_bytes / 1024;
#else
  return 0;
#endif
}

memory_instrumentation::mojom::OSMemDumpPtr CreatePublicOSDump(
    const mojom::RawOSMemDump& internal_os_dump) {
  mojom::OSMemDumpPtr os_dump = mojom::OSMemDump::New();

  os_dump->resident_set_kb = internal_os_dump.resident_set_kb;
  os_dump->private_footprint_kb = CalculatePrivateFootprintKb(internal_os_dump);
  return os_dump;
}

uint32_t GetDumpsSumKb(const std::string& pattern,
                       const base::trace_event::ProcessMemoryDump& pmd) {
  uint64_t sum = 0;
  for (const auto& kv : pmd.allocator_dumps()) {
    if (base::MatchPattern(kv.first /* name */, pattern))
      sum += kv.second->GetSizeInternal();
  }
  return sum / 1024;
}

mojom::ChromeMemDumpPtr CreateDumpSummary(
    const base::trace_event::ProcessMemoryDump& process_memory_dump) {
  mojom::ChromeMemDumpPtr result = mojom::ChromeMemDump::New();
  result->malloc_total_kb = GetDumpsSumKb("malloc", process_memory_dump);
  result->v8_total_kb = GetDumpsSumKb("v8/*", process_memory_dump);
  result->command_buffer_total_kb =
      GetDumpsSumKb("gpu/gl/textures/*", process_memory_dump);
  result->command_buffer_total_kb +=
      GetDumpsSumKb("gpu/gl/buffers/*", process_memory_dump);
  result->command_buffer_total_kb +=
      GetDumpsSumKb("gpu/gl/renderbuffers/*", process_memory_dump);

  // partition_alloc reports sizes for both allocated_objects and
  // partitions. The memory allocated_objects uses is a subset of
  // the partitions memory so to avoid double counting we only
  // count partitions memory.
  result->partition_alloc_total_kb =
      GetDumpsSumKb("partition_alloc/partitions/*", process_memory_dump);
  result->blink_gc_total_kb = GetDumpsSumKb("blink_gc", process_memory_dump);
  return result;
}

}  // namespace

void QueuedRequestDispatcher::SetUpAndDispatch(
    QueuedRequest* request,
    const std::vector<ClientInfo>& clients,
    const ChromeCallback& chrome_callback,
    const OsCallback& os_callback) {
  using ResponseType = QueuedRequest::PendingResponse::Type;
  DCHECK(!request->dump_in_progress);
  request->dump_in_progress = true;

  // A request must be either !VM_REGIONS_ONLY or, in the special case of the
  // heap profiler, must be of DETAILED type.
  DCHECK(request->wants_chrome_dumps() || request->wants_mmaps());

  request->start_time = base::Time::Now();

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN2(
      base::trace_event::MemoryDumpManager::kTraceCategory, "GlobalMemoryDump",
      TRACE_ID_LOCAL(request->args.dump_guid), "dump_type",
      base::trace_event::MemoryDumpTypeToString(request->args.dump_type),
      "level_of_detail",
      base::trace_event::MemoryDumpLevelOfDetailToString(
          request->args.level_of_detail));

  request->failed_memory_dump_count = 0;

  // Note: the service process itself is registered as a ClientProcess and
  // will be treated like any other process for the sake of memory dumps.
  request->pending_responses.clear();

  for (const auto& client_info : clients) {
    mojom::ClientProcess* client = client_info.client;
    request->responses[client].process_id = client_info.pid;
    request->responses[client].process_type = client_info.process_type;

    // Don't request a chrome memory dump at all if the client wants only the
    // processes' vm regions, which are retrieved via RequestOSMemoryDump().
    if (request->wants_chrome_dumps()) {
      request->pending_responses.insert({client, ResponseType::kChromeDump});
      client->RequestChromeMemoryDump(request->args,
                                      base::Bind(chrome_callback, client));
    }

// On most platforms each process can dump data about their own process
// so ask each process to do so Linux is special see below.
#if !defined(OS_LINUX)
    request->pending_responses.insert({client, ResponseType::kOSDump});
    client->RequestOSMemoryDump(request->wants_mmaps(), {base::kNullProcessId},
                                base::Bind(os_callback, client));
#endif  // !defined(OS_LINUX)
  }

// In some cases, OS stats can only be dumped from a privileged process to
// get around to sandboxing/selinux restrictions (see crbug.com/461788).
#if defined(OS_LINUX)
  std::vector<base::ProcessId> pids;
  mojom::ClientProcess* browser_client = nullptr;
  pids.reserve(clients.size());
  for (const auto& client_info : clients) {
    pids.push_back(client_info.pid);
    if (client_info.process_type == mojom::ProcessType::BROWSER) {
      browser_client = client_info.client;
    }
  }

  if (clients.size() > 0) {
    DCHECK(browser_client);
  }
  if (browser_client) {
    request->pending_responses.insert({browser_client, ResponseType::kOSDump});
    const auto callback = base::Bind(os_callback, browser_client);
    browser_client->RequestOSMemoryDump(request->wants_mmaps(), pids, callback);
  }
#endif  // defined(OS_LINUX)
}

void QueuedRequestDispatcher::Finalize(QueuedRequest* request,
                                       TracingObserver* tracing_observer) {
  DCHECK(request->dump_in_progress);
  DCHECK(request->pending_responses.empty());

  // Reconstruct a map of pid -> ProcessMemoryDump by reassembling the responses
  // received by the clients for this dump. In some cases the response coming
  // from one client can also provide the dump of OS counters for other
  // processes. A concrete case is Linux, where the browser process provides
  // details for the child processes to get around sandbox restrictions on
  // opening /proc pseudo files.

  struct RawDumpsForProcess {
    mojom::ProcessType process_type = mojom::ProcessType::OTHER;
    const base::trace_event::ProcessMemoryDump* raw_chrome_dump = nullptr;
    mojom::RawOSMemDump* raw_os_dump = nullptr;
  };

  std::map<base::ProcessId, RawDumpsForProcess> pid_to_results;
  for (auto& response : request->responses) {
    const base::ProcessId& original_pid = response.second.process_id;
    RawDumpsForProcess& raw_dumps = pid_to_results[original_pid];
    raw_dumps.process_type = response.second.process_type;

    // |chrome_dump| can be nullptr if this was a OS-counters only response.
    raw_dumps.raw_chrome_dump = response.second.chrome_dump.get();

    // |response| accumulates the replies received by each client process.
    // Depending on the OS each client process might return 1 chrome + 1 OS
    // dump each or, in the case of Linux, only 1 chrome dump each % the
    // browser process which will provide all the OS dumps.
    // In the former case (!OS_LINUX) we expect client processes to have
    // exactly one OS dump in their |response|, % the case when they
    // unexpectedly disconnect in the middle of a dump (e.g. because they
    // crash). In the latter case (OS_LINUX) we expect the full map to come
    // from the browser process response.
    OSMemDumpMap& extra_os_dumps = response.second.os_dumps;
#if defined(OS_LINUX)
    for (const auto& kv : extra_os_dumps) {
      auto pid = kv.first == base::kNullProcessId ? original_pid : kv.first;
      RawDumpsForProcess& extra_result = pid_to_results[pid];
      DCHECK_EQ(extra_result.raw_os_dump, nullptr);
      extra_result.raw_os_dump = kv.second.get();
    }
#else
    // This can be empty if the client disconnects before providing both
    // dumps. See UnregisterClientProcess().
    DCHECK_LE(extra_os_dumps.size(), 1u);
    for (const auto& kv : extra_os_dumps) {
      // When the OS dump comes from child processes, the pid is supposed to be
      // not used. We know the child process pid at the time of the request and
      // also wouldn't trust pids coming from child processes.
      DCHECK_EQ(base::kNullProcessId, kv.first);

      // Check we don't receive duplicate OS dumps for the same process.
      DCHECK_EQ(raw_dumps.raw_os_dump, nullptr);

      raw_dumps.raw_os_dump = kv.second.get();
    }
#endif
  }  // for (response : request->responses)

  // Build up the global dump by iterating on the |valid| process dumps.
  mojom::GlobalMemoryDumpPtr global_dump(mojom::GlobalMemoryDump::New());
  global_dump->process_dumps.reserve(pid_to_results.size());
  for (const auto& kv : pid_to_results) {
    const base::ProcessId pid = kv.first;
    const RawDumpsForProcess& raw_dumps = kv.second;

    // If we have the OS dump we should have the platform private footprint.
    DCHECK(!raw_dumps.raw_os_dump ||
           raw_dumps.raw_os_dump->platform_private_footprint);

    // Ignore incomplete results (can happen if the client crashes/disconnects).
    const bool valid =
        raw_dumps.raw_os_dump &&
        (!request->wants_chrome_dumps() || raw_dumps.raw_chrome_dump) &&
        (!request->wants_mmaps() ||
         (raw_dumps.raw_os_dump &&
          !raw_dumps.raw_os_dump->memory_maps.empty()));
    if (!valid)
      continue;

    mojom::OSMemDumpPtr os_dump = CreatePublicOSDump(*raw_dumps.raw_os_dump);
    if (request->add_to_trace) {
      tracing_observer->AddOsDumpToTraceIfEnabled(
          request->args, pid, os_dump.get(),
          &raw_dumps.raw_os_dump->memory_maps);
    }

    if (request->args.dump_type == MemoryDumpType::VM_REGIONS_ONLY) {
      DCHECK(request->wants_mmaps());
      os_dump->memory_maps_for_heap_profiler =
          std::move(raw_dumps.raw_os_dump->memory_maps);
    }

    // TODO(hjd): not sure we need an empty instance for the !SUMMARY_ONLY
    // requests. Check and make the else branch a nullptr otherwise.
    mojom::ChromeMemDumpPtr chrome_dump =
        request->should_return_summaries()
            ? CreateDumpSummary(*raw_dumps.raw_chrome_dump)
            : mojom::ChromeMemDump::New();

    mojom::ProcessMemoryDumpPtr pmd = mojom::ProcessMemoryDump::New();
    pmd->pid = pid;
    pmd->process_type = raw_dumps.process_type;
    pmd->os_dump = std::move(os_dump);
    pmd->chrome_dump = std::move(chrome_dump);
    global_dump->process_dumps.push_back(std::move(pmd));
  }

  const auto& callback = request->callback;
  const bool global_success = request->failed_memory_dump_count == 0;
  callback.Run(global_success, request->args.dump_guid, std::move(global_dump));
  UMA_HISTOGRAM_MEDIUM_TIMES("Memory.Experimental.Debug.GlobalDumpDuration",
                             base::Time::Now() - request->start_time);
  UMA_HISTOGRAM_COUNTS_1000(
      "Memory.Experimental.Debug.FailedProcessDumpsPerGlobalDump",
      request->failed_memory_dump_count);

  char guid_str[20];
  sprintf(guid_str, "0x%" PRIx64, request->args.dump_guid);
  TRACE_EVENT_NESTABLE_ASYNC_END2(
      base::trace_event::MemoryDumpManager::kTraceCategory, "GlobalMemoryDump",
      TRACE_ID_LOCAL(request->args.dump_guid), "dump_guid",
      TRACE_STR_COPY(guid_str), "success", global_success);
}

QueuedRequestDispatcher::ClientInfo::ClientInfo(mojom::ClientProcess* client,
                                                base::ProcessId pid,
                                                mojom::ProcessType process_type)
    : client(client), pid(pid), process_type(process_type) {}
QueuedRequestDispatcher::ClientInfo::~ClientInfo() {}

}  // namespace memory_instrumentation
