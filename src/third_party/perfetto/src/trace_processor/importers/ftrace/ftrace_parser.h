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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_FTRACE_FTRACE_PARSER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_FTRACE_FTRACE_PARSER_H_

#include "perfetto/trace_processor/status.h"
#include "src/trace_processor/event_tracker.h"
#include "src/trace_processor/importers/ftrace/ftrace_descriptors.h"
#include "src/trace_processor/importers/ftrace/sched_event_tracker.h"
#include "src/trace_processor/timestamped_trace_piece.h"
#include "src/trace_processor/trace_blob_view.h"
#include "src/trace_processor/trace_processor_context.h"

namespace perfetto {
namespace trace_processor {

class FtraceParser {
 public:
  explicit FtraceParser(TraceProcessorContext* context);

  void ParseFtraceStats(protozero::ConstBytes);

  util::Status ParseFtraceEvent(uint32_t cpu, const TimestampedTracePiece& ttp);

 private:
  void ParseGenericFtrace(int64_t timestamp,
                          uint32_t cpu,
                          uint32_t pid,
                          protozero::ConstBytes);
  void ParseTypedFtraceToRaw(uint32_t ftrace_id,
                             int64_t timestamp,
                             uint32_t cpu,
                             uint32_t pid,
                             protozero::ConstBytes);
  void ParseSchedSwitch(uint32_t cpu, int64_t timestamp, protozero::ConstBytes);
  void ParseSchedWakeup(int64_t timestamp, protozero::ConstBytes);
  void ParseSchedWaking(int64_t timestamp, protozero::ConstBytes);
  void ParseSchedProcessFree(int64_t timestamp, protozero::ConstBytes);
  void ParseCpuFreq(int64_t timestamp, protozero::ConstBytes);
  void ParseGpuFreq(int64_t timestamp, protozero::ConstBytes);
  void ParseCpuIdle(int64_t timestamp, protozero::ConstBytes);
  void ParsePrint(int64_t timestamp, uint32_t pid, protozero::ConstBytes);
  void ParseZero(int64_t timestamp, uint32_t pid, protozero::ConstBytes);
  void ParseSdeTracingMarkWrite(int64_t timestamp,
                                uint32_t pid,
                                protozero::ConstBytes);
  void ParseRssStat(int64_t ts, uint32_t pid, protozero::ConstBytes);
  void ParseIonHeapGrowOrShrink(int64_t ts,
                                uint32_t pid,
                                protozero::ConstBytes,
                                bool grow);
  void ParseSignalGenerate(int64_t ts, protozero::ConstBytes);
  void ParseSignalDeliver(int64_t ts, uint32_t pid, protozero::ConstBytes);
  void ParseLowmemoryKill(int64_t ts, protozero::ConstBytes);
  void ParseOOMScoreAdjUpdate(int64_t ts, protozero::ConstBytes);
  void ParseMmEventRecord(int64_t ts, uint32_t pid, protozero::ConstBytes);
  void ParseSysEvent(int64_t ts,
                     uint32_t pid,
                     bool is_enter,
                     protozero::ConstBytes);
  void ParseTaskNewTask(int64_t timestamp,
                        uint32_t source_tid,
                        protozero::ConstBytes);
  void ParseTaskRename(protozero::ConstBytes);
  void ParseBinderTransaction(int64_t timestamp,
                              uint32_t pid,
                              protozero::ConstBytes);
  void ParseBinderTransactionReceived(int64_t timestamp,
                                      uint32_t pid,
                                      protozero::ConstBytes);
  void ParseBinderTransactionAllocBuf(int64_t timestamp,
                                      uint32_t pid,
                                      protozero::ConstBytes);
  void ParseBinderLocked(int64_t timestamp,
                         uint32_t pid,
                         protozero::ConstBytes);
  void ParseBinderLock(int64_t timestamp, uint32_t pid, protozero::ConstBytes);
  void ParseBinderUnlock(int64_t timestamp,
                         uint32_t pid,
                         protozero::ConstBytes);

  TraceProcessorContext* context_;

  const StringId sched_wakeup_name_id_;
  const StringId sched_waking_name_id_;
  const StringId cpu_freq_name_id_;
  const StringId gpu_freq_name_id_;
  const StringId cpu_idle_name_id_;
  const StringId ion_total_unknown_id_;
  const StringId ion_change_unknown_id_;
  const StringId signal_generate_id_;
  const StringId signal_deliver_id_;
  const StringId oom_score_adj_id_;
  const StringId lmk_id_;
  const StringId comm_name_id_;

  std::vector<StringId> rss_members_;

  struct FtraceMessageStrings {
    // The string id of name of the event field (e.g. sched_switch's id).
    StringId message_name_id = 0;
    std::array<StringId, kMaxFtraceEventFields> field_name_ids;
  };
  std::vector<FtraceMessageStrings> ftrace_message_strings_;

  struct MmEventCounterNames {
    MmEventCounterNames() = default;
    MmEventCounterNames(StringId _count, StringId _max_lat, StringId _avg_lat)
        : count(_count), max_lat(_max_lat), avg_lat(_avg_lat) {}

    StringId count = 0;
    StringId max_lat = 0;
    StringId avg_lat = 0;
  };

  // Keep kMmEventCounterSize equal to mm_event_type::MM_TYPE_NUM in the kernel.
  static constexpr size_t kMmEventCounterSize = 7;
  std::array<MmEventCounterNames, kMmEventCounterSize> mm_event_counter_names_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_FTRACE_FTRACE_PARSER_H_
