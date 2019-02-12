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
#include "base/json/json_writer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/no_destructor.h"
#include "base/pickle.h"
#include "base/process/process_handle.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_buffer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/perfetto/traced_value_proto_writer.h"
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

namespace {
static const size_t kMaxEventsPerMessage = 100;
static const size_t kMaxCompleteEventDepth = 20;

// To mark TraceEvent handles that have been added by Perfetto,
// we use the chunk index so high that TraceLog would've asserted
// at this point anyway.
static const uint32_t kMagicChunkIndex =
    base::trace_event::TraceBufferChunk::kMaxChunkIndex;

}  // namespace

namespace tracing {

using ChromeEventBundleHandle =
    protozero::MessageHandle<perfetto::protos::pbzero::ChromeEventBundle>;

TraceEventMetadataSource::TraceEventMetadataSource()
    : DataSourceBase(mojom::kMetaDataSourceName),
      origin_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}

TraceEventMetadataSource::~TraceEventMetadataSource() = default;

void TraceEventMetadataSource::AddGeneratorFunction(
    MetadataGeneratorFunction generator) {
  DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());
  generator_functions_.push_back(generator);
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
  trace_writer_ =
      producer_client->CreateTraceWriter(data_source_config.target_buffer());
}

void TraceEventMetadataSource::StopTracing(
    base::OnceClosure stop_complete_callback) {
  if (trace_writer_) {
    // Write metadata at the end of tracing to make it less likely that it is
    // overridden by other trace data in perfetto's ring buffer.
    origin_task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&TraceEventMetadataSource::GenerateMetadata,
                       base::Unretained(this), std::move(trace_writer_)),
        std::move(stop_complete_callback));
  } else {
    std::move(stop_complete_callback).Run();
  }
}

void TraceEventMetadataSource::Flush(
    base::RepeatingClosure flush_complete_callback) {
  origin_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                        std::move(flush_complete_callback));
}

class TraceEventDataSource::ThreadLocalEventSink {
 public:
  ThreadLocalEventSink(
      std::unique_ptr<perfetto::StartupTraceWriter> trace_writer,
      uint32_t session_id,
      bool thread_will_flush)
      : trace_writer_(std::move(trace_writer)),
        session_id_(session_id),
        thread_will_flush_(thread_will_flush) {}

  ~ThreadLocalEventSink() {
    // Finalize the current message before posting the |trace_writer_| for
    // destruction, to avoid data races.
    event_bundle_ = ChromeEventBundleHandle();
    trace_packet_handle_ = perfetto::TraceWriter::TracePacketHandle();

    TraceEventDataSource::GetInstance()->ReturnTraceWriter(
        std::move(trace_writer_));
  }

  void EnsureValidHandles() {
    if (trace_packet_handle_) {
      return;
    }

    trace_packet_handle_ = trace_writer_->NewTracePacket();
    event_bundle_ =
        ChromeEventBundleHandle(trace_packet_handle_->set_chrome_events());
    string_table_.clear();
    next_string_table_index_ = 0;
    current_eventcount_for_message_ = 0;
  }

  int GetStringTableIndexForString(const char* str_value) {
    EnsureValidHandles();

    auto it = string_table_.find(reinterpret_cast<intptr_t>(str_value));
    if (it != string_table_.end()) {
      CHECK_EQ(std::string(reinterpret_cast<const char*>(it->first)),
               std::string(str_value));

      return it->second;
    }

    int string_table_index = ++next_string_table_index_;
    string_table_[reinterpret_cast<intptr_t>(str_value)] = string_table_index;

    auto* new_string_table_entry = event_bundle_->add_string_table();
    new_string_table_entry->set_value(str_value);
    new_string_table_entry->set_index(string_table_index);

    return string_table_index;
  }

  void AddConvertableToTraceFormat(
      base::trace_event::ConvertableToTraceFormat* value,
      perfetto::protos::pbzero::ChromeTraceEvent_Arg* arg) {
    PerfettoProtoAppender proto_appender(arg);
    if (value->AppendToProto(&proto_appender)) {
      return;
    }

    std::string json = value->ToString();
    arg->set_json_value(json.c_str());
  }

  void AddTraceEvent(TraceEvent* trace_event,
                     base::trace_event::TraceEventHandle* handle) {
    // TODO(oysteine): Adding trace events to Perfetto will
    // stall in some situations, specifically when we overflow
    // the buffer and need to make a sync call to flush it, and we're
    // running on the same thread as the service. The short-term fix (while
    // we're behind a flag) is to run the service on its own thread, the longer
    // term fix is most likely to not go via Mojo in that specific case.

    if (handle && trace_event->phase() == TRACE_EVENT_PHASE_COMPLETE) {
      // 'X' phase events are added through a scoped object and
      // will have its duration updated when said object drops off
      // the stack; keep a copy of the event around instead of
      // writing it into SHM, until we have the duration.
      // We can't keep the TraceEvent around in the scoped object
      // itself as that causes a lot more codegen in the callsites
      // and bloats the binary size too much (due to the increased
      // sizeof() of the scoped object itself).
      DCHECK_LT(current_stack_depth_, kMaxCompleteEventDepth);
      if (current_stack_depth_ >= kMaxCompleteEventDepth) {
        return;
      }

      complete_event_stack_[current_stack_depth_] = std::move(*trace_event);
      handle->event_index = ++current_stack_depth_;
      handle->chunk_index = kMagicChunkIndex;
      handle->chunk_seq = session_id_;
      return;
    }

    EnsureValidHandles();

    uint32_t name_index = 0;
    uint32_t category_name_index = 0;
    const size_t kMaxSize = base::trace_event::TraceArguments::kMaxSize;
    uint32_t arg_name_indices[kMaxSize] = {0};

    // By default, we bundle multiple events into a single TracePacket, e.g. to
    // avoid repeating strings by interning them into a string table. However,
    // we shouldn't bundle events in two situations:
    // 1) If the thread we're executing on is unable to flush events on demand
    //    (threads without a MessageLoop), the service will only be able to
    //    recover completed TracePackets. For these threads, bundling events
    //    would increase the number of events that are lost at the end of
    //    tracing.
    // 2) During startup tracing, the StartupTraceWriter buffers TracePackets in
    //    a temporary local buffer until it is bound to the SMB when it becomes
    //    available. While a TracePacket is written to this temporary buffer,
    //    the writer cannot be bound to the SMB. Bundling events would increase
    //    the time during which binding the writer is blocked.
    bool bundle_events = thread_will_flush_ && trace_writer_->was_bound();

    // Populate any new string table parts first; has to be done before
    // the add_trace_events() call (as the string table is part of the outer
    // proto message).
    // If the TRACE_EVENT_FLAG_COPY flag is set, the char* pointers aren't
    // necessarily valid after the TRACE_EVENT* call, and so we need to store
    // the string every time.
    bool string_table_enabled =
        !(trace_event->flags() & TRACE_EVENT_FLAG_COPY) && bundle_events;
    if (string_table_enabled) {
      name_index = GetStringTableIndexForString(trace_event->name());
      category_name_index =
          GetStringTableIndexForString(TraceLog::GetCategoryGroupName(
              trace_event->category_group_enabled()));

      for (size_t i = 0;
           i < trace_event->arg_size() && trace_event->arg_name(i); ++i) {
        arg_name_indices[i] =
            GetStringTableIndexForString(trace_event->arg_name(i));
      }
    }

    auto* new_trace_event = event_bundle_->add_trace_events();

    if (name_index) {
      new_trace_event->set_name_index(name_index);
    } else {
      new_trace_event->set_name(trace_event->name());
    }

    if (category_name_index) {
      new_trace_event->set_category_group_name_index(category_name_index);
    } else {
      new_trace_event->set_category_group_name(TraceLog::GetCategoryGroupName(
          trace_event->category_group_enabled()));
    }

    new_trace_event->set_timestamp(
        trace_event->timestamp().since_origin().InMicroseconds());

    uint32_t flags = trace_event->flags();
    new_trace_event->set_flags(flags);

    int process_id;
    int thread_id;
    if ((flags & TRACE_EVENT_FLAG_HAS_PROCESS_ID) &&
        trace_event->process_id() != base::kNullProcessId) {
      process_id = trace_event->process_id();
      thread_id = -1;
    } else {
      process_id = TraceLog::GetInstance()->process_id();
      thread_id = trace_event->thread_id();
    }

    new_trace_event->set_process_id(process_id);
    new_trace_event->set_thread_id(thread_id);

    char phase = trace_event->phase();
    new_trace_event->set_phase(phase);

    for (size_t i = 0; i < trace_event->arg_size() && trace_event->arg_name(i);
         ++i) {
      auto type = trace_event->arg_type(i);
      auto* new_arg = new_trace_event->add_args();

      if (arg_name_indices[i]) {
        new_arg->set_name_index(arg_name_indices[i]);
      } else {
        new_arg->set_name(trace_event->arg_name(i));
      }

      if (type == TRACE_VALUE_TYPE_CONVERTABLE) {
        AddConvertableToTraceFormat(trace_event->arg_convertible_value(i),
                                    new_arg);
        continue;
      }

      auto& value = trace_event->arg_value(i);
      switch (type) {
        case TRACE_VALUE_TYPE_BOOL:
          new_arg->set_bool_value(value.as_bool);
          break;
        case TRACE_VALUE_TYPE_UINT:
          new_arg->set_uint_value(value.as_uint);
          break;
        case TRACE_VALUE_TYPE_INT:
          new_arg->set_int_value(value.as_int);
          break;
        case TRACE_VALUE_TYPE_DOUBLE:
          new_arg->set_double_value(value.as_double);
          break;
        case TRACE_VALUE_TYPE_POINTER:
          new_arg->set_pointer_value(static_cast<uint64_t>(
              reinterpret_cast<uintptr_t>(value.as_pointer)));
          break;
        case TRACE_VALUE_TYPE_STRING:
        case TRACE_VALUE_TYPE_COPY_STRING:
          new_arg->set_string_value(value.as_string ? value.as_string : "NULL");
          break;
        default:
          NOTREACHED() << "Don't know how to print this value";
          break;
      }
    }

    if (phase == TRACE_EVENT_PHASE_COMPLETE) {
      new_trace_event->set_duration(trace_event->duration().InMicroseconds());

      if (!trace_event->thread_timestamp().is_null()) {
        int64_t thread_duration =
            trace_event->thread_duration().InMicroseconds();
        if (thread_duration != -1) {
          new_trace_event->set_thread_duration(thread_duration);
        }
      }
    }

    if (!trace_event->thread_timestamp().is_null()) {
      int64_t thread_time_int64 =
          trace_event->thread_timestamp().since_origin().InMicroseconds();
      new_trace_event->set_thread_timestamp(thread_time_int64);
    }

    if (trace_event->scope() != trace_event_internal::kGlobalScope) {
      new_trace_event->set_scope(trace_event->scope());
    }

    if (flags & (TRACE_EVENT_FLAG_HAS_ID | TRACE_EVENT_FLAG_HAS_LOCAL_ID |
                 TRACE_EVENT_FLAG_HAS_GLOBAL_ID)) {
      new_trace_event->set_id(trace_event->id());
    }

    if ((flags & TRACE_EVENT_FLAG_FLOW_OUT) ||
        (flags & TRACE_EVENT_FLAG_FLOW_IN)) {
      new_trace_event->set_bind_id(trace_event->bind_id());
    }

    // See comment for |bundle_events| above. We also enforce an upper bound on
    // how many submessages we'll add for a given TracePacket so they won't grow
    // infinitely.
    if (!bundle_events ||
        current_eventcount_for_message_++ > kMaxEventsPerMessage) {
      event_bundle_ = ChromeEventBundleHandle();
      trace_packet_handle_ = perfetto::TraceWriter::TracePacketHandle();
    }
  }

  void UpdateDuration(base::trace_event::TraceEventHandle handle,
                      const base::TimeTicks& now,
                      const base::ThreadTicks& thread_now) {
    if (!handle.event_index || handle.chunk_index != kMagicChunkIndex ||
        handle.chunk_seq != session_id_) {
      return;
    }

    DCHECK_EQ(handle.event_index, current_stack_depth_);
    DCHECK_GE(current_stack_depth_, 1u);
    current_stack_depth_--;
    complete_event_stack_[current_stack_depth_].UpdateDuration(now, thread_now);
    AddTraceEvent(&complete_event_stack_[current_stack_depth_], nullptr);

#if defined(OS_ANDROID)
    complete_event_stack_[current_stack_depth_].SendToATrace();
#endif
  }

  void Flush() {
    // TODO(oysteine): This will break events if we flush
    // while recording. This can't be done on destruction
    // as this can trigger PostTasks which may not be possible
    // if the thread is being shut down.
    while (current_stack_depth_--) {
      AddTraceEvent(&complete_event_stack_[current_stack_depth_], nullptr);
    }

    event_bundle_ = ChromeEventBundleHandle();
    trace_packet_handle_ = perfetto::TraceWriter::TracePacketHandle();
    trace_writer_->Flush();
  }

  uint32_t session_id() const { return session_id_; }

 private:
  std::unique_ptr<perfetto::StartupTraceWriter> trace_writer_;
  uint32_t session_id_;
  const bool thread_will_flush_;
  ChromeEventBundleHandle event_bundle_;
  perfetto::TraceWriter::TracePacketHandle trace_packet_handle_;
  std::map<intptr_t, int> string_table_;
  int next_string_table_index_ = 0;
  size_t current_eventcount_for_message_ = 0;
  TraceEvent complete_event_stack_[kMaxCompleteEventDepth];
  uint32_t current_stack_depth_ = 0;
};

namespace {

base::ThreadLocalStorage::Slot* ThreadLocalEventSinkSlot() {
  static base::NoDestructor<base::ThreadLocalStorage::Slot>
      thread_local_event_sink_tls([](void* event_sink) {
        delete static_cast<TraceEventDataSource::ThreadLocalEventSink*>(
            event_sink);
      });

  return thread_local_event_sink_tls.get();
}

}  // namespace

// static
TraceEventDataSource* TraceEventDataSource::GetInstance() {
  static base::NoDestructor<TraceEventDataSource> instance;
  return instance.get();
}

TraceEventDataSource::TraceEventDataSource()
    : DataSourceBase(mojom::kTraceEventDataSourceName) {}

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
    DCHECK(!startup_writer_registry_ && !producer_client_);
    startup_writer_registry_ =
        std::make_unique<perfetto::StartupTraceWriterRegistry>();
  }
  RegisterWithTraceLog();
}

void TraceEventDataSource::StartTracing(
    ProducerClient* producer_client,
    const perfetto::DataSourceConfig& data_source_config) {
  std::unique_ptr<perfetto::StartupTraceWriterRegistry> unbound_writer_registry;
  {
    base::AutoLock lock(lock_);

    DCHECK(!producer_client_);
    producer_client_ = producer_client;
    target_buffer_ = data_source_config.target_buffer();
    // Reduce lock contention by binding the registry without holding the lock.
    unbound_writer_registry = std::move(startup_writer_registry_);
  }

  session_id_.fetch_add(1u, std::memory_order_relaxed);

  if (unbound_writer_registry) {
    producer_client->BindStartupTraceWriterRegistry(
        std::move(unbound_writer_registry), data_source_config.target_buffer());
  } else {
    RegisterWithTraceLog();
  }

  auto trace_config =
      TraceConfig(data_source_config.chrome_config().trace_config());
  TraceLog::GetInstance()->SetEnabled(trace_config, TraceLog::RECORDING_MODE);
  ResetHistograms(trace_config);
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

TraceEventDataSource::ThreadLocalEventSink*
TraceEventDataSource::CreateThreadLocalEventSink(bool thread_will_flush) {
  base::AutoLock lock(lock_);
  // |startup_writer_registry_| only exists during startup tracing before we
  // connect to the service. |producer_client_| is reset when tracing is
  // stopped.
  if (startup_writer_registry_) {
    return new ThreadLocalEventSink(
        startup_writer_registry_->CreateUnboundTraceWriter(),
        session_id_.load(std::memory_order_relaxed), thread_will_flush);
  } else if (producer_client_) {
    return new ThreadLocalEventSink(
        std::make_unique<perfetto::StartupTraceWriter>(
            producer_client_->CreateTraceWriter(target_buffer_)),
        session_id_.load(std::memory_order_relaxed), thread_will_flush);
  } else {
    return nullptr;
  }
}

// static
void TraceEventDataSource::OnAddTraceEvent(
    TraceEvent* trace_event,
    bool thread_will_flush,
    base::trace_event::TraceEventHandle* handle) {
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
    if (new_session_id > kFirstSessionID &&
        new_session_id != thread_local_event_sink->session_id()) {
      delete thread_local_event_sink;
      thread_local_event_sink = nullptr;
    }
  }

  if (!thread_local_event_sink) {
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
  if (startup_writer_registry_) {
    // If the writer is still unbound, the registry will keep it alive until it
    // was bound and its buffered data was copied. This ensures that we don't
    // lose data from threads that are shut down during startup.
    startup_writer_registry_->ReturnUnboundTraceWriter(std::move(trace_writer));
  } else {
    // Delete the TraceWriter on the sequence that Perfetto runs on, needed
    // as the ThreadLocalEventSink gets deleted on thread
    // shutdown and we can't safely call TaskRunnerHandle::Get() at that point
    // (which can happen as the TraceWriter destructor might make a Mojo call
    // and trigger it).
    ProducerClient::GetTaskRunner()->DeleteSoon(FROM_HERE,
                                                std::move(trace_writer));
  }
}

}  // namespace tracing
