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

#ifndef SRC_TRACED_PROBES_PS_PROCESS_STATS_DATA_SOURCE_H_
#define SRC_TRACED_PROBES_PS_PROCESS_STATS_DATA_SOURCE_H_

#include <memory>
#include <set>
#include <vector>

#include "perfetto/base/weak_ptr.h"
#include "perfetto/tracing/core/basic_types.h"
#include "perfetto/tracing/core/data_source_config.h"
#include "perfetto/tracing/core/trace_writer.h"
#include "src/traced/probes/probes_data_source.h"

namespace perfetto {

namespace protos {
namespace pbzero {
class ProcessTree;
}  // namespace pbzero
}  // namespace protos

class ProcessStatsDataSource : public ProbesDataSource {
 public:
  static constexpr int kTypeId = 3;

  ProcessStatsDataSource(TracingSessionID,
                         std::unique_ptr<TraceWriter> writer,
                         const DataSourceConfig&);
  ~ProcessStatsDataSource() override;

  base::WeakPtr<ProcessStatsDataSource> GetWeakPtr() const;
  void WriteAllProcesses();
  void OnPids(const std::vector<int32_t>& pids);

  // Virtual for testing.
  virtual std::string ReadProcPidFile(int32_t pid, const std::string& file);

  // ProbesDataSource implementation.
  void Start() override;
  void Flush() override;

 private:
  ProcessStatsDataSource(const ProcessStatsDataSource&) = delete;
  ProcessStatsDataSource& operator=(const ProcessStatsDataSource&) = delete;

  void WriteProcess(int32_t pid, const std::string& proc_status);
  void WriteThread(int32_t tid, int32_t tgid, const std::string& proc_status);
  void WriteProcessOrThread(int32_t pid);
  std::string ReadProcStatusEntry(const std::string& buf, const char* key);

  protos::pbzero::ProcessTree* GetOrCreatePsTree();
  void FinalizeCurPsTree();

  std::unique_ptr<TraceWriter> writer_;
  TraceWriter::TracePacketHandle cur_packet_;
  protos::pbzero::ProcessTree* cur_ps_tree_ = nullptr;
  bool record_thread_names_ = false;
  bool enable_on_demand_dumps_ = true;
  bool dump_all_procs_on_start_ = false;

  // This set contains PIDs as per the Linux kernel notion of a PID (which is
  // really a TID). In practice this set will contain all TIDs for all processes
  // seen, not just the main thread id (aka thread group ID).
  // TODO(b/76663469): Optimization: use a bitmap.
  std::set<int32_t> seen_pids_;

  base::WeakPtrFactory<ProcessStatsDataSource> weak_factory_;  // Keep last.
};

}  // namespace perfetto

#endif  // SRC_TRACED_PROBES_PS_PROCESS_STATS_DATA_SOURCE_H_
