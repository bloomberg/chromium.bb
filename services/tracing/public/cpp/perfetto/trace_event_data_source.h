// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACE_EVENT_DATA_SOURCE_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACE_EVENT_DATA_SOURCE_H_

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/metrics/histogram_base.h"
#include "base/threading/thread_local.h"
#include "base/trace_event/trace_config.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"

namespace perfetto {
class StartupTraceWriter;
class StartupTraceWriterRegistry;
class TraceWriter;
}

namespace tracing {

// This class is a data source that clients can use to provide
// global metadata in dictionary form, by registering callbacks.
class COMPONENT_EXPORT(TRACING_CPP) TraceEventMetadataSource
    : public ProducerClient::DataSourceBase {
 public:
  TraceEventMetadataSource();
  ~TraceEventMetadataSource() override;

  using MetadataGeneratorFunction =
      base::RepeatingCallback<std::unique_ptr<base::DictionaryValue>()>;
  // Any callbacks passed here will be called when tracing starts.
  void AddGeneratorFunction(MetadataGeneratorFunction generator);

  // ProducerClient::DataSourceBase implementation, called by
  // ProducerClent.
  void StartTracing(
      ProducerClient* producer_client,
      const perfetto::DataSourceConfig& data_source_config) override;
  void StopTracing(base::OnceClosure stop_complete_callback) override;
  void Flush(base::RepeatingClosure flush_complete_callback) override;

 private:
  void GenerateMetadata(std::unique_ptr<perfetto::TraceWriter> trace_writer);

  std::vector<MetadataGeneratorFunction> generator_functions_;
  scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;
  std::unique_ptr<perfetto::TraceWriter> trace_writer_;

  DISALLOW_COPY_AND_ASSIGN(TraceEventMetadataSource);
};

// This class acts as a bridge between the TraceLog and
// the Perfetto ProducerClient. It converts incoming
// trace events to ChromeTraceEvent protos and writes
// them into the Perfetto shared memory.
class COMPONENT_EXPORT(TRACING_CPP) TraceEventDataSource
    : public ProducerClient::DataSourceBase {
 public:
  class ThreadLocalEventSink;

  static TraceEventDataSource* GetInstance();

  // Flushes and deletes the TraceWriter for the current thread, if any.
  static void FlushCurrentThread();

  // Installs TraceLog overrides for tracing during Chrome startup. Trace data
  // is locally buffered until connection to the perfetto service is
  // established. Expects a later call to StartTracing() to bind to the perfetto
  // service. Should only be called once.
  void SetupStartupTracing();

  // The ProducerClient is responsible for calling StopTracing
  // which will clear the stored pointer to it, before it
  // gets destroyed. ProducerClient::CreateTraceWriter can be
  // called by the TraceEventDataSource on any thread.
  void StartTracing(
      ProducerClient* producer_client,
      const perfetto::DataSourceConfig& data_source_config) override;

  // Called from the ProducerClient.
  void StopTracing(base::OnceClosure stop_complete_callback) override;
  void Flush(base::RepeatingClosure flush_complete_callback) override;

 private:
  friend class base::NoDestructor<TraceEventDataSource>;

  TraceEventDataSource();
  ~TraceEventDataSource() override;

  void RegisterWithTraceLog();
  void UnregisterFromTraceLog();

  ThreadLocalEventSink* CreateThreadLocalEventSink(bool thread_will_flush);

  // Callback from TraceLog, can be called from any thread.
  static void OnAddTraceEvent(base::trace_event::TraceEvent* trace_event,
                              bool thread_will_flush,
                              base::trace_event::TraceEventHandle* handle);
  static void OnUpdateDuration(base::trace_event::TraceEventHandle handle,
                               const base::TimeTicks& now,
                               const base::ThreadTicks& thread_now);

  // Deletes TraceWriter safely on behalf of a ThreadLocalEventSink.
  void ReturnTraceWriter(
      std::unique_ptr<perfetto::StartupTraceWriter> trace_writer);

  // Extracts UMA histogram names that should be logged in traces and logs their
  // starting values.
  void ResetHistograms(const base::trace_event::TraceConfig& trace_config);
  // Logs selected UMA histogram.
  void LogHistograms();
  // Logs a given histogram in traces.
  void LogHistogram(base::HistogramBase* histogram);

  base::OnceClosure stop_complete_callback_;

  // Incremented and accessed atomically but without memory order guarantees.
  // This ID is incremented whenever a new tracing session is started.
  static constexpr uint32_t kInvalidSessionID = 0;
  static constexpr uint32_t kFirstSessionID = 1;
  std::atomic<uint32_t> session_id_{kInvalidSessionID};

  base::Lock lock_;  // Protects subsequent members.
  uint32_t target_buffer_ = 0;
  ProducerClient* producer_client_ = nullptr;
  // We own the registry during startup, but transfer its ownership to the
  // ProducerClient once the perfetto service is available. Only set if
  // SetupStartupTracing() is called.
  std::unique_ptr<perfetto::StartupTraceWriterRegistry>
      startup_writer_registry_;
  std::vector<std::string> histograms_;

  DISALLOW_COPY_AND_ASSIGN(TraceEventDataSource);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACE_EVENT_DATA_SOURCE_H_
