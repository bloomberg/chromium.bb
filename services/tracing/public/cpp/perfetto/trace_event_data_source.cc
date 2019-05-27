// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"

#include <atomic>
#include <map>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/no_destructor.h"
#include "base/pickle.h"
#include "base/trace_event/trace_event.h"
#include "components/tracing/common/tracing_switches.h"
#include "services/tracing/public/cpp/perfetto/thread_local_event_sink.h"
#include "services/tracing/public/cpp/perfetto/traced_value_proto_writer.h"
#include "services/tracing/public/cpp/perfetto/track_event_thread_local_event_sink.h"
#include "services/tracing/public/cpp/trace_event_args_whitelist.h"
#include "services/tracing/public/mojom/constants.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/core/shared_memory_arbiter.h"
#include "third_party/perfetto/include/perfetto/tracing/core/startup_trace_writer.h"
#include "third_party/perfetto/include/perfetto/tracing/core/startup_trace_writer_registry.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_writer.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

using TraceLog = base::trace_event::TraceLog;
using TraceEvent = base::trace_event::TraceEvent;
using TraceConfig = base::trace_event::TraceConfig;

namespace tracing {

using ChromeEventBundleHandle =
    protozero::MessageHandle<perfetto::protos::pbzero::ChromeEventBundle>;

TraceEventMetadataSource::TraceEventMetadataSource()
    : DataSourceBase(mojom::kMetaDataSourceName),
      origin_task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  AddGeneratorFunction(base::BindRepeating(
      &TraceEventMetadataSource::GenerateTraceConfigMetadataDict,
      base::Unretained(this)));
}

TraceEventMetadataSource::~TraceEventMetadataSource() = default;

void TraceEventMetadataSource::AddGeneratorFunction(
    MetadataGeneratorFunction generator) {
  DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());
  generator_functions_.push_back(generator);
}

std::unique_ptr<base::DictionaryValue>
TraceEventMetadataSource::GenerateTraceConfigMetadataDict() {
  if (chrome_config_.empty()) {
    return nullptr;
  }

  base::trace_event::TraceConfig parsed_chrome_config(chrome_config_);

  auto metadata_dict = std::make_unique<base::DictionaryValue>();
  // If argument filtering is enabled, we need to check if the trace config is
  // whitelisted before emitting it.
  // TODO(eseckler): Figure out a way to solve this without calling directly
  // into IsMetadataWhitelisted().
  if (!parsed_chrome_config.IsArgumentFilterEnabled() ||
      IsMetadataWhitelisted("trace-config")) {
    metadata_dict->SetString("trace-config", chrome_config_);
  } else {
    metadata_dict->SetString("trace-config", "__stripped__");
  }

  chrome_config_ = std::string();
  return metadata_dict;
}

void TraceEventMetadataSource::GenerateMetadata(
    std::unique_ptr<perfetto::TraceWriter> trace_writer) {
  DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());

  auto trace_packet = trace_writer->NewTracePacket();
  ChromeEventBundleHandle event_bundle(trace_packet->set_chrome_events());

  for (auto& generator : generator_functions_) {
    std::unique_ptr<base::DictionaryValue> metadata_dict = generator.Run();
    if (!metadata_dict) {
      continue;
    }

    for (const auto& it : metadata_dict->DictItems()) {
      auto* new_metadata = event_bundle->add_metadata();
      new_metadata->set_name(it.first.c_str());

      if (it.second.is_int()) {
        new_metadata->set_int_value(it.second.GetInt());
      } else if (it.second.is_bool()) {
        new_metadata->set_bool_value(it.second.GetBool());
      } else if (it.second.is_string()) {
        new_metadata->set_string_value(it.second.GetString().c_str());
      } else {
        std::string json_value;
        base::JSONWriter::Write(it.second, &json_value);
        new_metadata->set_json_value(json_value.c_str());
      }
    }
  }
}

void TraceEventMetadataSource::StartTracing(
    ProducerClient* producer_client,
    const perfetto::DataSourceConfig& data_source_config) {
  // TODO(eseckler): Once we support streaming of trace data, it would make
  // sense to emit the metadata on startup, so the UI can display it right away.
  privacy_filtering_enabled_ =
      data_source_config.chrome_config().privacy_filtering_enabled();
  chrome_config_ = data_source_config.chrome_config().trace_config();
  trace_writer_ =
      producer_client->CreateTraceWriter(data_source_config.target_buffer());
}

void TraceEventMetadataSource::StopTracing(
    base::OnceClosure stop_complete_callback) {
  if (trace_writer_ && !privacy_filtering_enabled_) {
    // Write metadata at the end of tracing to make it less likely that it is
    // overridden by other trace data in perfetto's ring buffer.
    origin_task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&TraceEventMetadataSource::GenerateMetadata,
                       base::Unretained(this), std::move(trace_writer_)),
        std::move(stop_complete_callback));
  } else {
    trace_writer_.reset();
    chrome_config_ = std::string();
    std::move(stop_complete_callback).Run();
  }
}

void TraceEventMetadataSource::Flush(
    base::RepeatingClosure flush_complete_callback) {
  origin_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                        std::move(flush_complete_callback));
}

namespace {

class AutoThreadLocalBoolean {
 public:
  explicit AutoThreadLocalBoolean(
      base::ThreadLocalBoolean* thread_local_boolean)
      : thread_local_boolean_(thread_local_boolean) {
    DCHECK(!thread_local_boolean_->Get());
    thread_local_boolean_->Set(true);
  }
  ~AutoThreadLocalBoolean() { thread_local_boolean_->Set(false); }

 private:
  base::ThreadLocalBoolean* thread_local_boolean_;
  DISALLOW_COPY_AND_ASSIGN(AutoThreadLocalBoolean);
};

base::ThreadLocalBoolean* GetThreadIsInTraceEventTLS() {
  static base::NoDestructor<base::ThreadLocalBoolean> thread_is_in_trace_event;
  return thread_is_in_trace_event.get();
}

base::ThreadLocalStorage::Slot* ThreadLocalEventSinkSlot() {
  static base::NoDestructor<base::ThreadLocalStorage::Slot>
      thread_local_event_sink_tls([](void* event_sink) {
        AutoThreadLocalBoolean thread_is_in_trace_event(
            GetThreadIsInTraceEventTLS());
        delete static_cast<ThreadLocalEventSink*>(event_sink);
      });

  return thread_local_event_sink_tls.get();
}

TraceEventDataSource* g_trace_event_data_source_for_testing = nullptr;

}  // namespace

// static
TraceEventDataSource* TraceEventDataSource::GetInstance() {
  static base::NoDestructor<TraceEventDataSource> instance;
  return instance.get();
}

// static
void TraceEventDataSource::ResetForTesting() {
  if (!g_trace_event_data_source_for_testing)
    return;
  g_trace_event_data_source_for_testing->~TraceEventDataSource();
  new (g_trace_event_data_source_for_testing) TraceEventDataSource;
}

TraceEventDataSource::TraceEventDataSource()
    : DataSourceBase(mojom::kTraceEventDataSourceName),
      disable_interning_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kPerfettoDisableInterning)) {
  g_trace_event_data_source_for_testing = this;
}

TraceEventDataSource::~TraceEventDataSource() = default;

void TraceEventDataSource::RegisterWithTraceLog() {
  RegisterTracedValueProtoWriter(true);
  TraceLog::GetInstance()->SetAddTraceEventOverrides(
      &TraceEventDataSource::OnAddTraceEvent,
      &TraceEventDataSource::FlushCurrentThread,
      &TraceEventDataSource::OnUpdateDuration);
}

void TraceEventDataSource::UnregisterFromTraceLog() {
  RegisterTracedValueProtoWriter(false);
  TraceLog::GetInstance()->SetAddTraceEventOverrides(nullptr, nullptr, nullptr);
}

void TraceEventDataSource::SetupStartupTracing() {
  {
    base::AutoLock lock(lock_);
    // No need to do anything if startup tracing has already been set,
    // or we know Perfetto has already been setup.
    if (startup_writer_registry_ || producer_client_) {
      return;
    }

    startup_writer_registry_ =
        std::make_unique<perfetto::StartupTraceWriterRegistry>();
  }
  RegisterWithTraceLog();
}

void TraceEventDataSource::StartTracing(
    ProducerClient* producer_client,
    const perfetto::DataSourceConfig& data_source_config) {
  privacy_filtering_enabled_ =
      data_source_config.chrome_config().privacy_filtering_enabled();

  std::unique_ptr<perfetto::StartupTraceWriterRegistry> unbound_writer_registry;
  {
    base::AutoLock lock(lock_);

    DCHECK(!producer_client_);
    producer_client_ = producer_client;
    target_buffer_ = data_source_config.target_buffer();
    // Reduce lock contention by binding the registry without holding the lock.
    unbound_writer_registry = std::move(startup_writer_registry_);
    trace_writers_from_registry_.clear();
  }

  session_id_.fetch_add(1u, std::memory_order_relaxed);

  if (unbound_writer_registry) {
    // TODO(ssid): Startup tracing should know about filtering output.
    CHECK(!privacy_filtering_enabled_);

    // TODO(oysteine): Investigate why trace events emitted by something in
    // BindStartupTraceWriterRegistry() causes deadlocks.
    AutoThreadLocalBoolean thread_is_in_trace_event(
        GetThreadIsInTraceEventTLS());
    producer_client->BindStartupTraceWriterRegistry(
        std::move(unbound_writer_registry), data_source_config.target_buffer());
  } else {
    RegisterWithTraceLog();
  }

  auto trace_config =
      TraceConfig(data_source_config.chrome_config().trace_config());
  TraceLog::GetInstance()->SetEnabled(trace_config, TraceLog::RECORDING_MODE);
  ResetHistograms(trace_config);
  ProducerClient::GetTaskRunner()->StartDeferredTasksDrainTimer();
}

void TraceEventDataSource::StopTracing(
    base::OnceClosure stop_complete_callback) {
  LogHistograms();
  stop_complete_callback_ = std::move(stop_complete_callback);

  auto on_tracing_stopped_callback =
      [](TraceEventDataSource* data_source,
         const scoped_refptr<base::RefCountedString>&, bool has_more_events) {
        if (has_more_events) {
          return;
        }

        // It's extremely unlikely any threads are still in mid-trace-event
        // at this point and end up posting new tasks to the PerfettoTaskRunner
        // which end up not getting run until the next tracing session; worst
        // case is we lose some chunk commit messages and Perfetto will
        // scrape the chunks.
        ProducerClient::GetTaskRunner()->StopDeferredTasksDrainTimer();
        data_source->UnregisterFromTraceLog();

        if (data_source->stop_complete_callback_) {
          std::move(data_source->stop_complete_callback_).Run();
        }
      };

  bool was_enabled = TraceLog::GetInstance()->IsEnabled();
  if (was_enabled) {
    // Write metadata events etc.
    TraceLog::GetInstance()->SetDisabled();
  }

  {
    // Prevent recreation of ThreadLocalEventSinks after flush.
    base::AutoLock lock(lock_);
    DCHECK(producer_client_);
    producer_client_ = nullptr;
    target_buffer_ = 0;
  }

  if (was_enabled) {
    // TraceLog::SetDisabled will cause metadata events to be written; make
    // sure we flush the TraceWriter for this thread (TraceLog will only call
    // TraceEventDataSource::FlushCurrentThread for threads with a MessageLoop).
    // TODO(eseckler): Flush all worker threads.
    // TODO(oysteine): The perfetto service itself should be able to recover
    // unreturned chunks so technically this can go away at some point, but
    // seems needed for now.
    FlushCurrentThread();

    // Flush the remaining threads via TraceLog. We call CancelTracing because
    // we don't want/need TraceLog to do any of its own JSON serialization.
    TraceLog::GetInstance()->CancelTracing(base::BindRepeating(
        on_tracing_stopped_callback, base::Unretained(this)));
  } else {
    on_tracing_stopped_callback(this, scoped_refptr<base::RefCountedString>(),
                                false);
  }
}

void TraceEventDataSource::LogHistogram(base::HistogramBase* histogram) {
  if (!histogram) {
    return;
  }
  auto samples = histogram->SnapshotSamples();
  base::Pickle pickle;
  samples->Serialize(&pickle);
  std::string buckets;
  base::Base64Encode(
      std::string(static_cast<const char*>(pickle.data()), pickle.size()),
      &buckets);
  TRACE_EVENT_INSTANT2("benchmark", "UMAHistogramSamples",
                       TRACE_EVENT_SCOPE_PROCESS, "name",
                       histogram->histogram_name(), "buckets", buckets);
}

void TraceEventDataSource::ResetHistograms(const TraceConfig& trace_config) {
  histograms_.clear();
  for (const std::string& histogram_name : trace_config.histogram_names()) {
    histograms_.push_back(histogram_name);
    LogHistogram(base::StatisticsRecorder::FindHistogram(histogram_name));
  }
}

void TraceEventDataSource::LogHistograms() {
  for (const std::string& histogram_name : histograms_) {
    LogHistogram(base::StatisticsRecorder::FindHistogram(histogram_name));
  }
}

void TraceEventDataSource::Flush(
    base::RepeatingClosure flush_complete_callback) {
  DCHECK(TraceLog::GetInstance()->IsEnabled());
  TraceLog::GetInstance()->Flush(base::BindRepeating(
      [](base::RepeatingClosure flush_complete_callback,
         const scoped_refptr<base::RefCountedString>&, bool has_more_events) {
        if (has_more_events) {
          return;
        }

        flush_complete_callback.Run();
      },
      std::move(flush_complete_callback)));
}

void TraceEventDataSource::ResetIncrementalStateForTesting() {
  auto* thread_local_event_sink =
      static_cast<ThreadLocalEventSink*>(ThreadLocalEventSinkSlot()->Get());
  if (thread_local_event_sink) {
    thread_local_event_sink->ResetIncrementalState();
  }
}

ThreadLocalEventSink* TraceEventDataSource::CreateThreadLocalEventSink(
    bool thread_will_flush) {
  base::AutoLock lock(lock_);
  // |startup_writer_registry_| only exists during startup tracing before we
  // connect to the service. |producer_client_| is reset when tracing is
  // stopped.
  std::unique_ptr<perfetto::StartupTraceWriter> trace_writer;
  uint32_t session_id = session_id_.load(std::memory_order_relaxed);
  if (startup_writer_registry_) {
    trace_writer = startup_writer_registry_->CreateUnboundTraceWriter();
    trace_writers_from_registry_.insert(trace_writer.get());
  } else if (producer_client_) {
    trace_writer = std::make_unique<perfetto::StartupTraceWriter>(
        producer_client_->CreateTraceWriter(target_buffer_));
  }

  if (!trace_writer) {
    return nullptr;
  }

  return new TrackEventThreadLocalEventSink(std::move(trace_writer), session_id,
                                            disable_interning_,
                                            privacy_filtering_enabled_);
}

// static
void TraceEventDataSource::OnAddTraceEvent(
    TraceEvent* trace_event,
    bool thread_will_flush,
    base::trace_event::TraceEventHandle* handle) {
  // Avoid re-entrancy, which can happen during PostTasks (the taskqueue can
  // emit trace events). We discard the events in this case, as any PostTasking
  // to deal with these events later would break the event ordering that the
  // JSON traces rely on to merge 'A'/'B' events, as well as having to deal with
  // updating duration of 'X' events which haven't been added yet.
  if (GetThreadIsInTraceEventTLS()->Get()) {
    return;
  }

  AutoThreadLocalBoolean thread_is_in_trace_event(GetThreadIsInTraceEventTLS());

  auto* thread_local_event_sink =
      static_cast<ThreadLocalEventSink*>(ThreadLocalEventSinkSlot()->Get());

  // Make sure the sink was reset since the last tracing session. Normally, it
  // is reset on Flush after the session is disabled. However, it may not have
  // been reset if the current thread doesn't support flushing. In that case, we
  // need to check here that it writes to the right buffer.
  //
  // Because we want to avoid locking for each event, we access |session_id_|
  // racily. It's OK if we don't see it change to the session immediately. In
  // that case, the first few trace events may get lost, but we will eventually
  // notice that we are writing to the wrong buffer once the change to
  // |session_id_| has propagated, and reset the sink. Note we will still
  // acquire the |lock_| to safely recreate the sink in
  // CreateThreadLocalEventSink().
  if (!thread_will_flush && thread_local_event_sink) {
    uint32_t new_session_id =
        GetInstance()->session_id_.load(std::memory_order_relaxed);
    // Ignore the first session to avoid resetting the sink during startup
    // tracing, where the sink is created with kInvalidSessionID. Resetting the
    // sink during startup might cause data buffered in its potentially still
    // unbound StartupTraceWriter to be lost.
    // NOTE: If the trace event we're adding disallows PostTasks (meaning
    // events emitted while the taskqueue is locked), we can't reset the
    // sink as the TraceWriter deletion is done through PostTask.
    if (new_session_id > kFirstSessionID &&
        new_session_id != thread_local_event_sink->session_id() &&
        !(trace_event->flags() & TRACE_EVENT_FLAG_DISALLOW_POSTTASK)) {
      delete thread_local_event_sink;
      thread_local_event_sink = nullptr;
    }
  }

  if (!thread_local_event_sink) {
    // Trace events emitted by the task queue itself can happen while the task
    // queue is locked, posting to it reentrantly would deadlock so these events
    // need to be flagged so we can avoid PostTasks while they're being emitted.
    ScopedPerfettoPostTaskBlocker post_task_blocker(
        !!(trace_event->flags() & TRACE_EVENT_FLAG_DISALLOW_POSTTASK));

    thread_local_event_sink =
        GetInstance()->CreateThreadLocalEventSink(thread_will_flush);
    ThreadLocalEventSinkSlot()->Set(thread_local_event_sink);
  }

  if (thread_local_event_sink) {
    thread_local_event_sink->AddTraceEvent(trace_event, handle);
  }
}

// static
void TraceEventDataSource::OnUpdateDuration(
    base::trace_event::TraceEventHandle handle,
    const base::TimeTicks& now,
    const base::ThreadTicks& thread_now) {
  if (GetThreadIsInTraceEventTLS()->Get()) {
    return;
  }

  AutoThreadLocalBoolean thread_is_in_trace_event(GetThreadIsInTraceEventTLS());

  auto* thread_local_event_sink =
      static_cast<ThreadLocalEventSink*>(ThreadLocalEventSinkSlot()->Get());
  if (thread_local_event_sink) {
    thread_local_event_sink->UpdateDuration(handle, now, thread_now);
  }
}

// static
void TraceEventDataSource::FlushCurrentThread() {
  auto* thread_local_event_sink =
      static_cast<ThreadLocalEventSink*>(ThreadLocalEventSinkSlot()->Get());
  if (thread_local_event_sink) {
    // Prevent any events from being emitted while we're deleting
    // the sink (like from the TraceWriter being PostTask'ed for deletion).
    AutoThreadLocalBoolean thread_is_in_trace_event(
        GetThreadIsInTraceEventTLS());
    thread_local_event_sink->Flush();
    // TODO(oysteine): To support flushing while still recording, this needs to
    // be changed to not destruct the TLS object as that will emit any
    // uncompleted _COMPLETE events on the stack.
    delete thread_local_event_sink;
    ThreadLocalEventSinkSlot()->Set(nullptr);
  }
}

void TraceEventDataSource::ReturnTraceWriter(
    std::unique_ptr<perfetto::StartupTraceWriter> trace_writer) {
  base::AutoLock lock(lock_);
  // It's possible that the returned trace writer was created by a former
  // StartupTraceWriterRegistry. In this case, we should not attempt to return
  // it to the current registry, so we need to verify first that it was indeed
  // created by the current registry.
  if (startup_writer_registry_ &&
      trace_writers_from_registry_.find(trace_writer.get()) !=
          trace_writers_from_registry_.end()) {
    // If the writer is still unbound, the registry will keep it alive until it
    // was bound and its buffered data was copied. This ensures that we don't
    // lose data from threads that are shut down during startup.
    trace_writers_from_registry_.erase(trace_writer.get());
    startup_writer_registry_->ReturnUnboundTraceWriter(std::move(trace_writer));
  } else {
    // Delete the TraceWriter on the sequence that Perfetto runs on, needed
    // as the ThreadLocalEventSink gets deleted on thread
    // shutdown and we can't safely call TaskRunnerHandle::Get() at that point
    // (which can happen as the TraceWriter destructor might make a Mojo call
    // and trigger it).
    ProducerClient::GetTaskRunner()->task_runner()->DeleteSoon(
        FROM_HERE, std::move(trace_writer));
  }
}

}  // namespace tracing
