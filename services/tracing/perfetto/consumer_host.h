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
#include "base/threading/sequence_bound.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/core/consumer.h"
#include "third_party/perfetto/include/perfetto/tracing/core/tracing_service.h"

namespace service_manager {
struct BindSourceInfo;
}  // namespace service_manager

namespace tracing {

class JSONTraceExporter;
class PerfettoService;

// This is a Mojo interface which enables any client
// to act as a Perfetto consumer.
class ConsumerHost : public perfetto::Consumer, public mojom::ConsumerHost {
 public:
  static void BindConsumerRequest(
      PerfettoService* service,
      mojom::ConsumerHostRequest request,
      const service_manager::BindSourceInfo& source_info);

  class StreamWriter;
  class TracingSession : public mojom::TracingSessionHost {
   public:
    TracingSession(ConsumerHost* host,
                   mojom::TracingSessionHostRequest tracing_session_host,
                   mojom::TracingSessionClientPtr tracing_session_client,
                   const perfetto::TraceConfig& trace_config,
                   mojom::TracingClientPriority priority);
    ~TracingSession() override;

    void OnPerfettoEvents(const perfetto::ObservableEvents&);
    void OnTraceData(std::vector<perfetto::TracePacket> packets, bool has_more);
    void OnTraceStats(bool success, const perfetto::TraceStats&);
    void OnTracingDisabled();
    void OnConsumerClientDisconnected();
    void Flush(uint32_t timeout, base::OnceCallback<void(bool)> callback);

    mojom::TracingClientPriority tracing_priority() const {
      return tracing_priority_;
    }
    bool tracing_enabled() const { return tracing_enabled_; }
    ConsumerHost* host() const { return host_; }

    // Called by TracingService.
    void OnActiveServicePidAdded(base::ProcessId pid);
    void OnActiveServicePidRemoved(base::ProcessId pid);
    void OnActiveServicePidsInitialized();
    void RequestDisableTracing(base::OnceClosure on_disabled_callback);

    // mojom::TracingSessionHost implementation.
    void ChangeTraceConfig(const perfetto::TraceConfig& config) override;
    void DisableTracing() override;
    void ReadBuffers(mojo::ScopedDataPipeProducerHandle stream,
                     ReadBuffersCallback callback) override;

    void RequestBufferUsage(RequestBufferUsageCallback callback) override;
    void DisableTracingAndEmitJson(
        const std::string& agent_label_filter,
        mojo::ScopedDataPipeProducerHandle stream,
        DisableTracingAndEmitJsonCallback callback) override;

   private:
    void OnJSONTraceData(std::string* json,
                         base::DictionaryValue* metadata,
                         bool has_more);
    void OnEnableTracingTimeout();
    void MaybeSendEnableTracingAck();
    bool IsExpectedPid(base::ProcessId pid) const;

    ConsumerHost* const host_;
    mojom::TracingSessionClientPtr tracing_session_client_;
    mojo::Binding<mojom::TracingSessionHost> binding_;
    bool privacy_filtering_enabled_ = false;
    base::SequenceBound<StreamWriter> read_buffers_stream_writer_;
    RequestBufferUsageCallback request_buffer_usage_callback_;
    std::unique_ptr<JSONTraceExporter> json_trace_exporter_;
    base::OnceCallback<void(bool)> flush_callback_;
    const mojom::TracingClientPriority tracing_priority_;
    base::OnceClosure on_disabled_callback_;
    std::set<base::ProcessId> filtered_pids_;
    bool tracing_enabled_ = true;

    // If set, we didn't issue OnTracingEnabled() on the session yet. If set and
    // empty, no more pids are pending and we should issue OnTracingEnabled().
    base::Optional<std::set<base::ProcessId>> pending_enable_tracing_ack_pids_;
    base::OneShotTimer enable_tracing_ack_timer_;

    SEQUENCE_CHECKER(sequence_checker_);
    base::WeakPtrFactory<TracingSession> weak_factory_{this};

    DISALLOW_COPY_AND_ASSIGN(TracingSession);
  };

  // The owner of ConsumerHost should make sure to destroy
  // |service| after destroying this.
  explicit ConsumerHost(PerfettoService* service);
  ~ConsumerHost() override;

  PerfettoService* service() const { return service_; }
  perfetto::TracingService::ConsumerEndpoint* consumer_endpoint() const {
    return consumer_endpoint_.get();
  }

  // mojom::ConsumerHost implementation.
  void EnableTracing(mojom::TracingSessionHostRequest tracing_session_host,
                     mojom::TracingSessionClientPtr tracing_session_client,
                     const perfetto::TraceConfig& config,
                     mojom::TracingClientPriority priority) override;

  // perfetto::Consumer implementation.
  // This gets called by the Perfetto service as control signals,
  // and to send finished protobufs over.
  void OnConnect() override;
  void OnDisconnect() override;
  void OnTracingDisabled() override;
  void OnTraceData(std::vector<perfetto::TracePacket> packets,
                   bool has_more) override;
  void OnObservableEvents(const perfetto::ObservableEvents&) override;
  void OnTraceStats(bool success, const perfetto::TraceStats&) override;

  // Unused in Chrome.
  void OnDetach(bool success) override {}
  void OnAttach(bool success, const perfetto::TraceConfig&) override {}

  TracingSession* tracing_session_for_testing() {
    return tracing_session_.get();
  }

 private:
  void DestructTracingSession();

  PerfettoService* const service_;
  std::unique_ptr<TracingSession> tracing_session_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Keep last to avoid edge-cases where its callbacks come in mid-destruction.
  std::unique_ptr<perfetto::TracingService::ConsumerEndpoint>
      consumer_endpoint_;

  base::WeakPtrFactory<ConsumerHost> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ConsumerHost);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PERFETTO_CONSUMER_HOST_H_
