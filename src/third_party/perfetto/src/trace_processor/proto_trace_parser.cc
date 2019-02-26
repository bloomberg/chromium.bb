/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "src/trace_processor/proto_trace_parser.h"

#include <string.h>

#include <string>

#include "perfetto/base/logging.h"
#include "perfetto/base/string_view.h"
#include "perfetto/base/utils.h"
#include "perfetto/protozero/proto_decoder.h"
#include "perfetto/traced/sys_stats_counters.h"
#include "src/trace_processor/event_tracker.h"
#include "src/trace_processor/process_tracker.h"
#include "src/trace_processor/slice_tracker.h"
#include "src/trace_processor/trace_processor_context.h"

#include "perfetto/trace/trace.pb.h"
#include "perfetto/trace/trace_packet.pb.h"

namespace perfetto {
namespace trace_processor {

// We have to handle trace_marker events of a few different types:
// 1. some random text
// 2. B|1636|pokeUserActivity
// 3. E|1636
// 4. C|1636|wq:monitor|0
bool ParseSystraceTracePoint(base::StringView str, SystraceTracePoint* out) {
  // THIS char* IS NOT NULL TERMINATED.
  const char* s = str.data();
  size_t len = str.size();

  // If str matches '[BEC]\|[0-9]+[\|\n]' set tid_length to the length of
  // the number. Otherwise return false.
  if (s[1] != '|' && s[1] != '\n')
    return false;
  if (s[0] != 'B' && s[0] != 'E' && s[0] != 'C')
    return false;
  size_t tid_length = 0;
  for (size_t i = 2; i < len; i++) {
    if (s[i] == '|' || s[i] == '\n') {
      tid_length = i - 2;
      break;
    }
    if (s[i] < '0' || s[i] > '9')
      return false;
  }

  if (tid_length == 0) {
    out->tid = 0;
  } else {
    std::string tid_str(s + 2, tid_length);
    out->tid = static_cast<uint32_t>(std::stoi(tid_str.c_str()));
  }

  out->phase = s[0];
  switch (s[0]) {
    case 'B': {
      size_t name_index = 2 + tid_length + 1;
      out->name = base::StringView(s + name_index, len - name_index);
      return true;
    }
    case 'E': {
      return true;
    }
    case 'C': {
      size_t name_index = 2 + tid_length + 1;
      size_t name_length = 0;
      for (size_t i = name_index; i < len; i++) {
        if (s[i] == '|' || s[i] == '\n') {
          name_length = i - name_index;
          break;
        }
      }
      out->name = base::StringView(s + name_index, name_length);
      size_t value_index = name_index + name_length + 1;
      char value_str[32];
      strcpy(value_str, s + value_index);
      out->value = std::stod(value_str);
      return true;
    }
    default:
      return false;
  }
}

using protozero::ProtoDecoder;
using protozero::proto_utils::kFieldTypeLengthDelimited;

ProtoTraceParser::ProtoTraceParser(TraceProcessorContext* context)
    : context_(context),
      cpu_freq_name_id_(context->storage->InternString("cpufreq")),
      num_forks_name_id_(context->storage->InternString("num_forks")),
      num_irq_total_name_id_(context->storage->InternString("num_irq_total")),
      num_softirq_total_name_id_(
          context->storage->InternString("num_softirq_total")),
      num_irq_name_id_(context->storage->InternString("num_irq")),
      num_softirq_name_id_(context->storage->InternString("num_softirq")),
      cpu_times_user_ns_id_(
          context->storage->InternString("cpu.times.user_ns")),
      cpu_times_user_ice_ns_id_(
          context->storage->InternString("cpu.times.user_ice_ns")),
      cpu_times_system_mode_ns_id_(
          context->storage->InternString("cpu.times.system_mode_ns")),
      cpu_times_idle_ns_id_(
          context->storage->InternString("cpu.times.idle_ns")),
      cpu_times_io_wait_ns_id_(
          context->storage->InternString("cpu.times.io_wait_ns")),
      cpu_times_irq_ns_id_(context->storage->InternString("cpu.times.irq_ns")),
      cpu_times_softirq_ns_id_(
          context->storage->InternString("cpu.times.softirq_ns")),
      ion_heap_grow_id_(context->storage->InternString("ion_heap_grow")),
      ion_heap_shrink_id_(context->storage->InternString("ion_heap_shrink")),
      signal_deliver_id_(context->storage->InternString("signal_deliver")),
      signal_generate_id_(context->storage->InternString("signal_generate")) {
  for (const auto& name : BuildMeminfoCounterNames()) {
    meminfo_strs_id_.emplace_back(context->storage->InternString(name));
  }
  for (const auto& name : BuildVmstatCounterNames()) {
    vmstat_strs_id_.emplace_back(context->storage->InternString(name));
  }
  rss_members_.emplace_back(
      context->storage->InternString("rss_stat.mm_filepages"));
  rss_members_.emplace_back(
      context->storage->InternString("rss_stat.mm_anonpages"));
  rss_members_.emplace_back(
      context->storage->InternString("rss_stat.mm_swapents"));
  rss_members_.emplace_back(
      context->storage->InternString("rss_stat.nr_mm_counters"));

  using MemCounters = protos::ProcessStats::MemCounters;
  proc_mem_counter_names_[MemCounters::kVmSizeKbFieldNumber] =
      context->storage->InternString("mem.virt");
  proc_mem_counter_names_[MemCounters::kVmRssKbFieldNumber] =
      context->storage->InternString("mem.rss");
  proc_mem_counter_names_[MemCounters::kRssAnonKbFieldNumber] =
      context->storage->InternString("mem.rss.anon");
  proc_mem_counter_names_[MemCounters::kRssFileKbFieldNumber] =
      context->storage->InternString("mem.rss.file");
  proc_mem_counter_names_[MemCounters::kRssShmemKbFieldNumber] =
      context->storage->InternString("mem.rss.shmem");
  proc_mem_counter_names_[MemCounters::kVmSwapKbFieldNumber] =
      context->storage->InternString("mem.swap");
  proc_mem_counter_names_[MemCounters::kVmLockedKbFieldNumber] =
      context->storage->InternString("mem.locked");
  proc_mem_counter_names_[MemCounters::kVmHwmKbFieldNumber] =
      context->storage->InternString("mem.rss.watermark");
}

ProtoTraceParser::~ProtoTraceParser() = default;

void ProtoTraceParser::ParseTracePacket(uint64_t ts, TraceBlobView packet) {
  ProtoDecoder decoder(packet.data(), packet.length());

  for (auto fld = decoder.ReadField(); fld.id != 0; fld = decoder.ReadField()) {
    switch (fld.id) {
      case protos::TracePacket::kProcessTreeFieldNumber: {
        const size_t fld_off = packet.offset_of(fld.data());
        ParseProcessTree(packet.slice(fld_off, fld.size()));
        break;
      }
      case protos::TracePacket::kProcessStatsFieldNumber: {
        const size_t fld_off = packet.offset_of(fld.data());
        ParseProcessStats(ts, packet.slice(fld_off, fld.size()));
        break;
      }
      case protos::TracePacket::kSysStatsFieldNumber: {
        const size_t fld_off = packet.offset_of(fld.data());
        ParseSysStats(ts, packet.slice(fld_off, fld.size()));
        break;
      }
      default:
        break;
    }
  }
  PERFETTO_DCHECK(decoder.IsEndOfBuffer());
}

void ProtoTraceParser::ParseSysStats(uint64_t ts, TraceBlobView stats) {
  ProtoDecoder decoder(stats.data(), stats.length());
  for (auto fld = decoder.ReadField(); fld.id != 0; fld = decoder.ReadField()) {
    switch (fld.id) {
      case protos::SysStats::kMeminfoFieldNumber: {
        const size_t fld_off = stats.offset_of(fld.data());
        ParseMemInfo(ts, stats.slice(fld_off, fld.size()));
        break;
      }
      case protos::SysStats::kVmstatFieldNumber: {
        const size_t fld_off = stats.offset_of(fld.data());
        ParseVmStat(ts, stats.slice(fld_off, fld.size()));
        break;
      }
      case protos::SysStats::kCpuStatFieldNumber: {
        const size_t fld_off = stats.offset_of(fld.data());
        ParseCpuTimes(ts, stats.slice(fld_off, fld.size()));
        break;
      }
      case protos::SysStats::kNumIrqFieldNumber: {
        const size_t fld_off = stats.offset_of(fld.data());
        ParseIrqCount(ts, stats.slice(fld_off, fld.size()),
                      /*is_softirq=*/false);
        break;
      }
      case protos::SysStats::kNumSoftirqFieldNumber: {
        const size_t fld_off = stats.offset_of(fld.data());
        ParseIrqCount(ts, stats.slice(fld_off, fld.size()),
                      /*is_softirq=*/true);
        break;
      }
      case protos::SysStats::kNumForksFieldNumber: {
        context_->event_tracker->PushCounter(
            ts, fld.as_uint32(), num_forks_name_id_, 0, RefType::kRefNoRef);
        break;
      }
      case protos::SysStats::kNumIrqTotalFieldNumber: {
        context_->event_tracker->PushCounter(
            ts, fld.as_uint32(), num_irq_total_name_id_, 0, RefType::kRefNoRef);
        break;
      }
      case protos::SysStats::kNumSoftirqTotalFieldNumber: {
        context_->event_tracker->PushCounter(ts, fld.as_uint32(),
                                             num_softirq_total_name_id_, 0,
                                             RefType::kRefNoRef);
        break;
      }
      default:
        break;
    }
  }
}
void ProtoTraceParser::ParseIrqCount(uint64_t ts,
                                     TraceBlobView irq,
                                     bool is_soft) {
  ProtoDecoder decoder(irq.data(), irq.length());
  uint32_t key = 0;
  uint32_t value = 0;
  for (auto fld = decoder.ReadField(); fld.id != 0; fld = decoder.ReadField()) {
    switch (fld.id) {
      case protos::SysStats::InterruptCount::kIrqFieldNumber:
        key = fld.as_uint32();
        break;
      case protos::SysStats::InterruptCount::kCountFieldNumber:
        value = fld.as_uint32();
        break;
    }
  }
  RefType ref_type = is_soft ? RefType::kRefIrq : RefType::kRefSoftIrq;
  StringId name_id = is_soft ? num_irq_name_id_ : num_softirq_name_id_;
  context_->event_tracker->PushCounter(ts, value, name_id, key, ref_type);
}

void ProtoTraceParser::ParseMemInfo(uint64_t ts, TraceBlobView mem) {
  ProtoDecoder decoder(mem.data(), mem.length());
  uint32_t key = 0;
  uint32_t value = 0;
  for (auto fld = decoder.ReadField(); fld.id != 0; fld = decoder.ReadField()) {
    switch (fld.id) {
      case protos::SysStats::MeminfoValue::kKeyFieldNumber:
        key = fld.as_uint32();
        break;
      case protos::SysStats::MeminfoValue::kValueFieldNumber:
        value = fld.as_uint32();
        break;
    }
  }
  if (PERFETTO_UNLIKELY(key >= meminfo_strs_id_.size())) {
    PERFETTO_ELOG("MemInfo key %d is not recognized.", key);
    return;
  }
  context_->event_tracker->PushCounter(ts, value, meminfo_strs_id_[key], 0,
                                       RefType::kRefNoRef);
}

void ProtoTraceParser::ParseVmStat(uint64_t ts, TraceBlobView stat) {
  ProtoDecoder decoder(stat.data(), stat.length());
  uint32_t key = 0;
  uint32_t value = 0;
  for (auto fld = decoder.ReadField(); fld.id != 0; fld = decoder.ReadField()) {
    switch (fld.id) {
      case protos::SysStats::VmstatValue::kKeyFieldNumber:
        key = fld.as_uint32();
        break;
      case protos::SysStats::VmstatValue::kValueFieldNumber:
        value = fld.as_uint32();
        break;
    }
  }
  if (PERFETTO_UNLIKELY(key >= vmstat_strs_id_.size())) {
    PERFETTO_ELOG("VmStat key %d is not recognized.", key);
    return;
  }
  context_->event_tracker->PushCounter(ts, value, vmstat_strs_id_[key], 0,
                                       RefType::kRefNoRef);
}

void ProtoTraceParser::ParseCpuTimes(uint64_t ts, TraceBlobView cpu_times) {
  ProtoDecoder decoder(cpu_times.data(), cpu_times.length());
  uint64_t cpu = 0;
  uint32_t value = 0;
  // Speculate on CPU being first.
  constexpr auto kCpuFieldTag = protozero::proto_utils::MakeTagVarInt(
      protos::SysStats::CpuTimes::kCpuIdFieldNumber);
  if (cpu_times.length() > 2 && cpu_times.data()[0] == kCpuFieldTag &&
      cpu_times.data()[1] < 0x80) {
    cpu = cpu_times.data()[1];
  } else {
    if (!PERFETTO_LIKELY((
            decoder.FindIntField<protos::SysStats::CpuTimes::kCpuIdFieldNumber>(
                &cpu)))) {
      PERFETTO_ELOG("CPU field not found in CpuTimes");
      return;
    }
  }

  for (auto fld = decoder.ReadField(); fld.id != 0; fld = decoder.ReadField()) {
    switch (fld.id) {
      case protos::SysStats::CpuTimes::kUserNsFieldNumber: {
        value = fld.as_uint32();
        context_->event_tracker->PushCounter(ts, value, cpu_times_user_ns_id_,
                                             cpu, RefType::kRefCpuId);
        break;
      }
      case protos::SysStats::CpuTimes::kUserIceNsFieldNumber: {
        value = fld.as_uint32();
        context_->event_tracker->PushCounter(
            ts, value, cpu_times_user_ice_ns_id_, cpu, RefType::kRefCpuId);
        break;
      }
      case protos::SysStats::CpuTimes::kSystemModeNsFieldNumber: {
        value = fld.as_uint32();
        context_->event_tracker->PushCounter(
            ts, value, cpu_times_system_mode_ns_id_, cpu, RefType::kRefCpuId);
        break;
      }
      case protos::SysStats::CpuTimes::kIdleNsFieldNumber: {
        value = fld.as_uint32();
        context_->event_tracker->PushCounter(ts, value, cpu_times_idle_ns_id_,
                                             cpu, RefType::kRefCpuId);
        break;
      }
      case protos::SysStats::CpuTimes::kIoWaitNsFieldNumber: {
        value = fld.as_uint32();
        context_->event_tracker->PushCounter(
            ts, value, cpu_times_io_wait_ns_id_, cpu, RefType::kRefCpuId);
        break;
      }
      case protos::SysStats::CpuTimes::kIrqNsFieldNumber: {
        value = fld.as_uint32();
        context_->event_tracker->PushCounter(ts, value, cpu_times_irq_ns_id_,
                                             cpu, RefType::kRefCpuId);
        break;
      }
      case protos::SysStats::CpuTimes::kSoftirqNsFieldNumber: {
        value = fld.as_uint32();
        context_->event_tracker->PushCounter(
            ts, value, cpu_times_softirq_ns_id_, cpu, RefType::kRefCpuId);
        break;
      }
      default:
        break;
    }
  }
}

void ProtoTraceParser::ParseProcessTree(TraceBlobView pstree) {
  ProtoDecoder decoder(pstree.data(), pstree.length());

  for (auto fld = decoder.ReadField(); fld.id != 0; fld = decoder.ReadField()) {
    const size_t fld_off = pstree.offset_of(fld.data());
    switch (fld.id) {
      case protos::ProcessTree::kProcessesFieldNumber: {
        ParseProcess(pstree.slice(fld_off, fld.size()));
        break;
      }
      case protos::ProcessTree::kThreadsFieldNumber: {
        ParseThread(pstree.slice(fld_off, fld.size()));
        break;
      }
      default:
        break;
    }
  }
  PERFETTO_DCHECK(decoder.IsEndOfBuffer());
}

void ProtoTraceParser::ParseProcessStats(uint64_t ts, TraceBlobView stats) {
  ProtoDecoder decoder(stats.data(), stats.length());

  for (auto fld = decoder.ReadField(); fld.id != 0; fld = decoder.ReadField()) {
    const size_t fld_off = stats.offset_of(fld.data());
    switch (fld.id) {
      case protos::ProcessStats::kMemCountersFieldNumber: {
        ParseProcMemCounters(ts, stats.slice(fld_off, fld.size()));
        break;
      }
      default:
        break;
    }
  }
  PERFETTO_DCHECK(decoder.IsEndOfBuffer());
}

void ProtoTraceParser::ParseProcMemCounters(uint64_t ts,
                                            TraceBlobView proc_stat) {
  ProtoDecoder decoder(proc_stat.data(), proc_stat.length());
  uint32_t pid = 0;
  // Maps a process counter field it to its value.
  // E.g., 4 := 1024 -> "mem.rss.anon" := 1024.
  std::array<uint64_t, kProcMemCounterSize> counter_values{};
  std::array<uint8_t, kProcMemCounterSize> has_counter{};

  for (auto fld = decoder.ReadField(); fld.id != 0; fld = decoder.ReadField()) {
    switch (fld.id) {
      case protos::ProcessStats::MemCounters::kPidFieldNumber:
        pid = fld.as_uint32();
        break;
      default:
        if (fld.id < counter_values.size()) {
          // Memory counters are in KB, keep values in bytes in the trace
          // processor.
          counter_values[fld.id] = fld.as_uint64() * 1024;
          has_counter[fld.id] = 1;
        } else {
          PERFETTO_ELOG("Skipping unknown process counters %" PRIu32, fld.id);
        }
    }
  }

  UniqueTid utid = context_->process_tracker->UpdateThread(ts, pid, 0);

  // Skip field_id 0 (invalid) and 1 (pid).
  for (size_t field_id = 2; field_id < counter_values.size(); field_id++) {
    if (!has_counter[field_id])
      continue;

    // Lookup the interned string id from the field name using the
    // pre-cached |proc_mem_counter_names_| map.
    StringId name = proc_mem_counter_names_[field_id];
    uint64_t value = counter_values[field_id];
    context_->event_tracker->PushCounter(ts, value, name, utid,
                                         RefType::kRefUtidLookupUpid);
  }

  PERFETTO_DCHECK(decoder.IsEndOfBuffer());
}

void ProtoTraceParser::ParseThread(TraceBlobView thread) {
  ProtoDecoder decoder(thread.data(), thread.length());
  uint32_t tid = 0;
  uint32_t tgid = 0;
  for (auto fld = decoder.ReadField(); fld.id != 0; fld = decoder.ReadField()) {
    switch (fld.id) {
      case protos::ProcessTree::Thread::kTidFieldNumber:
        tid = fld.as_uint32();
        break;
      case protos::ProcessTree::Thread::kTgidFieldNumber:
        tgid = fld.as_uint32();
        break;
      default:
        break;
    }
  }
  context_->process_tracker->UpdateThread(tid, tgid);

  PERFETTO_DCHECK(decoder.IsEndOfBuffer());
}

void ProtoTraceParser::ParseProcess(TraceBlobView process) {
  ProtoDecoder decoder(process.data(), process.length());

  uint32_t pid = 0;
  base::StringView process_name;

  for (auto fld = decoder.ReadField(); fld.id != 0; fld = decoder.ReadField()) {
    switch (fld.id) {
      case protos::ProcessTree::Process::kPidFieldNumber:
        pid = fld.as_uint32();
        break;
      case protos::ProcessTree::Process::kCmdlineFieldNumber:
        if (process_name.empty())
          process_name = fld.as_string();
        break;
      default:
        break;
    }
  }
  context_->process_tracker->UpdateProcess(pid, process_name);
  PERFETTO_DCHECK(decoder.IsEndOfBuffer());
}

void ProtoTraceParser::ParseFtracePacket(uint32_t cpu,
                                         uint64_t timestamp,
                                         TraceBlobView ftrace) {
  ProtoDecoder decoder(ftrace.data(), ftrace.length());
  uint32_t pid = 0;
  for (auto fld = decoder.ReadField(); fld.id != 0; fld = decoder.ReadField()) {
    switch (fld.id) {
      case protos::FtraceEvent::kPidFieldNumber: {
        pid = fld.as_uint32();
        break;
      }
      case protos::FtraceEvent::kSchedSwitchFieldNumber: {
        PERFETTO_DCHECK(timestamp > 0);
        const size_t fld_off = ftrace.offset_of(fld.data());
        ParseSchedSwitch(cpu, timestamp, ftrace.slice(fld_off, fld.size()));
        break;
      }
      case protos::FtraceEvent::kCpuFrequency: {
        PERFETTO_DCHECK(timestamp > 0);
        const size_t fld_off = ftrace.offset_of(fld.data());
        ParseCpuFreq(timestamp, ftrace.slice(fld_off, fld.size()));
        break;
      }
      case protos::FtraceEvent::kPrintFieldNumber: {
        PERFETTO_DCHECK(timestamp > 0);
        const size_t fld_off = ftrace.offset_of(fld.data());
        ParsePrint(cpu, timestamp, pid, ftrace.slice(fld_off, fld.size()));
        break;
      }
      case protos::FtraceEvent::kRssStatFieldNumber: {
        PERFETTO_DCHECK(timestamp > 0);
        const size_t fld_off = ftrace.offset_of(fld.data());
        ParseRssStat(timestamp, pid, ftrace.slice(fld_off, fld.size()));
        break;
      }
      case protos::FtraceEvent::kIonHeapGrow: {
        PERFETTO_DCHECK(timestamp > 0);
        const size_t fld_off = ftrace.offset_of(fld.data());
        ParseIonHeapGrow(timestamp, pid, ftrace.slice(fld_off, fld.size()));
        break;
      }
      case protos::FtraceEvent::kIonHeapShrink: {
        PERFETTO_DCHECK(timestamp > 0);
        const size_t fld_off = ftrace.offset_of(fld.data());
        ParseIonHeapShrink(timestamp, pid, ftrace.slice(fld_off, fld.size()));
        break;
      }
      case protos::FtraceEvent::kSignalGenerate: {
        PERFETTO_DCHECK(timestamp > 0);
        const size_t fld_off = ftrace.offset_of(fld.data());
        ParseSignalGenerate(timestamp, ftrace.slice(fld_off, fld.size()));
        break;
      }
      case protos::FtraceEvent::kSignalDeliver: {
        PERFETTO_DCHECK(timestamp > 0);
        const size_t fld_off = ftrace.offset_of(fld.data());
        ParseSignalDeliver(timestamp, pid, ftrace.slice(fld_off, fld.size()));
        break;
      }

      default:
        break;
    }
  }
  PERFETTO_DCHECK(decoder.IsEndOfBuffer());
}

void ProtoTraceParser::ParseSignalDeliver(uint64_t timestamp,
                                          uint32_t pid,
                                          TraceBlobView view) {
  ProtoDecoder decoder(view.data(), view.length());
  uint32_t sig = 0;
  for (auto fld = decoder.ReadField(); fld.id != 0; fld = decoder.ReadField()) {
    switch (fld.id) {
      case protos::SignalDeliverFtraceEvent::kSigFieldNumber:
        sig = fld.as_uint32();
        break;
    }
  }
  auto* instants = context_->storage->mutable_instants();
  UniqueTid utid = context_->process_tracker->UpdateThread(timestamp, pid, 0);
  instants->AddInstantEvent(timestamp, signal_deliver_id_, sig, utid,
                            RefType::kRefUtid);
}

// This event has both the pid of the thread that sent the signal and the
// destination of the signal. Currently storing the pid of the destination.
void ProtoTraceParser::ParseSignalGenerate(uint64_t timestamp,
                                           TraceBlobView view) {
  ProtoDecoder decoder(view.data(), view.length());
  uint32_t pid = 0;
  uint32_t sig = 0;
  for (auto fld = decoder.ReadField(); fld.id != 0; fld = decoder.ReadField()) {
    switch (fld.id) {
      case protos::SignalGenerateFtraceEvent::kPidFieldNumber:
        pid = fld.as_uint32();
        break;
      case protos::SignalGenerateFtraceEvent::kSigFieldNumber:
        sig = fld.as_uint32();
        break;
    }
  }
  auto* instants = context_->storage->mutable_instants();
  UniqueTid utid = context_->process_tracker->UpdateThread(timestamp, pid, 0);
  instants->AddInstantEvent(timestamp, signal_generate_id_, sig, utid,
                            RefType::kRefUtid);
}

void ProtoTraceParser::ParseLowmemoryKill(uint64_t timestamp,
                                          TraceBlobView view) {
  // TODO(taylori): Store the pagecache_size, pagecache_limit and free fields
  // in an args table
  ProtoDecoder decoder(view.data(), view.length());
  uint32_t pid = 0;
  base::StringView comm;
  for (auto fld = decoder.ReadField(); fld.id != 0; fld = decoder.ReadField()) {
    switch (fld.id) {
      case protos::LowmemoryKillFtraceEvent::kPidFieldNumber:
        pid = fld.as_uint32();
        break;
      case protos::LowmemoryKillFtraceEvent::kCommFieldNumber:
        comm = fld.as_string();
        break;
    }
  }
  // TODO(taylori): Move the comm to the args table once it exists.
  StringId name = context_->storage->InternString(
      base::StringView("mem.lmk." + comm.ToStdString()));
  auto* instants = context_->storage->mutable_instants();
  // Storing the pid of the event that is lmk-ed.
  UniqueTid utid = context_->process_tracker->UpdateThread(timestamp, pid, 0);
  instants->AddInstantEvent(timestamp, 0, name, utid, RefType::kRefUtid);
}

void ProtoTraceParser::ParseRssStat(uint64_t timestamp,
                                    uint32_t pid,
                                    TraceBlobView view) {
  ProtoDecoder decoder(view.data(), view.length());
  uint32_t member = 0;
  uint32_t size = 0;
  for (auto fld = decoder.ReadField(); fld.id != 0; fld = decoder.ReadField()) {
    switch (fld.id) {
      case protos::RssStatFtraceEvent::kMemberFieldNumber:
        member = fld.as_uint32();
        break;
      case protos::RssStatFtraceEvent::kSizeFieldNumber:
        size = fld.as_uint32();
        break;
    }
  }
  if (member >= rss_members_.size()) {
    PERFETTO_ELOG("Unknown member field %" PRIu32 " in rss_stat event", member);
    return;
  }
  UniqueTid utid = context_->process_tracker->UpdateThread(timestamp, pid, 0);

  context_->event_tracker->PushCounter(timestamp, size, rss_members_[member],
                                       utid, RefType::kRefUtidLookupUpid);
  PERFETTO_DCHECK(decoder.IsEndOfBuffer());
}

void ProtoTraceParser::ParseIonHeapGrow(uint64_t timestamp,
                                        uint32_t pid,
                                        TraceBlobView view) {
  ProtoDecoder decoder(view.data(), view.length());
  uint32_t value = 0;
  // TODO(b/118300811): The heap name pointer cannot be read. Read once it
  // has been fixed.
  for (auto fld = decoder.ReadField(); fld.id != 0; fld = decoder.ReadField()) {
    switch (fld.id) {
      case protos::IonHeapGrowFtraceEvent::kTotalAllocatedFieldNumber:
        value = fld.as_uint32();
        break;
    }
  }
  UniqueTid utid = context_->process_tracker->UpdateThread(timestamp, pid, 0);
  context_->event_tracker->PushCounter(timestamp, value, ion_heap_grow_id_,
                                       utid, RefType::kRefUtid);
  PERFETTO_DCHECK(decoder.IsEndOfBuffer());
}

void ProtoTraceParser::ParseIonHeapShrink(uint64_t timestamp,
                                          uint32_t pid,
                                          TraceBlobView view) {
  ProtoDecoder decoder(view.data(), view.length());
  uint32_t value = 0;
  // TODO(b/118300811): The heap name pointer cannot be read. Read once it
  // has been fixed.
  for (auto fld = decoder.ReadField(); fld.id != 0; fld = decoder.ReadField()) {
    switch (fld.id) {
      case protos::IonHeapShrinkFtraceEvent::kTotalAllocatedFieldNumber:
        value = fld.as_uint32();
        break;
    }
  }
  UniqueTid utid = context_->process_tracker->UpdateThread(timestamp, pid, 0);
  context_->event_tracker->PushCounter(timestamp, value, ion_heap_shrink_id_,
                                       utid, RefType::kRefUtid);
  PERFETTO_DCHECK(decoder.IsEndOfBuffer());
}

void ProtoTraceParser::ParseCpuFreq(uint64_t timestamp, TraceBlobView view) {
  ProtoDecoder decoder(view.data(), view.length());

  uint32_t cpu_affected = 0;
  uint32_t new_freq = 0;
  for (auto fld = decoder.ReadField(); fld.id != 0; fld = decoder.ReadField()) {
    switch (fld.id) {
      case protos::CpuFrequencyFtraceEvent::kCpuIdFieldNumber:
        cpu_affected = fld.as_uint32();
        break;
      case protos::CpuFrequencyFtraceEvent::kStateFieldNumber:
        new_freq = fld.as_uint32();
        break;
    }
  }
  context_->event_tracker->PushCounter(timestamp, new_freq, cpu_freq_name_id_,
                                       cpu_affected, RefType::kRefCpuId);
  PERFETTO_DCHECK(decoder.IsEndOfBuffer());
}

void ProtoTraceParser::ParseSchedSwitch(uint32_t cpu,
                                        uint64_t timestamp,
                                        TraceBlobView sswitch) {
  ProtoDecoder decoder(sswitch.data(), sswitch.length());

  uint32_t prev_pid = 0;
  uint32_t prev_state = 0;
  base::StringView next_comm;
  uint32_t next_pid = 0;
  for (auto fld = decoder.ReadField(); fld.id != 0; fld = decoder.ReadField()) {
    switch (fld.id) {
      case protos::SchedSwitchFtraceEvent::kPrevPidFieldNumber:
        prev_pid = fld.as_uint32();
        break;
      case protos::SchedSwitchFtraceEvent::kPrevStateFieldNumber:
        prev_state = fld.as_uint32();
        break;
      case protos::SchedSwitchFtraceEvent::kNextPidFieldNumber:
        next_pid = fld.as_uint32();
        break;
      case protos::SchedSwitchFtraceEvent::kNextCommFieldNumber:
        next_comm = fld.as_string();
        break;
      default:
        break;
    }
  }
  context_->event_tracker->PushSchedSwitch(cpu, timestamp, prev_pid, prev_state,
                                           next_pid, next_comm);
  PERFETTO_DCHECK(decoder.IsEndOfBuffer());
}

void ProtoTraceParser::ParsePrint(uint32_t,
                                  uint64_t timestamp,
                                  uint32_t pid,
                                  TraceBlobView print) {
  ProtoDecoder decoder(print.data(), print.length());

  base::StringView buf{};
  for (auto fld = decoder.ReadField(); fld.id != 0; fld = decoder.ReadField()) {
    if (fld.id == protos::PrintFtraceEvent::kBufFieldNumber) {
      buf = fld.as_string();
      break;
    }
  }

  SystraceTracePoint point{};
  if (!ParseSystraceTracePoint(buf, &point))
    return;

  switch (point.phase) {
    case 'B': {
      StringId name_id = context_->storage->InternString(point.name);
      context_->slice_tracker->BeginAndroid(timestamp, pid, point.tid,
                                            0 /*cat_id*/, name_id);
      break;
    }

    case 'E': {
      context_->slice_tracker->EndAndroid(timestamp, pid, point.tid);
      break;
    }

    case 'C': {
      UniqueTid utid =
          context_->process_tracker->UpdateThread(timestamp, point.tid, 0);
      StringId name_id = context_->storage->InternString(point.name);
      context_->event_tracker->PushCounter(timestamp, point.value, name_id,
                                           utid, RefType::kRefUtid);
    }
  }
  PERFETTO_DCHECK(decoder.IsEndOfBuffer());
}

}  // namespace trace_processor
}  // namespace perfetto
