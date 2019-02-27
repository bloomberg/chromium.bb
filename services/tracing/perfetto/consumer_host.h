// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PERFETTO_CONSUMER_HOST_H_
#define SERVICES_TRACING_PERFETTO_CONSUMER_HOST_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/core/consumer.h"
#include "third_party/perfetto/include/perfetto/tracing/core/tracing_service.h"

namespace service_manager {
struct BindSourceInfo;
}  // namespace service_manager

namespace tracing {

class PerfettoService;

// This is a Mojo interface which enables any client
// to act as a Perfetto consumer.
class ConsumerHost : public perfetto::Consumer, public mojom::ConsumerHost {
 public:
  static void BindConsumerRequest(
      PerfettoService* service,
      mojom::ConsumerHostRequest request,
      const service_manager::BindSourceInfo& source_info);

  // The owner of ConsumerHost should make sure to destroy
  // |service| after destroying this.
  explicit ConsumerHost(perfetto::TracingService* service);
  ~ConsumerHost() override;

  // mojom::ConsumerHost implementation.
  void EnableTracing(mojom::TracingSessionPtr tracing_session,
                     const perfetto::TraceConfig& config) override;
  void ReadBuffers(mojo::ScopedDataPipeProducerHandle stream,
                   ReadBuffersCallback callback) override;

  void DisableTracing();
  void Flush(uint32_t timeout, base::OnceCallback<void(bool)> callback);
  void FreeBuffers();

  // perfetto::ConsumerHost implementation.
  // This gets called by the Perfetto service as control signals,
  // and to send finished protobufs over.
  void OnConnect() override;
  void OnDisconnect() override;
  void OnTracingDisabled() override;
  void OnTraceData(std::vector<perfetto::TracePacket> packets,
                   bool has_more) override;

  // Unused in Chrome.
  void OnDetach(bool success) override {}
  void OnAttach(bool success, const perfetto::TraceConfig&) override {}
  void OnTraceStats(bool success, const perfetto::TraceStats&) override {}

 private:
  void WriteToStream(const void* start, size_t size);

  mojo::ScopedDataPipeProducerHandle read_buffers_stream_;
  ReadBuffersCallback read_buffers_callback_;
  base::OnceCallback<void(bool)> flush_callback_;
  mojom::TracingSessionPtr tracing_session_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Keep last to avoid edge-cases where its callbacks come in mid-destruction.
  std::unique_ptr<perfetto::TracingService::ConsumerEndpoint>
      consumer_endpoint_;

  base::WeakPtrFactory<ConsumerHost> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(ConsumerHost);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PERFETTO_CONSUMER_HOST_H_
