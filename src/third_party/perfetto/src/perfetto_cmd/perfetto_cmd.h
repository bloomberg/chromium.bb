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

#ifndef SRC_PERFETTO_CMD_PERFETTO_CMD_H_
#define SRC_PERFETTO_CMD_PERFETTO_CMD_H_

#include <memory>
#include <string>
#include <vector>

#include <time.h>

#include "perfetto/base/build_config.h"
#include "perfetto/ext/base/event_fd.h"
#include "perfetto/ext/base/optional.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/unix_task_runner.h"
#include "perfetto/ext/tracing/core/consumer.h"
#include "perfetto/ext/tracing/ipc/consumer_ipc_client.h"
#include "src/perfetto_cmd/rate_limiter.h"

#include "src/perfetto_cmd/perfetto_cmd_state.pb.h"

#if PERFETTO_BUILDFLAG(PERFETTO_ANDROID_BUILD)
#include "perfetto/ext/base/android_task_runner.h"
#endif  // PERFETTO_BUILDFLAG(PERFETTO_ANDROID_BUILD)

namespace perfetto {

class PacketWriter;

// Temporary directory for DropBox traces. Note that this is automatically
// created by the system by setting setprop persist.traced.enable=1.
extern const char* kTempDropBoxTraceDir;

#if PERFETTO_BUILDFLAG(PERFETTO_ANDROID_BUILD)
using PlatformTaskRunner = base::AndroidTaskRunner;
#else
using PlatformTaskRunner = base::UnixTaskRunner;
#endif

class PerfettoCmd : public Consumer {
 public:
  int Main(int argc, char** argv);

  // perfetto::Consumer implementation.
  void OnConnect() override;
  void OnDisconnect() override;
  void OnTracingDisabled() override;
  void OnTraceData(std::vector<TracePacket>, bool has_more) override;
  void OnDetach(bool) override;
  void OnAttach(bool, const TraceConfig&) override;
  void OnTraceStats(bool, const TraceStats&) override;
  void OnObservableEvents(const ObservableEvents&) override;

  void SignalCtrlC() { ctrl_c_evt_.Notify(); }

 private:
  bool OpenOutputFile();
  void SetupCtrlCSignalHandler();
  void FinalizeTraceAndExit();
  int PrintUsage(const char* argv0);
  void PrintServiceState(bool success, const TracingServiceState&);
  void OnTimeout();
  bool is_detach() const { return !detach_key_.empty(); }
  bool is_attach() const { return !attach_key_.empty(); }
  void SaveOutputToDropboxOrCrash();
  void SaveOutputToIncidentTraceOrCrash();

  PlatformTaskRunner task_runner_;
  std::unique_ptr<perfetto::TracingService::ConsumerEndpoint>
      consumer_endpoint_;
  std::unique_ptr<TraceConfig> trace_config_;

  std::unique_ptr<PacketWriter> packet_writer_;
  base::ScopedFstream trace_out_stream_;

  std::string trace_out_path_;
  base::EventFd ctrl_c_evt_;
  std::string dropbox_tag_;
  bool did_process_full_trace_ = false;
  uint64_t bytes_written_ = 0;
  std::string detach_key_;
  std::string attach_key_;
  bool stop_trace_once_attached_ = false;
  bool redetach_once_attached_ = false;
  bool query_service_ = false;
  bool query_service_output_raw_ = false;
};

}  // namespace perfetto

#endif  // SRC_PERFETTO_CMD_PERFETTO_CMD_H_
