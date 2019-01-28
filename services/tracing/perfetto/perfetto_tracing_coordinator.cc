// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/perfetto_tracing_coordinator.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "services/tracing/perfetto/json_trace_exporter.h"
#include "services/tracing/perfetto/perfetto_service.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_stats.h"

namespace tracing {

// A TracingSession is used to wrap all the associated
// state of an on-going tracing session, for easy setup
// and cleanup.
class PerfettoTracingCoordinator::TracingSession {
 public:
  TracingSession(const std::string& config,
                 base::OnceClosure tracing_over_callback)
      : tracing_over_callback_(std::move(tracing_over_callback)) {
    json_trace_exporter_ = std::make_unique<JSONTraceExporter>(
        config, PerfettoService::GetInstance()->GetService());
  }

  ~TracingSession() {
    if (!stop_and_flush_callback_.is_null()) {
      base::ResetAndReturn(&stop_and_flush_callback_)
          .Run(base::Value(base::Value::Type::DICTIONARY));
    }

    stream_.reset();
  }

  void StopAndFlush(mojo::ScopedDataPipeProducerHandle stream,
                    StopAndFlushCallback callback) {
    stream_ = std::move(stream);
    stop_and_flush_callback_ = std::move(callback);

    json_trace_exporter_->StopAndFlush(base::BindRepeating(
        &TracingSession::OnJSONTraceEventCallback, base::Unretained(this)));
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
    json_trace_exporter_->GetTraceStats(
        base::BindOnce(&TracingSession::OnTraceStats, base::Unretained(this)));
  }

  void OnTraceStats(bool success, const perfetto::TraceStats& stats) {
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
  PerfettoService::GetInstance()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&PerfettoTracingCoordinator::BindOnSequence,
                                base::Unretained(this), std::move(request)));
}

void PerfettoTracingCoordinator::BindOnSequence(
    mojom::CoordinatorRequest request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(
      base::BindOnce(&PerfettoTracingCoordinator::OnClientConnectionError,
                     base::Unretained(this)));
}

void PerfettoTracingCoordinator::StartTracing(const std::string& config,
                                              StartTracingCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  tracing_session_ = std::make_unique<TracingSession>(
      config, base::BindOnce(&PerfettoTracingCoordinator::OnTracingOverCallback,
                             weak_factory_.GetWeakPtr()));
  std::move(callback).Run(true);
}

void PerfettoTracingCoordinator::OnTracingOverCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(tracing_session_);
  tracing_session_.reset();
}

void PerfettoTracingCoordinator::StopAndFlush(
    mojo::ScopedDataPipeProducerHandle stream,
    StopAndFlushCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(tracing_session_);
  tracing_session_->StopAndFlush(std::move(stream), std::move(callback));
}

void PerfettoTracingCoordinator::StopAndFlushAgent(
    mojo::ScopedDataPipeProducerHandle stream,
    const std::string& agent_label,
    StopAndFlushCallback callback) {
  NOTREACHED();
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
