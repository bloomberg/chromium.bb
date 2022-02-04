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
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/statistics_recorder.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_local.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/typed_macros.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "services/tracing/public/cpp/perfetto/track_event_thread_local_event_sink.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_metadata.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pbzero.h"

namespace base {

class HistogramSamples;

namespace trace_event {
class ThreadInstructionCount;
class TraceEvent;
struct TraceEventHandle;
}  // namespace trace_event

}  // namespace base

namespace perfetto {
class TraceWriter;
}

namespace tracing {

class ThreadLocalEventSink;

// This class is a data source that clients can use to provide
// global metadata in dictionary form, by registering callbacks.
class COMPONENT_EXPORT(TRACING_CPP) TraceEventMetadataSource
    : public PerfettoTracedProcess::DataSourceBase {
 public:
  static TraceEventMetadataSource* GetInstance();

  TraceEventMetadataSource(const TraceEventMetadataSource&) = delete;
  TraceEventMetadataSource& operator=(const TraceEventMetadataSource&) = delete;

  using JsonMetadataGeneratorFunction =
      base::RepeatingCallback<absl::optional<base::Value>()>;

  using MetadataGeneratorFunction = base::RepeatingCallback<void(
      perfetto::protos::pbzero::ChromeMetadataPacket*,
      bool /* privacy_filtering_enabled */)>;

  using PacketGeneratorFunction =
      base::RepeatingCallback<void(perfetto::protos::pbzero::TracePacket*,
                                   bool /* privacy_filtering_enabled */)>;

  // Any callbacks passed here will be called when tracing. Note that if tracing
  // is enabled while calling this method, the callback may be invoked
  // directly.
  void AddGeneratorFunction(JsonMetadataGeneratorFunction generator);
  // Same as above, but for filling in proto format.
  void AddGeneratorFunction(MetadataGeneratorFunction generator);
  void AddGeneratorFunction(PacketGeneratorFunction generator);
  // For background tracing, the legacy crash uploader needs
  // metadata fields to be uploaded as POST args in addition to being
  // embedded in the trace. TODO(oysteine): Remove when only the
  // UMA uploader path is used.
  base::Value GenerateLegacyMetadataDict();

  // PerfettoTracedProcess::DataSourceBase implementation:
  void StartTracingImpl(
      PerfettoProducer* producer,
      const perfetto::DataSourceConfig& data_source_config) override;
  void StopTracingImpl(base::OnceClosure stop_complete_callback) override;
  void Flush(base::RepeatingClosure flush_complete_callback) override;
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  base::SequencedTaskRunner* GetTaskRunner() override;
#endif

  void ResetForTesting();

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  using DataSourceProxy =
      PerfettoTracedProcess::DataSourceProxy<TraceEventMetadataSource>;
#endif

 private:
  friend class base::NoDestructor<TraceEventMetadataSource>;
  friend class perfetto::DataSource<TraceEventMetadataSource>;

  TraceEventMetadataSource();
  ~TraceEventMetadataSource() override;

  void GenerateMetadata(
      std::unique_ptr<std::vector<JsonMetadataGeneratorFunction>>
          json_generators,
      std::unique_ptr<std::vector<MetadataGeneratorFunction>> proto_generators,
      std::unique_ptr<std::vector<PacketGeneratorFunction>> packet_generators);
  void GenerateMetadataFromGenerator(
      const MetadataGeneratorFunction& generator);
  void GenerateJsonMetadataFromGenerator(
      const JsonMetadataGeneratorFunction& generator,
      perfetto::protos::pbzero::ChromeEventBundle* event_bundle);
  void GenerateMetadataPacket(
      const TraceEventMetadataSource::PacketGeneratorFunction& generator);

  void WriteMetadataPacket(perfetto::protos::pbzero::ChromeMetadataPacket*,
                           bool privacy_filtering_enabled);
  absl::optional<base::Value> GenerateTraceConfigMetadataDict();

  // All members are protected by |lock_|.
  // TODO(crbug.com/1138893): Change annotations to GUARDED_BY
  base::Lock lock_;
  std::vector<JsonMetadataGeneratorFunction> json_generator_functions_
      GUARDED_BY(lock_);
  std::vector<MetadataGeneratorFunction> generator_functions_ GUARDED_BY(lock_);
  std::vector<PacketGeneratorFunction> packet_generator_functions_
      GUARDED_BY(lock_);

  const scoped_refptr<base::SequencedTaskRunner> origin_task_runner_
      GUARDED_BY_FIXME(lock_);

#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  std::unique_ptr<perfetto::TraceWriter> trace_writer_ GUARDED_BY_FIXME(lock_);
#endif
  bool privacy_filtering_enabled_ GUARDED_BY_FIXME(lock_) = false;
  std::string chrome_config_ GUARDED_BY(lock_);
  std::unique_ptr<base::trace_event::TraceConfig> parsed_chrome_config_
      GUARDED_BY(lock_);
  bool emit_metadata_at_start_ GUARDED_BY(lock_) = false;
};

// This class acts as a bridge between the TraceLog and
// the PerfettoProducer. It converts incoming
// trace events to ChromeTraceEvent protos and writes
// them into the Perfetto shared memory.
class COMPONENT_EXPORT(TRACING_CPP) TraceEventDataSource :
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
    public perfetto::TrackEventSessionObserver,
#endif
    public PerfettoTracedProcess::DataSourceBase {
 public:
  struct SessionFlags {
    // True if startup tracing is enabled for the current tracing session.
    bool is_startup_tracing : 1;

    // This ID is incremented whenever a new tracing session is started (either
    // when startup tracing is enabled or when the service tells us to start the
    // session otherwise).
    uint32_t session_id : 31;
  };

  static TraceEventDataSource* GetInstance();

  TraceEventDataSource(const TraceEventDataSource&) = delete;
  TraceEventDataSource& operator=(const TraceEventDataSource&) = delete;

  // Destroys and recreates the global instance for testing.
  static void ResetForTesting();

  // Flushes and deletes the TraceWriter for the current thread, if any.
  static void FlushCurrentThread();

  // Installs TraceLog overrides for tracing during Chrome startup.
  void RegisterStartupHooks();

  // PerfettoProducer::DataSourceBase implementation:
  void StartTracingImpl(
      PerfettoProducer* producer,
      const perfetto::DataSourceConfig& data_source_config) override;
  void StopTracingImpl(base::OnceClosure stop_complete_callback) override;
  void Flush(base::RepeatingClosure flush_complete_callback) override;
  void ClearIncrementalState() override;
  void SetupStartupTracing(PerfettoProducer* producer,
                           const base::trace_event::TraceConfig& trace_config,
                           bool privacy_filtering_enabled) override;
  void AbortStartupTracing() override;

  // Deletes TraceWriter safely on behalf of a ThreadLocalEventSink.
  void ReturnTraceWriter(std::unique_ptr<perfetto::TraceWriter> trace_writer);

  bool IsEnabled();
  bool IsPrivacyFilteringEnabled();

  // Records trace event for a histogram sample. When histogram_samples category
  // is enabled, it is registered with base::StatisticsRecorder to monitor the
  // histograms listed in the trace config. If there are no histograms listed in
  // the trace config, all the histograms will be monitored.
  static void OnMetricsSampleCallback(const char* histogram_name,
                                      uint64_t name_hash,
                                      base::HistogramBase::Sample sample);

  // Registered as a callback to receive every action recorded using
  // base::RecordAction(), when tracing is enabled with a histogram category.
  static void OnUserActionSampleCallback(const std::string& action,
                                         base::TimeTicks action_time);

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  // perfetto::TrackEventSessionObserver:
  void OnSetup(const perfetto::DataSourceBase::SetupArgs&) override;
  void OnStop(const perfetto::DataSourceBase::StopArgs&) override;
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

 private:
  friend class base::NoDestructor<TraceEventDataSource>;

  TraceEventDataSource();
  ~TraceEventDataSource() override;

  void OnFlushFinished(const scoped_refptr<base::RefCountedString>&,
                       bool has_more_events);

  void StartTracingInternal(
      PerfettoProducer* producer_client,
      const perfetto::DataSourceConfig& data_source_config);

  void RegisterWithTraceLog(const base::trace_event::TraceConfig& trace_config);
  void OnStopTracingDone();

  std::unique_ptr<perfetto::TraceWriter> CreateTraceWriterLocked();
  TrackEventThreadLocalEventSink* CreateThreadLocalEventSink();

  // Returns the event sink for the current thread, creates it if none unless |!create_if_needed|.
  static TrackEventThreadLocalEventSink* GetOrPrepareEventSink(bool create_if_needed = true);

  // Callback from TraceLog / typed macros, can be called from any thread.
  static void OnAddLegacyTraceEvent(
      base::trace_event::TraceEvent* trace_event,
      bool thread_will_flush,
      base::trace_event::TraceEventHandle* handle);
  static base::trace_event::TrackEventHandle OnAddTypedTraceEvent(
      base::trace_event::TraceEvent* trace_event);
  static void OnUpdateDuration(
      const unsigned char* category_group_enabled,
      const char* name,
      base::trace_event::TraceEventHandle handle,
      int thread_id,
      bool explicit_timestamps,
      const base::TimeTicks& now,
      const base::ThreadTicks& thread_now,
      base::trace_event::ThreadInstructionCount thread_instruction_now);
  static base::trace_event::TracePacketHandle OnAddTracePacket();
  static void OnAddEmptyPacket();

  // Extracts UMA histogram names that should be logged in traces and logs their
  // starting values.
  void ResetHistograms(const base::trace_event::TraceConfig& trace_config);
  // Logs selected UMA histogram.
  void LogHistograms();
  // Logs a given histogram in traces.
  void LogHistogram(base::HistogramBase* histogram);
  void EmitTrackDescriptor();

  uint32_t IncrementSessionIdOrClearStartupFlagWhileLocked();
  void SetStartupTracingFlagsWhileLocked();
  bool IsStartupTracingActive() const;

  bool disable_interning_ = false;
  base::OnceClosure stop_complete_callback_;
  base::TimeTicks process_creation_time_ticks_;

  // Incremented and accessed atomically but without memory order guarantees.
  static constexpr uint32_t kInvalidSessionID = 0;
  std::atomic<SessionFlags> session_flags_{
      SessionFlags{false, kInvalidSessionID}};

  // To avoid lock-order inversion, this lock should not be held while making
  // calls to mojo interfaces or posting tasks, or calling any other code path
  // that may acquire another lock that may also be held while emitting a trace
  // event (crbug.com/986248). Use AutoLockWithDeferredTaskPosting rather than
  // base::AutoLock to protect code paths which may post tasks.
  // TODO(eseckler): Use GUARDED_BY annotations on all fields below.
  base::Lock lock_;  // Protects subsequent members.
  raw_ptr<PerfettoProducer> producer_ GUARDED_BY(lock_) = nullptr;
  uint32_t target_buffer_ = 0;
  std::unique_ptr<perfetto::TraceWriter> trace_writer_;
  bool is_enabled_ = false;
  bool flushing_trace_log_ = false;
  base::OnceClosure flush_complete_task_;
  std::vector<std::string> histograms_;
  // For each of the Histogram that we are tracking, cache the snapshot of their
  // HistogramSamples from before tracing began. So that we can calculate the
  // delta when we go to LogHistograms.
  std::map<std::string, std::unique_ptr<base::HistogramSamples>>
      startup_histogram_samples_;
  // Stores the registered histogram callbacks for which OnMetricsSampleCallback
  // was set individually.
  std::vector<
      std::unique_ptr<base::StatisticsRecorder::ScopedHistogramSampleObserver>>
      monitored_histograms_;
  bool privacy_filtering_enabled_ = false;
  std::string process_name_;
  int process_id_ = base::kNullProcessId;
  base::ActionCallback user_action_callback_ =
      base::BindRepeating(&TraceEventDataSource::OnUserActionSampleCallback);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACE_EVENT_DATA_SOURCE_H_
