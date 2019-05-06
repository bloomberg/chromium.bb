// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/consumer_host.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/wait.h"
#include "services/tracing/perfetto/json_trace_exporter.h"
#include "services/tracing/perfetto/perfetto_service.h"
#include "services/tracing/perfetto/track_event_json_exporter.h"
#include "services/tracing/public/cpp/trace_event_args_whitelist.h"
#include "third_party/perfetto/include/perfetto/tracing/core/observable_events.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_packet.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_stats.h"
#include "third_party/perfetto/protos/perfetto/config/trace_config.pb.h"

namespace tracing {

namespace {

const int32_t kEnableTracingTimeoutSeconds = 10;

}  // namespace

class ConsumerHost::StreamWriter {
 public:
  using Slices = std::vector<std::string>;

  static scoped_refptr<base::SequencedTaskRunner> CreateTaskRunner() {
    return base::CreateSequencedTaskRunnerWithTraits(
        {base::WithBaseSyncPrimitives(), base::TaskPriority::BEST_EFFORT});
  }

  StreamWriter(mojo::ScopedDataPipeProducerHandle stream,
               ReadBuffersCallback callback,
               base::OnceClosure disconnect_callback,
               scoped_refptr<base::SequencedTaskRunner> callback_task_runner)
      : stream_(std::move(stream)),
        read_buffers_callback_(std::move(callback)),
        disconnect_callback_(std::move(disconnect_callback)),
        callback_task_runner_(callback_task_runner) {}

  void WriteToStream(std::unique_ptr<Slices> slices, bool has_more) {
    DCHECK(stream_.is_valid());
    for (const auto& slice : *slices) {
      uint32_t write_position = 0;

      while (write_position < slice.size()) {
        uint32_t write_bytes = slice.size() - write_position;

        MojoResult result =
            stream_->WriteData(slice.data() + write_position, &write_bytes,
                               MOJO_WRITE_DATA_FLAG_NONE);

        if (result == MOJO_RESULT_OK) {
          write_position += write_bytes;
          continue;
        }

        if (result == MOJO_RESULT_SHOULD_WAIT) {
          result = mojo::Wait(stream_.get(), MOJO_HANDLE_SIGNAL_WRITABLE);
        }

        if (result != MOJO_RESULT_OK) {
          if (!disconnect_callback_.is_null()) {
            callback_task_runner_->PostTask(FROM_HERE,
                                            std::move(disconnect_callback_));
          }
          return;
        }
      }
    }

    if (!has_more && !read_buffers_callback_.is_null()) {
      callback_task_runner_->PostTask(FROM_HERE,
                                      std::move(read_buffers_callback_));
    }
  }

 private:
  mojo::ScopedDataPipeProducerHandle stream_;
  ReadBuffersCallback read_buffers_callback_;
  base::OnceClosure disconnect_callback_;
  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(StreamWriter);
};

// static
void ConsumerHost::BindConsumerRequest(
    PerfettoService* service,
    mojom::ConsumerHostRequest request,
    const service_manager::BindSourceInfo& source_info) {
  mojo::MakeStrongBinding(std::make_unique<ConsumerHost>(service),
                          std::move(request));
}

ConsumerHost::ConsumerHost(PerfettoService* service)
    : service_(service), weak_factory_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  consumer_endpoint_ =
      service_->GetService()->ConnectConsumer(this, 0 /*uid_t*/);
  consumer_endpoint_->ObserveEvents(
      perfetto::TracingService::ConsumerEndpoint::ObservableEventType::
          kDataSourceInstances);
  service_->RegisterConsumerHost(this);
}

ConsumerHost::~ConsumerHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  service_->UnregisterConsumerHost(this);
}

void ConsumerHost::EnableTracing(mojom::TracingSessionPtr tracing_session,
                                 const perfetto::TraceConfig& trace_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  tracing_session_ = std::move(tracing_session);

  privacy_filtering_enabled_ = false;
  for (const auto& data_source : trace_config.data_sources()) {
    if (data_source.config().chrome_config().privacy_filtering_enabled()) {
      privacy_filtering_enabled_ = true;
    }
  }
#if DCHECK_IS_ON()
  if (privacy_filtering_enabled_) {
    // If enabled, filtering must be enabled for all data sources.
    for (const auto& data_source : trace_config.data_sources()) {
      DCHECK(data_source.config().chrome_config().privacy_filtering_enabled());
    }
  }
#endif
  perfetto::TraceConfig trace_config_copy = AdjustTraceConfig(trace_config);

  filtered_pids_.clear();
  for (const auto& ds_config : trace_config_copy.data_sources()) {
    if (ds_config.config().name() == mojom::kTraceEventDataSourceName) {
      for (const auto& filter : ds_config.producer_name_filter()) {
        base::ProcessId pid;
        if (PerfettoService::ParsePidFromProducerName(filter, &pid)) {
          filtered_pids_.insert(pid);
        }
      }
      break;
    }
  }

  pending_enable_tracing_ack_pids_ = service_->active_service_pids();
  base::EraseIf(*pending_enable_tracing_ack_pids_,
                [this](base::ProcessId pid) { return !IsExpectedPid(pid); });

  consumer_endpoint_->EnableTracing(trace_config_copy);
  MaybeSendEnableTracingAck();

  if (pending_enable_tracing_ack_pids_) {
    // We can't know for sure whether all processes we request to connect to the
    // tracing service will connect back, or if all the connected services will
    // ACK our EnableTracing request eventually, so we'll add a timeout for that
    // case.
    enable_tracing_ack_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(kEnableTracingTimeoutSeconds),
        this, &ConsumerHost::OnEnableTracingTimeout);
  }
}

void ConsumerHost::ChangeTraceConfig(
    const perfetto::TraceConfig& trace_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  perfetto::TraceConfig trace_config_copy = AdjustTraceConfig(trace_config);
  consumer_endpoint_->ChangeTraceConfig(trace_config_copy);
}

perfetto::TraceConfig ConsumerHost::AdjustTraceConfig(
    const perfetto::TraceConfig& trace_config) {
  perfetto::TraceConfig trace_config_copy(trace_config);
  // Clock snapshotting is incompatible with chrome's process sandboxing.
  // Telemetry uses its own way of snapshotting clocks anyway.
  auto* builtin_data_sources = trace_config_copy.mutable_builtin_data_sources();
  builtin_data_sources->set_disable_clock_snapshotting(true);
  return trace_config_copy;
}

void ConsumerHost::DisableTracing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  consumer_endpoint_->DisableTracing();
}

void ConsumerHost::Flush(uint32_t timeout,
                         base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  flush_callback_ = std::move(callback);
  base::WeakPtr<ConsumerHost> weak_this = weak_factory_.GetWeakPtr();
  consumer_endpoint_->Flush(timeout, [weak_this](bool success) {
    if (!weak_this) {
      return;
    }

    if (weak_this->flush_callback_) {
      std::move(weak_this->flush_callback_).Run(success);
    }
  });
}

void ConsumerHost::ReadBuffers(mojo::ScopedDataPipeProducerHandle stream,
                               ReadBuffersCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  read_buffers_stream_writer_ = base::SequenceBound<StreamWriter>(
      StreamWriter::CreateTaskRunner(), std::move(stream), std::move(callback),
      base::BindOnce(&ConsumerHost::OnConsumerClientDisconnected,
                     weak_factory_.GetWeakPtr()),
      base::SequencedTaskRunnerHandle::Get());

  consumer_endpoint_->ReadBuffers();
}

void ConsumerHost::DisableTracingAndEmitJson(
    const std::string& agent_label_filter,
    mojo::ScopedDataPipeProducerHandle stream,
    DisableTracingAndEmitJsonCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!read_buffers_stream_writer_);

  read_buffers_stream_writer_ = base::SequenceBound<StreamWriter>(
      StreamWriter::CreateTaskRunner(), std::move(stream), std::move(callback),
      base::BindOnce(&ConsumerHost::OnConsumerClientDisconnected,
                     weak_factory_.GetWeakPtr()),
      base::SequencedTaskRunnerHandle::Get());

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

  JSONTraceExporter::ArgumentFilterPredicate arg_filter_predicate;
  JSONTraceExporter::MetadataFilterPredicate metadata_filter_predicate;
  if (privacy_filtering_enabled_) {
    auto* trace_log = base::trace_event::TraceLog::GetInstance();
    arg_filter_predicate = trace_log->GetArgumentFilterPredicate();
    metadata_filter_predicate = trace_log->GetMetadataFilterPredicate();
  }
  json_trace_exporter_ = std::make_unique<TrackEventJSONExporter>(
      std::move(arg_filter_predicate), std::move(metadata_filter_predicate),
      base::BindRepeating(&ConsumerHost::OnJSONTraceData,
                          base::Unretained(this)));

  json_trace_exporter_->set_label_filter(agent_label_filter);

  consumer_endpoint_->DisableTracing();
}

void ConsumerHost::FreeBuffers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  consumer_endpoint_->FreeBuffers();
}

void ConsumerHost::RequestBufferUsage(RequestBufferUsageCallback callback) {
  if (!request_buffer_usage_callback_.is_null()) {
    std::move(callback).Run(false, 0);
    return;
  }

  request_buffer_usage_callback_ = std::move(callback);
  consumer_endpoint_->GetTraceStats();
}

void ConsumerHost::OnConnect() {}

void ConsumerHost::OnDisconnect() {}

void ConsumerHost::OnTracingDisabled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(tracing_session_);

  if (enable_tracing_ack_timer_.IsRunning()) {
    enable_tracing_ack_timer_.FireNow();
  }
  DCHECK(!pending_enable_tracing_ack_pids_);

  tracing_session_.reset();

  if (json_trace_exporter_) {
    consumer_endpoint_->ReadBuffers();
  }
}

void ConsumerHost::OnTraceData(std::vector<perfetto::TracePacket> packets,
                               bool has_more) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (json_trace_exporter_) {
    json_trace_exporter_->OnTraceData(std::move(packets), has_more);
    if (!has_more) {
      json_trace_exporter_.reset();
    }
    return;
  }

  auto copy = std::make_unique<StreamWriter::Slices>();
  for (auto& packet : packets) {
    char* data;
    size_t size;
    std::tie(data, size) = packet.GetProtoPreamble();
    copy->emplace_back(data, size);
    auto& slices = packet.slices();
    for (auto& slice : slices) {
      copy->emplace_back(static_cast<const char*>(slice.start), slice.size);
    }
  }
  read_buffers_stream_writer_.Post(FROM_HERE, &StreamWriter::WriteToStream,
                                   std::move(copy), has_more);
  if (!has_more) {
    read_buffers_stream_writer_.Reset();
  }
}

void ConsumerHost::OnObservableEvents(
    const perfetto::ObservableEvents& events) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pending_enable_tracing_ack_pids_) {
    return;
  }

  for (const auto& state_change : events.instance_state_changes()) {
    if (state_change.state() !=
        perfetto::ObservableEvents::DataSourceInstanceStateChange::
            DATA_SOURCE_INSTANCE_STATE_STARTED) {
      continue;
    }

    if (state_change.data_source_name() != mojom::kTraceEventDataSourceName) {
      continue;
    }

    // Attempt to parse the PID out of the producer name.
    base::ProcessId pid;
    if (!PerfettoService::ParsePidFromProducerName(state_change.producer_name(),
                                                   &pid)) {
      continue;
    }

    pending_enable_tracing_ack_pids_->erase(pid);
  }
  MaybeSendEnableTracingAck();
}

void ConsumerHost::OnActiveServicePidAdded(base::ProcessId pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pending_enable_tracing_ack_pids_ && IsExpectedPid(pid)) {
    pending_enable_tracing_ack_pids_->insert(pid);
  }
}

void ConsumerHost::OnActiveServicePidRemoved(base::ProcessId pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pending_enable_tracing_ack_pids_) {
    pending_enable_tracing_ack_pids_->erase(pid);
    MaybeSendEnableTracingAck();
  }
}

void ConsumerHost::OnActiveServicePidsInitialized() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeSendEnableTracingAck();
}

void ConsumerHost::OnEnableTracingTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pending_enable_tracing_ack_pids_) {
    return;
  }
  DCHECK(tracing_session_);
  tracing_session_->OnTracingEnabled();
  pending_enable_tracing_ack_pids_.reset();
}

void ConsumerHost::MaybeSendEnableTracingAck() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pending_enable_tracing_ack_pids_ ||
      !pending_enable_tracing_ack_pids_->empty() ||
      !service_->active_service_pids_initialized()) {
    return;
  }

  DCHECK(tracing_session_);
  tracing_session_->OnTracingEnabled();
  pending_enable_tracing_ack_pids_.reset();
  enable_tracing_ack_timer_.Stop();
}

bool ConsumerHost::IsExpectedPid(base::ProcessId pid) const {
  return filtered_pids_.empty() || base::ContainsKey(filtered_pids_, pid);
}

void ConsumerHost::OnTraceStats(bool success,
                                const perfetto::TraceStats& stats) {
  if (!request_buffer_usage_callback_) {
    return;
  }

  if (!success || stats.buffer_stats_size() != 1) {
    std::move(request_buffer_usage_callback_).Run(false, 0.0f);
    return;
  }

  const perfetto::TraceStats::BufferStats& buf_stats = stats.buffer_stats()[0];
  size_t bytes_in_buffer = buf_stats.bytes_written() - buf_stats.bytes_read() -
                           buf_stats.bytes_overwritten() +
                           buf_stats.padding_bytes_written() -
                           buf_stats.padding_bytes_cleared();
  double percent_full =
      bytes_in_buffer / static_cast<double>(buf_stats.buffer_size());
  percent_full = std::min(std::max(0.0, percent_full), 1.0);
  std::move(request_buffer_usage_callback_).Run(true, percent_full);
}

void ConsumerHost::OnJSONTraceData(std::string* json,
                                   base::DictionaryValue* metadata,
                                   bool has_more) {
  auto slices = std::make_unique<StreamWriter::Slices>();
  slices->push_back(std::string());
  slices->back().swap(*json);
  read_buffers_stream_writer_.Post(FROM_HERE, &StreamWriter::WriteToStream,
                                   std::move(slices), has_more);

  if (!has_more) {
    read_buffers_stream_writer_.Reset();
  }
}

void ConsumerHost::OnConsumerClientDisconnected() {
  // Bail out; destination handle got closed.
  consumer_endpoint_->FreeBuffers();
}

}  // namespace tracing
