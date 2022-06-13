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

#ifndef SRC_TRACING_INTERNAL_TRACING_MUXER_IMPL_H_
#define SRC_TRACING_INTERNAL_TRACING_MUXER_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <atomic>
#include <bitset>
#include <list>
#include <map>
#include <memory>
#include <vector>

#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/thread_checker.h"
#include "perfetto/ext/tracing/core/basic_types.h"
#include "perfetto/ext/tracing/core/consumer.h"
#include "perfetto/ext/tracing/core/producer.h"
#include "perfetto/tracing/core/data_source_descriptor.h"
#include "perfetto/tracing/core/forward_decls.h"
#include "perfetto/tracing/core/trace_config.h"
#include "perfetto/tracing/internal/basic_types.h"
#include "perfetto/tracing/internal/tracing_muxer.h"
#include "perfetto/tracing/tracing.h"

#include "protos/perfetto/common/interceptor_descriptor.gen.h"

namespace perfetto {

class ConsumerEndpoint;
class DataSourceBase;
class ProducerEndpoint;
class TraceWriterBase;
class TracingBackend;
class TracingSession;
struct TracingInitArgs;

namespace base {
class TaskRunner;
}

namespace internal {

struct DataSourceStaticState;

// This class acts as a bridge between the public API and the TracingBackend(s).
// It exposes a simplified view of the world to the API methods handling all the
// bookkeeping to map data source instances and trace writers to the various
// backends. It deals with N data sources, M backends (1 backend == 1 tracing
// service == 1 producer connection) and T concurrent tracing sessions.
//
// Handing data source registration and start/stop flows [producer side]:
// ----------------------------------------------------------------------
// 1. The API client subclasses perfetto::DataSource and calls
//    DataSource::Register<MyDataSource>(). In turn this calls into the
//    TracingMuxer.
// 2. The tracing muxer iterates through all the backends (1 backend == 1
//    service == 1 producer connection) and registers the data source on each
//    backend.
// 3. When any (services behind a) backend starts tracing and requests to start
//    that specific data source, the TracingMuxerImpl constructs a new instance
//    of MyDataSource and calls the OnStart() method.
//
// Controlling trace and retrieving trace data [consumer side]:
// ------------------------------------------------------------
// 1. The API client calls Tracing::NewTrace(), returns a RAII TracingSession
//    object.
// 2. NewTrace() calls into internal::TracingMuxer(Impl). TracingMuxer
//    subclasses the TracingSession object (TracingSessionImpl) and returns it.
// 3. The tracing muxer identifies the backend (according to the args passed to
//    NewTrace), creates a new Consumer and connects to it.
// 4. When the API client calls Start()/Stop()/ReadTrace() methods, the
//    TracingMuxer forwards them to the consumer associated to the
//    TracingSession. Likewise for callbacks coming from the consumer-side of
//    the service.
class TracingMuxerImpl : public TracingMuxer {
 public:
  // This is different than TracingSessionID because it's global across all
  // backends. TracingSessionID is global only within the scope of one service.
  using TracingSessionGlobalID = uint64_t;

  static void InitializeInstance(const TracingInitArgs&);
  static void ResetForTesting();

  // TracingMuxer implementation.
  bool RegisterDataSource(const DataSourceDescriptor&,
                          DataSourceFactory,
                          DataSourceStaticState*) override;
  void UpdateDataSourceDescriptor(const DataSourceDescriptor&,
                                  const DataSourceStaticState*) override;
  std::unique_ptr<TraceWriterBase> CreateTraceWriter(
      DataSourceStaticState*,
      uint32_t data_source_instance_index,
      DataSourceState*,
      BufferExhaustedPolicy buffer_exhausted_policy) override;
  void DestroyStoppedTraceWritersForCurrentThread() override;
  void RegisterInterceptor(const InterceptorDescriptor&,
                           InterceptorFactory,
                           InterceptorBase::TLSFactory,
                           InterceptorBase::TracePacketCallback) override;

  std::unique_ptr<TracingSession> CreateTracingSession(BackendType);

  // Producer-side bookkeeping methods.
  void UpdateDataSourcesOnAllBackends();
  void SetupDataSource(TracingBackendId,
                       uint32_t backend_connection_id,
                       DataSourceInstanceID,
                       const DataSourceConfig&);
  void StartDataSource(TracingBackendId, DataSourceInstanceID);
  void StopDataSource_AsyncBegin(TracingBackendId, DataSourceInstanceID);
  void StopDataSource_AsyncEnd(TracingBackendId, DataSourceInstanceID);
  void ClearDataSourceIncrementalState(TracingBackendId, DataSourceInstanceID);
  void SyncProducersForTesting();

  // Consumer-side bookkeeping methods.
  void SetupTracingSession(TracingSessionGlobalID,
                           const std::shared_ptr<TraceConfig>&,
                           base::ScopedFile trace_fd = base::ScopedFile());
  void StartTracingSession(TracingSessionGlobalID);
  void ChangeTracingSessionConfig(TracingSessionGlobalID, const TraceConfig&);
  void StopTracingSession(TracingSessionGlobalID);
  void DestroyTracingSession(TracingSessionGlobalID);
  void FlushTracingSession(TracingSessionGlobalID,
                           uint32_t,
                           std::function<void(bool)>);
  void ReadTracingSessionData(
      TracingSessionGlobalID,
      std::function<void(TracingSession::ReadTraceCallbackArgs)>);
  void GetTraceStats(TracingSessionGlobalID,
                     TracingSession::GetTraceStatsCallback);
  void QueryServiceState(TracingSessionGlobalID,
                         TracingSession::QueryServiceStateCallback);

  // Sets the batching period to |batch_commits_duration_ms| on the backends
  // with type |backend_type|.
  void SetBatchCommitsDurationForTesting(uint32_t batch_commits_duration_ms,
                                         BackendType backend_type);

  // Enables direct SMB patching on the backends with type |backend_type| (see
  // SharedMemoryArbiter::EnableDirectSMBPatching). Returns true if the
  // operation succeeded for all backends with type |backend_type|, false
  // otherwise.
  bool EnableDirectSMBPatchingForTesting(BackendType backend_type);

  void SetMaxProducerReconnectionsForTesting(uint32_t count);

 private:
  // For each TracingBackend we create and register one ProducerImpl instance.
  // This talks to the producer-side of the service, gets start/stop requests
  // from it and routes them to the registered data sources.
  // One ProducerImpl == one backend == one tracing service.
  // This class is needed to disambiguate callbacks coming from different
  // services. TracingMuxerImpl can't directly implement the Producer interface
  // because the Producer virtual methods don't allow to identify the service.
  class ProducerImpl : public Producer {
   public:
    ProducerImpl(TracingMuxerImpl*,
                 TracingBackendId,
                 uint32_t shmem_batch_commits_duration_ms);
    ~ProducerImpl() override;

    void Initialize(std::unique_ptr<ProducerEndpoint> endpoint);
    void RegisterDataSource(const DataSourceDescriptor&,
                            DataSourceFactory,
                            DataSourceStaticState*);
    void DisposeConnection();

    // perfetto::Producer implementation.
    void OnConnect() override;
    void OnDisconnect() override;
    void OnTracingSetup() override;
    void SetupDataSource(DataSourceInstanceID,
                         const DataSourceConfig&) override;
    void StartDataSource(DataSourceInstanceID,
                         const DataSourceConfig&) override;
    void StopDataSource(DataSourceInstanceID) override;
    void Flush(FlushRequestID, const DataSourceInstanceID*, size_t) override;
    void ClearIncrementalState(const DataSourceInstanceID*, size_t) override;

    bool SweepDeadServices();

    PERFETTO_THREAD_CHECKER(thread_checker_)
    TracingMuxerImpl* muxer_;
    TracingBackendId const backend_id_;
    bool connected_ = false;
    bool did_setup_tracing_ = false;
    uint32_t connection_id_ = 0;

    const uint32_t shmem_batch_commits_duration_ms_ = 0;

    // Set of data sources that have been actually registered on this producer.
    // This can be a subset of the global |data_sources_|, because data sources
    // can register before the producer is fully connected.
    std::bitset<kMaxDataSources> registered_data_sources_{};

    // A collection of disconnected service endpoints. Since trace writers on
    // arbitrary threads might continue writing data to disconnected services,
    // we keep the old services around and periodically try to clean up ones
    // that no longer have any writers (see SweepDeadServices).
    std::list<std::shared_ptr<ProducerEndpoint>> dead_services_;

    // The currently active service endpoint is maintained as an atomic shared
    // pointer so it won't get deleted from underneath threads that are creating
    // trace writers. At any given time one endpoint can be shared (and thus
    // kept alive) by the |service_| pointer, an entry in |dead_services_| and
    // as a pointer on the stack in CreateTraceWriter() (on an arbitrary
    // thread). The endpoint is never shared outside ProducerImpl itself.
    //
    // WARNING: Any *write* access to this variable or any *read* access from a
    // non-muxer thread must be done through std::atomic_{load,store} to avoid
    // data races.
    std::shared_ptr<ProducerEndpoint> service_;  // Keep last.
  };

  // For each TracingSession created by the API client (Tracing::NewTrace() we
  // create and register one ConsumerImpl instance.
  // This talks to the consumer-side of the service, gets end-of-trace and
  // on-trace-data callbacks and routes them to the API client callbacks.
  // This class is needed to disambiguate callbacks coming from different
  // tracing sessions.
  class ConsumerImpl : public Consumer {
   public:
    ConsumerImpl(TracingMuxerImpl*,
                 BackendType,
                 TracingBackendId,
                 TracingSessionGlobalID);
    ~ConsumerImpl() override;

    void Initialize(std::unique_ptr<ConsumerEndpoint> endpoint);

    // perfetto::Consumer implementation.
    void OnConnect() override;
    void OnDisconnect() override;
    void OnTracingDisabled(const std::string& error) override;
    void OnTraceData(std::vector<TracePacket>, bool has_more) override;
    void OnDetach(bool success) override;
    void OnAttach(bool success, const TraceConfig&) override;
    void OnTraceStats(bool success, const TraceStats&) override;
    void OnObservableEvents(const ObservableEvents&) override;

    void NotifyStartComplete();
    void NotifyError(const TracingError&);
    void NotifyStopComplete();

    // Will eventually inform the |muxer_| when it is safe to remove |this|.
    void Disconnect();

    TracingMuxerImpl* muxer_;
    BackendType const backend_type_;
    TracingBackendId const backend_id_;
    TracingSessionGlobalID const session_id_;
    bool connected_ = false;

    // This is to handle the case where the Setup call from the API client
    // arrives before the consumer has connected. In this case we keep around
    // the config and check if we have it after connection.
    bool start_pending_ = false;

    // Similarly if the session is stopped before the consumer was connected, we
    // need to wait until the session has started before stopping it.
    bool stop_pending_ = false;

    // Similarly we need to buffer a call to get trace statistics if the
    // consumer wasn't connected yet.
    bool get_trace_stats_pending_ = false;

    // Whether this session was already stopped. This will happen in response to
    // Stop{,Blocking}, but also if the service stops the session for us
    // automatically (e.g., when there are no data sources).
    bool stopped_ = false;

    // shared_ptr because it's posted across threads. This is to avoid copying
    // it more than once.
    std::shared_ptr<TraceConfig> trace_config_;
    base::ScopedFile trace_fd_;

    // If the API client passes a callback to start, we should invoke this when
    // NotifyStartComplete() is invoked.
    std::function<void()> start_complete_callback_;

    // An internal callback used to implement StartBlocking().
    std::function<void()> blocking_start_complete_callback_;

    // If the API client passes a callback to get notification about the
    // errors, we should invoke this when NotifyError() is invoked.
    std::function<void(TracingError)> error_callback_;

    // If the API client passes a callback to stop, we should invoke this when
    // OnTracingDisabled() is invoked.
    std::function<void()> stop_complete_callback_;

    // An internal callback used to implement StopBlocking().
    std::function<void()> blocking_stop_complete_callback_;

    // Callback passed to ReadTrace().
    std::function<void(TracingSession::ReadTraceCallbackArgs)>
        read_trace_callback_;

    // Callback passed to GetTraceStats().
    TracingSession::GetTraceStatsCallback get_trace_stats_callback_;

    // Callback for a pending call to QueryServiceState().
    TracingSession::QueryServiceStateCallback query_service_state_callback_;

    // The states of all data sources in this tracing session. |true| means the
    // data source has started tracing.
    using DataSourceHandle = std::pair<std::string, std::string>;
    std::map<DataSourceHandle, bool> data_source_states_;

    std::unique_ptr<ConsumerEndpoint> service_;  // Keep before last.
    PERFETTO_THREAD_CHECKER(thread_checker_)     // Keep last.
  };

  // This object is returned to API clients when they call
  // Tracing::CreateTracingSession().
  class TracingSessionImpl : public TracingSession {
   public:
    TracingSessionImpl(TracingMuxerImpl*, TracingSessionGlobalID, BackendType);
    ~TracingSessionImpl() override;
    void Setup(const TraceConfig&, int fd) override;
    void Start() override;
    void StartBlocking() override;
    void SetOnStartCallback(std::function<void()>) override;
    void SetOnErrorCallback(std::function<void(TracingError)>) override;
    void Stop() override;
    void StopBlocking() override;
    void Flush(std::function<void(bool)>, uint32_t timeout_ms) override;
    void ReadTrace(ReadTraceCallback) override;
    void SetOnStopCallback(std::function<void()>) override;
    void GetTraceStats(GetTraceStatsCallback) override;
    void QueryServiceState(QueryServiceStateCallback) override;
    void ChangeTraceConfig(const TraceConfig&) override;

   private:
    TracingMuxerImpl* const muxer_;
    TracingSessionGlobalID const session_id_;
    BackendType const backend_type_;
  };

  struct RegisteredDataSource {
    DataSourceDescriptor descriptor;
    DataSourceFactory factory{};
    DataSourceStaticState* static_state = nullptr;
  };

  struct RegisteredInterceptor {
    protos::gen::InterceptorDescriptor descriptor;
    InterceptorFactory factory{};
    InterceptorBase::TLSFactory tls_factory{};
    InterceptorBase::TracePacketCallback packet_callback{};
  };

  struct RegisteredBackend {
    // Backends are supposed to have static lifetime.
    TracingBackend* backend = nullptr;
    TracingBackendId id = 0;
    BackendType type{};

    TracingBackend::ConnectProducerArgs producer_conn_args;
    std::unique_ptr<ProducerImpl> producer;

    // The calling code can request more than one concurrently active tracing
    // session for the same backend. We need to create one consumer per session.
    std::vector<std::unique_ptr<ConsumerImpl>> consumers;
  };

  void UpdateDataSourceOnAllBackends(RegisteredDataSource& rds,
                                     bool is_changed);
  explicit TracingMuxerImpl(const TracingInitArgs&);
  void Initialize(const TracingInitArgs& args);
  ConsumerImpl* FindConsumer(TracingSessionGlobalID session_id);
  void InitializeConsumer(TracingSessionGlobalID session_id);
  void OnConsumerDisconnected(ConsumerImpl* consumer);
  void OnProducerDisconnected(ProducerImpl* producer);
  void SweepDeadBackends();

  struct FindDataSourceRes {
    FindDataSourceRes() = default;
    FindDataSourceRes(DataSourceStaticState* a, DataSourceState* b, uint32_t c)
        : static_state(a), internal_state(b), instance_idx(c) {}
    explicit operator bool() const { return !!internal_state; }

    DataSourceStaticState* static_state = nullptr;
    DataSourceState* internal_state = nullptr;
    uint32_t instance_idx = 0;
  };
  FindDataSourceRes FindDataSource(TracingBackendId, DataSourceInstanceID);

  // WARNING: If you add new state here, be sure to update ResetForTesting.
  std::unique_ptr<base::TaskRunner> task_runner_;
  std::vector<RegisteredDataSource> data_sources_;
  std::vector<RegisteredBackend> backends_;
  std::vector<RegisteredInterceptor> interceptors_;
  TracingPolicy* policy_ = nullptr;

  std::atomic<TracingSessionGlobalID> next_tracing_session_id_{};
  std::atomic<uint32_t> next_data_source_index_{};
  uint32_t muxer_id_for_testing_{};

  // Maximum number of times we will try to reconnect producer backend.
  // Should only be modified for testing purposes.
  std::atomic<uint32_t> max_producer_reconnections_{100u};

  // After ResetForTesting() is called, holds tracing backends which needs to be
  // kept alive until all inbound references have gone away. See
  // SweepDeadBackends().
  std::list<RegisteredBackend> dead_backends_;

  PERFETTO_THREAD_CHECKER(thread_checker_)
};

}  // namespace internal
}  // namespace perfetto

#endif  // SRC_TRACING_INTERNAL_TRACING_MUXER_IMPL_H_
