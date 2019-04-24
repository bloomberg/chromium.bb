// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/consumer_host.h"

#include <tuple>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/wait.h"
#include "services/tracing/perfetto/perfetto_service.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_packet.h"
#include "third_party/perfetto/protos/perfetto/config/trace_config.pb.h"

namespace tracing {

// static
void ConsumerHost::BindConsumerRequest(
    PerfettoService* service,
    mojom::ConsumerHostRequest request,
    const service_manager::BindSourceInfo& source_info) {
  mojo::MakeStrongBinding(std::make_unique<ConsumerHost>(service->GetService()),
                          std::move(request));
}

ConsumerHost::ConsumerHost(perfetto::TracingService* service)
    : weak_factory_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  consumer_endpoint_ = service->ConnectConsumer(this, 0 /*uid_t*/);
}

ConsumerHost::~ConsumerHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ConsumerHost::EnableTracing(mojom::TracingSessionPtr tracing_session,
                                 const perfetto::TraceConfig& trace_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  tracing_session_ = std::move(tracing_session);
  consumer_endpoint_->EnableTracing(trace_config);
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
  DCHECK(tracing_session_);
  tracing_session_.reset();
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
