// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/consumer_host.h"

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
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/wait.h"
#include "services/tracing/perfetto/perfetto_service.h"
#include "third_party/perfetto/include/perfetto/tracing/core/observable_events.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_packet.h"
#include "third_party/perfetto/protos/perfetto/config/trace_config.pb.h"

namespace tracing {

namespace {

bool StringToProcessId(const std::string& input, base::ProcessId* output) {
  // Pid is encoded as uint in the string.
  return base::StringToUint(input, reinterpret_cast<uint32_t*>(output));
}

}  // namespace

// static
bool ConsumerHost::ParsePidFromProducerName(const std::string& producer_name,
                                            base::ProcessId* pid) {
  if (!base::StartsWith(producer_name, mojom::kPerfettoProducerNamePrefix,
                        base::CompareCase::SENSITIVE)) {
    LOG(DFATAL) << "Unexpected producer name: " << producer_name;
    return false;
  }

  static const size_t kPrefixLength =
      strlen(mojom::kPerfettoProducerNamePrefix);
  if (!StringToProcessId(producer_name.substr(kPrefixLength), pid)) {
    LOG(DFATAL) << "Unexpected producer name: " << producer_name;
    return false;
  }
  return true;
}

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

  filtered_pids_.clear();
  for (const auto& ds_config : trace_config.data_sources()) {
    if (ds_config.config().name() == mojom::kTraceEventDataSourceName) {
      for (const auto& filter : ds_config.producer_name_filter()) {
        base::ProcessId pid;
        if (ParsePidFromProducerName(filter, &pid)) {
          filtered_pids_.insert(pid);
        }
      }
      break;
    }
  }

  pending_enable_tracing_ack_pids_ = service_->active_service_pids();
  base::EraseIf(*pending_enable_tracing_ack_pids_,
                [this](base::ProcessId pid) { return !IsExpectedPid(pid); });

  consumer_endpoint_->EnableTracing(trace_config);
  MaybeSendEnableTracingAck();
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
  read_buffers_stream_ = std::move(stream);
  read_buffers_callback_ = std::move(callback);

  consumer_endpoint_->ReadBuffers();
}

void ConsumerHost::FreeBuffers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  consumer_endpoint_->FreeBuffers();
}

void ConsumerHost::OnConnect() {}

void ConsumerHost::OnDisconnect() {}

void ConsumerHost::OnTracingDisabled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(tracing_session_);
  tracing_session_.reset();
  pending_enable_tracing_ack_pids_.reset();
}

void ConsumerHost::OnTraceData(std::vector<perfetto::TracePacket> packets,
                               bool has_more) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& packet : packets) {
    char* data;
    size_t size;
    std::tie(data, size) = packet.GetProtoPreamble();
    WriteToStream(data, size);
    auto& slices = packet.slices();
    for (auto& slice : slices) {
      WriteToStream(slice.start, slice.size);
    }
  }

  if (!has_more) {
    read_buffers_stream_.reset();
    if (read_buffers_callback_) {
      std::move(read_buffers_callback_).Run();
    }
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
    if (!ParsePidFromProducerName(state_change.producer_name(), &pid)) {
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
}

bool ConsumerHost::IsExpectedPid(base::ProcessId pid) const {
  return filtered_pids_.empty() || base::ContainsKey(filtered_pids_, pid);
}

void ConsumerHost::WriteToStream(const void* start, size_t size) {
  TRACE_EVENT0("ipc", "ConsumerHost::WriteToStream");
  DCHECK(read_buffers_stream_.is_valid());
  uint32_t write_position = 0;

  while (write_position < size) {
    uint32_t write_bytes = size - write_position;

    MojoResult result = read_buffers_stream_->WriteData(
        static_cast<const uint8_t*>(start) + write_position, &write_bytes,
        MOJO_WRITE_DATA_FLAG_NONE);

    if (result == MOJO_RESULT_OK) {
      write_position += write_bytes;
      continue;
    }

    if (result == MOJO_RESULT_SHOULD_WAIT) {
      // TODO(oysteine): If we end up actually blocking here it means
      // the client is consuming data slower than Perfetto is producing
      // it. Consider other solutions at that point because it means
      // eventually Producers will run out of chunks and will stall waiting
      // for new ones.
      result =
          mojo::Wait(read_buffers_stream_.get(), MOJO_HANDLE_SIGNAL_WRITABLE);
    }

    if (result != MOJO_RESULT_OK) {
      // Bail out; destination handle got closed.
      consumer_endpoint_->FreeBuffers();
      return;
    }
  }
}

}  // namespace tracing
