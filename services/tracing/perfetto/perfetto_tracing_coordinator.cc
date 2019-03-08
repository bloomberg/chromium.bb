// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/perfetto_tracing_coordinator.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "services/tracing/perfetto/chrome_event_bundle_json_exporter.h"
#include "services/tracing/perfetto/perfetto_service.h"
#include "services/tracing/public/cpp/trace_event_args_whitelist.h"
#include "services/tracing/public/mojom/constants.mojom.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/core/consumer.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_stats.h"
#include "third_party/perfetto/include/perfetto/tracing/core/tracing_service.h"

namespace tracing {

// A TracingSession acts as a perfetto consumer and is used to wrap all the
// associated state of an on-going tracing session, for easy setup and cleanup.
//
// TODO(eseckler): Make it possible to have multiple active sessions
// concurrently. Also support configuring the output format of a session (JSON
// vs. proto).
class PerfettoTracingCoordinator::TracingSession : public perfetto::Consumer {
 public:
  TracingSession(const base::trace_event::TraceConfig& chrome_config,
                 base::OnceClosure tracing_over_callback)
      : tracing_over_callback_(std::move(tracing_over_callback)) {
    // In legacy backend, the trace event agent sets the predicate used by
    // TraceLog. For perfetto backend, ensure that predicate is always set
    // before creating the exporter. The agent can be created later than this
    // point.
    if (base::trace_event::TraceLog::GetInstance()
            ->GetArgumentFilterPredicate()
            .is_null()) {
      base::trace_event::TraceLog::GetInstance()->SetArgumentFilterPredicate(
          base::BindRepeating(&IsTraceEventArgsWhitelisted));
      base::trace_event::TraceLog::GetInstance()->SetMetadataFilterPredicate(
          base::BindRepeating(&IsMetadataWhitelisted));
    }

    json_trace_exporter_ = std::make_unique<ChromeEventBundleJsonExporter>(
        chrome_config.IsArgumentFilterEnabled()
            ? base::trace_event::TraceLog::GetInstance()
                  ->GetArgumentFilterPredicate()
            : JSONTraceExporter::ArgumentFilterPredicate(),
        base::BindRepeating(&TracingSession::OnJSONTraceEventCallback,
                            base::Unretained(this)));
    perfetto::TracingService* service =
        PerfettoService::GetInstance()->GetService();
    consumer_endpoint_ = service->ConnectConsumer(this, /*uid=*/0);

    // Start tracing.
    auto perfetto_config = CreatePerfettoConfiguration(chrome_config);
    consumer_endpoint_->EnableTracing(perfetto_config);
  }

  ~TracingSession() override {
    if (!stop_and_flush_callback_.is_null()) {
      base::ResetAndReturn(&stop_and_flush_callback_)
          .Run(base::Value(base::Value::Type::DICTIONARY));
    }

    stream_.reset();
  }

  void ChangeTraceConfig(const base::trace_event::TraceConfig& chrome_config) {
    auto perfetto_config = CreatePerfettoConfiguration(chrome_config);
    consumer_endpoint_->ChangeTraceConfig(perfetto_config);
  }

  perfetto::TraceConfig CreatePerfettoConfiguration(
      const base::trace_event::TraceConfig& chrome_config) {
    perfetto::TraceConfig perfetto_config;
    size_t size_limit = chrome_config.GetTraceBufferSizeInKb();
    if (size_limit == 0) {
      size_limit = 100 * 1024;
    }
    perfetto_config.add_buffers()->set_size_kb(size_limit);

    // Perfetto uses clock_gettime for its internal snapshotting, which gets
    // blocked by the sandboxed and isn't needed for Chrome regardless.
    perfetto_config.set_disable_clock_snapshotting(true);

    auto* trace_event_data_source = perfetto_config.add_data_sources();
    for (auto& enabled_pid :
         chrome_config.process_filter_config().included_process_ids()) {
      *trace_event_data_source->add_producer_name_filter() =
          base::StrCat({mojom::kPerfettoProducerName, ".",
                        base::NumberToString(enabled_pid)});
    }

    // We strip the process filter from the config string we send to Perfetto,
    // so perfetto doesn't reject it from a future
    // TracingService::ChangeTraceConfig call due to being an unsupported
    // update.
    base::trace_event::TraceConfig processfilter_stripped_config(chrome_config);
    processfilter_stripped_config.SetProcessFilterConfig(
        base::trace_event::TraceConfig::ProcessFilterConfig());

#if DCHECK_IS_ON()
    // Ensure that the process filter is the only thing that gets changed
    // in a configuration during a tracing session.
    DCHECK((last_config_for_perfetto_.ToString() ==
            base::trace_event::TraceConfig().ToString()) ||
           (last_config_for_perfetto_.ToString() ==
            processfilter_stripped_config.ToString()));
    last_config_for_perfetto_ = processfilter_stripped_config;
#endif

    auto* trace_event_config = trace_event_data_source->mutable_config();
    trace_event_config->set_name(mojom::kTraceEventDataSourceName);
    trace_event_config->set_target_buffer(0);
    auto* chrome_proto_config = trace_event_config->mutable_chrome_config();
    chrome_proto_config->set_trace_config(
        processfilter_stripped_config.ToString());

// Only CrOS and Cast support system tracing.
#if defined(OS_CHROMEOS) || (defined(IS_CHROMECAST) && defined(OS_LINUX))
    auto* system_trace_config =
        perfetto_config.add_data_sources()->mutable_config();
    system_trace_config->set_name(mojom::kSystemTraceDataSourceName);
    system_trace_config->set_target_buffer(0);
    auto* system_chrome_config = system_trace_config->mutable_chrome_config();
    system_chrome_config->set_trace_config(
        processfilter_stripped_config.ToString());
#endif

#if defined(OS_CHROMEOS)
    auto* arc_trace_config =
        perfetto_config.add_data_sources()->mutable_config();
    arc_trace_config->set_name(mojom::kArcTraceDataSourceName);
    arc_trace_config->set_target_buffer(0);
    auto* arc_chrome_config = arc_trace_config->mutable_chrome_config();
    arc_chrome_config->set_trace_config(
        processfilter_stripped_config.ToString());
#endif

    auto* trace_metadata_config =
        perfetto_config.add_data_sources()->mutable_config();
    trace_metadata_config->set_name(mojom::kMetaDataSourceName);
    trace_metadata_config->set_target_buffer(0);

    return perfetto_config;
  }

  void StopAndFlush(mojo::ScopedDataPipeProducerHandle stream,
                    const std::string& agent_label,
                    StopAndFlushCallback callback) {
    stream_ = std::move(stream);
    stop_and_flush_callback_ = std::move(callback);
    json_trace_exporter_->set_label_filter(agent_label);
    consumer_endpoint_->DisableTracing();
  }

  void OnJSONTraceEventCallback(const std::string& json,
                                base::DictionaryValue* metadata,
                                bool has_more) {
    if (stream_.is_valid()) {
      mojo::BlockingCopyFromString(json, stream_);
    }

    if (!has_more) {
      DCHECK(!stop_and_flush_callback_.is_null());
      base::ResetAndReturn(&stop_and_flush_callback_)
          .Run(/*metadata=*/std::move(*metadata));

      std::move(tracing_over_callback_).Run();
      // |this| is now destroyed.
    }
  }

  void RequestBufferUsage(RequestBufferUsageCallback callback) {
    if (!request_buffer_usage_callback_.is_null()) {
      // TODO(eseckler): Reconsider whether we can DCHECK here after refactoring
      // of content's TracingController.
      std::move(callback).Run(false, 0, 0);
      return;
    }
    request_buffer_usage_callback_ = std::move(callback);
    consumer_endpoint_->GetTraceStats();
  }

  // perfetto::Consumer implementation.
  // This gets called by the Perfetto service as control signals,
  // and to send finished protobufs over.
  void OnConnect() override {}

  void OnDisconnect() override {}

  void OnTracingDisabled() override { consumer_endpoint_->ReadBuffers(); }

  void OnTraceData(std::vector<perfetto::TracePacket> packets,
                   bool has_more) override {
    json_trace_exporter_->OnTraceData(std::move(packets), has_more);
  }

  // Consumer Detach / Attach is not used in Chrome.
  void OnDetach(bool success) override { NOTREACHED(); }

  void OnAttach(bool success, const perfetto::TraceConfig&) override {
    NOTREACHED();
  }

  void OnTraceStats(bool success, const perfetto::TraceStats& stats) override {
    DCHECK(request_buffer_usage_callback_);
    if (!success) {
      std::move(request_buffer_usage_callback_).Run(false, 0.0f, 0);
      return;
    }
    DCHECK_EQ(1, stats.buffer_stats_size());
    const perfetto::TraceStats::BufferStats& buf_stats =
        stats.buffer_stats()[0];
    size_t bytes_in_buffer =
        buf_stats.bytes_written() - buf_stats.bytes_read() -
        buf_stats.bytes_overwritten() + buf_stats.padding_bytes_written() -
        buf_stats.padding_bytes_cleared();
    double percent_full =
        bytes_in_buffer / static_cast<double>(buf_stats.buffer_size());
    percent_full = std::min(std::max(0.0, percent_full), 1.0);
    // We know the size of data in the buffer, but not the number of events.
    std::move(request_buffer_usage_callback_)
        .Run(true, percent_full, 0 /*approx_event_count*/);
  }

 private:
  mojo::ScopedDataPipeProducerHandle stream_;
  std::unique_ptr<JSONTraceExporter> json_trace_exporter_;
  StopAndFlushCallback stop_and_flush_callback_;
  base::OnceClosure tracing_over_callback_;
  RequestBufferUsageCallback request_buffer_usage_callback_;

#if DCHECK_IS_ON()
  base::trace_event::TraceConfig last_config_for_perfetto_;
#endif

  // Keep last to avoid edge-cases where its callbacks come in mid-destruction.
  std::unique_ptr<perfetto::TracingService::ConsumerEndpoint>
      consumer_endpoint_;

  DISALLOW_COPY_AND_ASSIGN(TracingSession);
};

PerfettoTracingCoordinator::PerfettoTracingCoordinator(
    AgentRegistry* agent_registry,
    base::RepeatingClosure on_disconnect_callback)
    : Coordinator(agent_registry, std::move(on_disconnect_callback)),
      binding_(this),
      weak_factory_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

PerfettoTracingCoordinator::~PerfettoTracingCoordinator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PerfettoTracingCoordinator::OnClientConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  tracing_session_.reset();
  binding_.Close();
  Coordinator::OnClientConnectionError();
}

void PerfettoTracingCoordinator::BindCoordinatorRequest(
    mojom::CoordinatorRequest request,
    const service_manager::BindSourceInfo& source_info) {
  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(
      base::BindOnce(&PerfettoTracingCoordinator::OnClientConnectionError,
                     base::Unretained(this)));
}

void PerfettoTracingCoordinator::StartTracing(const std::string& config,
                                              StartTracingCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  parsed_config_ = base::trace_event::TraceConfig(config);
  if (!tracing_session_) {
    tracing_session_ = std::make_unique<TracingSession>(
        parsed_config_,
        base::BindOnce(&PerfettoTracingCoordinator::OnTracingOverCallback,
                       weak_factory_.GetWeakPtr()));
  } else {
    tracing_session_->ChangeTraceConfig(parsed_config_);
  }

  agent_registry_->SetAgentInitializationCallback(
      base::BindRepeating(&PerfettoTracingCoordinator::PingAgent,
                          weak_factory_.GetWeakPtr()),
      false /* call_on_new_agents_only */);

  SetStartTracingCallback(std::move(callback));
}

void PerfettoTracingCoordinator::PingAgent(
    AgentRegistry::AgentEntry* agent_entry) {
  if (!parsed_config_.process_filter_config().IsEnabled(agent_entry->pid()))
    return;

  // TODO(oysteine): While we're still using the Agent
  // system as a fallback when using Perfetto, rather than
  // the browser directly using a Consumer interface, we have to
  // attempt to linearize with newly connected agents so we only
  // call the BeginTracing callback when we can be sure
  // that all current agents have registered with Perfetto and
  // started tracing if requested. We do this linearization
  // explicitly using WaitForTracingEnabled() which will wait
  // for the TraceLog to be enabled before calling its provided
  // callback.
  auto closure = base::BindRepeating(
      [](base::WeakPtr<PerfettoTracingCoordinator> coordinator,
         AgentRegistry::AgentEntry* agent_entry) {
        bool removed =
            agent_entry->RemoveDisconnectClosure(GetStartTracingClosureName());
        DCHECK(removed);

        if (coordinator) {
          coordinator->CallStartTracingCallbackIfNeeded();
        }
      },
      weak_factory_.GetWeakPtr(), base::Unretained(agent_entry));

  agent_entry->AddDisconnectClosure(GetStartTracingClosureName(), closure);

  agent_entry->agent()->WaitForTracingEnabled(closure);
  RemoveExpectedPID(agent_entry->pid());
}

void PerfettoTracingCoordinator::OnTracingOverCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(tracing_session_);
  tracing_session_.reset();
}

void PerfettoTracingCoordinator::StopAndFlush(
    mojo::ScopedDataPipeProducerHandle stream,
    StopAndFlushCallback callback) {
  StopAndFlushAgent(std::move(stream), "", std::move(callback));
}

void PerfettoTracingCoordinator::StopAndFlushInternal(
    mojo::ScopedDataPipeProducerHandle stream,
    const std::string& agent_label,
    StopAndFlushCallback callback) {
  if (start_tracing_callback_) {
    // We received a |StopAndFlush| command before receiving |StartTracing| acks
    // from all agents. Let's retry after a delay.
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PerfettoTracingCoordinator::StopAndFlushInternal,
                       weak_factory_.GetWeakPtr(), std::move(stream),
                       agent_label, std::move(callback)),
        base::TimeDelta::FromMilliseconds(
            mojom::kStopTracingRetryTimeMilliseconds));
    return;
  }

  agent_registry_->SetAgentInitializationCallback(
      base::DoNothing(), true /* call_on_new_agents_only */);
  tracing_session_->StopAndFlush(std::move(stream), agent_label,
                                 std::move(callback));
}

void PerfettoTracingCoordinator::StopAndFlushAgent(
    mojo::ScopedDataPipeProducerHandle stream,
    const std::string& agent_label,
    StopAndFlushCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(tracing_session_);

  ClearConnectedPIDs();
  StopAndFlushInternal(std::move(stream), agent_label, std::move(callback));
}

void PerfettoTracingCoordinator::IsTracing(IsTracingCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(tracing_session_ != nullptr);
}

void PerfettoTracingCoordinator::RequestBufferUsage(
    RequestBufferUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!tracing_session_) {
    std::move(callback).Run(false, 0, 0);
    return;
  }
  tracing_session_->RequestBufferUsage(std::move(callback));
}

}  // namespace tracing
