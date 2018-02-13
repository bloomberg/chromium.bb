// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/memory_instrumentation/queued_request_dispatcher.h"

#include <inttypes.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/pattern.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "services/resource_coordinator/memory_instrumentation/graph_processor.h"
#include "services/resource_coordinator/memory_instrumentation/switches.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/mac/mac_util.h"
#endif

using base::trace_event::MemoryAllocatorDump;
using base::trace_event::MemoryDumpLevelOfDetail;
using base::trace_event::TracedValue;
using Node = memory_instrumentation::GlobalDumpGraph::Node;

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
#if defined(OS_LINUX) || defined(OS_ANDROID)
  os_dump->private_footprint_swap_kb =
      internal_os_dump.platform_private_footprint->vm_swap_bytes / 1024;
#endif
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

void NodeAsValueIntoRecursively(const GlobalDumpGraph::Node& node,
                                TracedValue* value,
                                std::vector<base::StringPiece>* path) {
  // Don't dump the root node.
  if (!path->empty()) {
    std::string string_conversion_buffer;

    std::string name = base::JoinString(*path, "/");
    value->BeginDictionaryWithCopiedName(name);

    if (!node.guid().empty())
      value->SetString("guid", node.guid().ToString());

    value->BeginDictionary("attrs");
    for (const auto& name_to_entry : node.const_entries()) {
      const auto& entry = name_to_entry.second;
      value->BeginDictionaryWithCopiedName(name_to_entry.first);
      switch (entry.type) {
        case GlobalDumpGraph::Node::Entry::kUInt64:
          base::SStringPrintf(&string_conversion_buffer, "%" PRIx64,
                              entry.value_uint64);
          value->SetString("type", MemoryAllocatorDump::kTypeScalar);
          value->SetString("value", string_conversion_buffer);
          break;
        case GlobalDumpGraph::Node::Entry::kString:
          value->SetString("type", MemoryAllocatorDump::kTypeString);
          value->SetString("value", entry.value_string);
          break;
      }
      switch (entry.units) {
        case GlobalDumpGraph::Node::Entry::ScalarUnits::kBytes:
          value->SetString("units", MemoryAllocatorDump::kUnitsBytes);
          break;
        case GlobalDumpGraph::Node::Entry::ScalarUnits::kObjects:
          value->SetString("units", MemoryAllocatorDump::kUnitsObjects);
          break;
      }
      value->EndDictionary();
    }
    value->EndDictionary();  // "attrs": { ... }

    if (node.is_weak())
      value->SetInteger("flags", MemoryAllocatorDump::Flags::WEAK);

    value->EndDictionary();  // "allocator_name/heap_subheap": { ... }
  }

  for (const auto& name_to_child : node.const_children()) {
    path->push_back(name_to_child.first);
    NodeAsValueIntoRecursively(*name_to_child.second, value, path);
    path->pop_back();
  }
}

std::unique_ptr<TracedValue> GetChromeDumpTracedValue(
    const GlobalDumpGraph::Process& process) {
  std::unique_ptr<TracedValue> traced_value = std::make_unique<TracedValue>();
  if (!process.root()->const_children().empty()) {
    traced_value->BeginDictionary("allocators");
    std::vector<base::StringPiece> path;
    NodeAsValueIntoRecursively(*process.root(), traced_value.get(), &path);
    traced_value->EndDictionary();
  }
  return traced_value;
}

std::unique_ptr<TracedValue> GetChromeDumpAndGlobalAndEdgesTracedValue(
    const GlobalDumpGraph::Process& process,
    const GlobalDumpGraph::Process& global_process,
    const std::forward_list<GlobalDumpGraph::Edge>& edges) {
  std::unique_ptr<TracedValue> traced_value = std::make_unique<TracedValue>();
  bool suppress_graphs = process.root()->const_children().empty() &&
                         global_process.root()->const_children().empty();

  if (!suppress_graphs) {
    traced_value->BeginDictionary("allocators");
    std::vector<base::StringPiece> path;
    NodeAsValueIntoRecursively(*process.root(), traced_value.get(), &path);
    NodeAsValueIntoRecursively(*global_process.root(), traced_value.get(),
                               &path);
    traced_value->EndDictionary();
  }
  traced_value->BeginArray("allocators_graph");
  for (const auto& edge : edges) {
    traced_value->BeginDictionary();
    traced_value->SetString("source", edge.source()->guid().ToString());
    traced_value->SetString("target", edge.target()->guid().ToString());
    traced_value->SetInteger("importance", edge.priority());
    traced_value->EndDictionary();
  }
  traced_value->EndArray();
  return traced_value;
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
      TRACE_ID_LOCAL(request->dump_guid), "dump_type",
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

    // If we're only looking for a single pid process, then ignore clients
    // with different pid.
    if (request->args.pid != base::kNullProcessId &&
        request->args.pid != client_info.pid) {
      continue;
    }

    request->responses[client].process_id = client_info.pid;
    request->responses[client].process_type = client_info.process_type;

    // Don't request a chrome memory dump at all if the client wants only the
    // processes' vm regions, which are retrieved via RequestOSMemoryDump().
    if (request->wants_chrome_dumps()) {
      request->pending_responses.insert({client, ResponseType::kChromeDump});
      client->RequestChromeMemoryDump(request->GetRequestArgs(),
                                      base::Bind(chrome_callback, client));
    }

// On most platforms each process can dump data about their own process
// so ask each process to do so Linux is special see below.
#if !defined(OS_LINUX)
    request->pending_responses.insert({client, ResponseType::kOSDump});
    client->RequestOSMemoryDump(request->wants_mmaps(), {base::kNullProcessId},
                                base::Bind(os_callback, client));
#endif  // !defined(OS_LINUX)

    // If we are in the single pid case, then we've already found the only
    // process we're looking for.
    if (request->args.pid != base::kNullProcessId)
      break;
  }

// In some cases, OS stats can only be dumped from a privileged process to
// get around to sandboxing/selinux restrictions (see crbug.com/461788).
#if defined(OS_LINUX)
  std::vector<base::ProcessId> pids;
  mojom::ClientProcess* browser_client = nullptr;
  pids.reserve(request->args.pid == base::kNullProcessId ? clients.size() : 1);
  for (const auto& client_info : clients) {
    if (request->args.pid == base::kNullProcessId ||
        client_info.pid == request->args.pid) {
      pids.push_back(client_info.pid);
    }
    if (client_info.process_type == mojom::ProcessType::BROWSER) {
      browser_client = client_info.client;
    }
  }
  if (clients.size() > 0) {
    DCHECK(browser_client);
  }
  if (browser_client && pids.size() > 0) {
    request->pending_responses.insert({browser_client, ResponseType::kOSDump});
    const auto callback = base::Bind(os_callback, browser_client);
    browser_client->RequestOSMemoryDump(request->wants_mmaps(), pids, callback);
  }
#endif  // defined(OS_LINUX)

  // In this case, we have not found the pid we are looking for so increment
  // the failed dump count and exit.
  if (request->args.pid != base::kNullProcessId &&
      request->pending_responses.empty()) {
    request->failed_memory_dump_count++;
    return;
  }
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

  // All the pointers in the maps will continue to be owned by |request|
  // which outlives these containers.
  std::map<base::ProcessId, mojom::ProcessType> pid_to_process_type;
  std::map<base::ProcessId, const base::trace_event::ProcessMemoryDump*>
      pid_to_pmd;
  std::map<base::ProcessId, mojom::RawOSMemDump*> pid_to_os_dump;
  for (auto& response : request->responses) {
    const base::ProcessId& original_pid = response.second.process_id;
    pid_to_process_type[original_pid] = response.second.process_type;

    // |chrome_dump| can be nullptr if this was a OS-counters only response.
    pid_to_pmd[original_pid] = response.second.chrome_dump.get();

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
      DCHECK_EQ(pid_to_os_dump[pid], nullptr);
      pid_to_os_dump[pid] = kv.second.get();
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
      DCHECK_EQ(pid_to_os_dump[original_pid], nullptr);

      pid_to_os_dump[original_pid] = kv.second.get();
    }
#endif
  }  // for (response : request->responses)

  // Generate the global memory graph from the map of pids to dumps, removing
  // weak nodes.
  std::unique_ptr<GlobalDumpGraph> global_graph =
      GraphProcessor::CreateMemoryGraph(pid_to_pmd);
  GraphProcessor::RemoveWeakNodesFromGraph(global_graph.get());

  // Compute the shared memory footprint for each process from the graph.
  std::map<base::ProcessId, uint64_t> shared_footprints =
      GraphProcessor::ComputeSharedFootprintFromGraph(*global_graph);

  // Perform the rest of the computation on the graph.
  GraphProcessor::AddOverheadsAndPropogateEntries(global_graph.get());
  GraphProcessor::CalculateSizesForGraph(global_graph.get());

  // Build up the global dump by iterating on the |valid| process dumps.
  mojom::GlobalMemoryDumpPtr global_dump(mojom::GlobalMemoryDump::New());
  global_dump->process_dumps.reserve(request->responses.size());
  for (const auto& response : request->responses) {
    base::ProcessId pid = response.second.process_id;

    // On Linux, we may also have the browser process as a response.
    // Just ignore it if we are looking for the single pid case.
    if (request->args.pid != base::kNullProcessId && pid != request->args.pid)
      continue;

    // These pointers are owned by |request|.
    mojom::RawOSMemDump* raw_os_dump = pid_to_os_dump[pid];
    auto* raw_chrome_dump = pid_to_pmd[pid];

    // If we have the OS dump we should have the platform private footprint.
    DCHECK(!raw_os_dump || raw_os_dump->platform_private_footprint);

    // If the raw dump exists, create a summarised version of it.
    mojom::OSMemDumpPtr os_dump = nullptr;
    if (raw_os_dump) {
      os_dump = CreatePublicOSDump(*raw_os_dump);
      os_dump->shared_footprint_kb = shared_footprints[pid] / 1024;
    }

    // Trace the OS and Chrome dumps if they exist.
    if (request->args.add_to_trace) {
      if (raw_os_dump) {
        bool trace_os_success = tracing_observer->AddOsDumpToTraceIfEnabled(
            request->GetRequestArgs(), pid, os_dump.get(),
            &raw_os_dump->memory_maps);
        if (!trace_os_success)
          request->failed_memory_dump_count++;
      }

      if (raw_chrome_dump) {
        bool trace_chrome_success = AddChromeMemoryDumpToTrace(
            request->GetRequestArgs(), pid, *raw_chrome_dump, *global_graph,
            pid_to_process_type, tracing_observer);
        if (!trace_chrome_success)
          request->failed_memory_dump_count++;
      }
    }

    // Ignore incomplete results (can happen if the client crashes/disconnects).
    const bool valid = raw_os_dump &&
                       (!request->wants_chrome_dumps() || raw_chrome_dump) &&
                       (!request->wants_mmaps() ||
                        (raw_os_dump && !raw_os_dump->memory_maps.empty()));

    if (!valid)
      continue;

    if (request->args.level_of_detail ==
        MemoryDumpLevelOfDetail::VM_REGIONS_ONLY_FOR_HEAP_PROFILER) {
      DCHECK(request->wants_mmaps());
      os_dump->memory_maps_for_heap_profiler =
          std::move(raw_os_dump->memory_maps);
    }

    // TODO(hjd): not sure we need an empty instance for the !SUMMARY_ONLY
    // requests. Check and make the else branch a nullptr otherwise.
    mojom::ChromeMemDumpPtr chrome_dump =
        request->should_return_summaries() ? CreateDumpSummary(*raw_chrome_dump)
                                           : mojom::ChromeMemDump::New();

    // If we have to return a summary, add all entries for the requested
    // allocator dumps.
    if (request->should_return_summaries()) {
      const auto& process_graph =
          global_graph->process_dump_graphs().find(pid)->second;
      for (const std::string& name : request->args.allocator_dump_names) {
        auto* node = process_graph->FindNode(name);
        // Silently ignore any missing node in the process graph.
        if (!node)
          continue;
        std::unordered_map<std::string, uint64_t> numeric_entries;
        for (const auto& entry : *node->entries()) {
          if (entry.second.type == Node::Entry::Type::kUInt64)
            numeric_entries.emplace(entry.first, entry.second.value_uint64);
        }
        chrome_dump->entries_for_allocator_dumps.emplace(
            name, mojom::AllocatorMemDump::New(std::move(numeric_entries)));
      }
    }

    mojom::ProcessMemoryDumpPtr pmd = mojom::ProcessMemoryDump::New();
    pmd->pid = pid;
    pmd->process_type = pid_to_process_type[pid];
    pmd->os_dump = std::move(os_dump);
    pmd->chrome_dump = std::move(chrome_dump);
    global_dump->process_dumps.push_back(std::move(pmd));
  }

  const bool global_success = request->failed_memory_dump_count == 0;

  // In the single process-case, we want to ensure that global_success
  // is true if and only if global_dump is not nullptr.
  if (request->args.pid != base::kNullProcessId && !global_success) {
    global_dump = nullptr;
  }
  const auto& callback = request->callback;
  callback.Run(global_success, request->dump_guid, std::move(global_dump));
  UMA_HISTOGRAM_MEDIUM_TIMES("Memory.Experimental.Debug.GlobalDumpDuration",
                             base::Time::Now() - request->start_time);
  UMA_HISTOGRAM_COUNTS_1000(
      "Memory.Experimental.Debug.FailedProcessDumpsPerGlobalDump",
      request->failed_memory_dump_count);

  char guid_str[20];
  sprintf(guid_str, "0x%" PRIx64, request->dump_guid);
  TRACE_EVENT_NESTABLE_ASYNC_END2(
      base::trace_event::MemoryDumpManager::kTraceCategory, "GlobalMemoryDump",
      TRACE_ID_LOCAL(request->dump_guid), "dump_guid", TRACE_STR_COPY(guid_str),
      "success", global_success);
}

bool QueuedRequestDispatcher::AddChromeMemoryDumpToTrace(
    const base::trace_event::MemoryDumpRequestArgs& args,
    base::ProcessId pid,
    const base::trace_event::ProcessMemoryDump& raw_chrome_dump,
    const GlobalDumpGraph& global_graph,
    const std::map<base::ProcessId, mojom::ProcessType>& pid_to_process_type,
    TracingObserver* tracing_observer) {
  bool is_chrome_tracing_enabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableChromeTracingComputation);
  if (!is_chrome_tracing_enabled) {
    return tracing_observer->AddChromeDumpToTraceIfEnabled(args, pid,
                                                           &raw_chrome_dump);
  }
  if (!tracing_observer->ShouldAddToTrace(args))
    return false;

  const GlobalDumpGraph::Process& process =
      *global_graph.process_dump_graphs().find(pid)->second;

  std::unique_ptr<TracedValue> traced_value;
  if (pid_to_process_type.find(pid)->second == mojom::ProcessType::BROWSER) {
    traced_value = GetChromeDumpAndGlobalAndEdgesTracedValue(
        process, *global_graph.shared_memory_graph(), global_graph.edges());
  } else {
    traced_value = GetChromeDumpTracedValue(process);
  }
  tracing_observer->AddToTrace(args, pid, std::move(traced_value));

  return true;
}

QueuedRequestDispatcher::ClientInfo::ClientInfo(mojom::ClientProcess* client,
                                                base::ProcessId pid,
                                                mojom::ProcessType process_type)
    : client(client), pid(pid), process_type(process_type) {}
QueuedRequestDispatcher::ClientInfo::~ClientInfo() {}

}  // namespace memory_instrumentation
