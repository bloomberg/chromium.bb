// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"

#include <atomic>
#include <map>
#include <utility>

#include "base/json/json_writer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/no_destructor.h"
#include "base/process/process_handle.h"
#include "base/trace_event/trace_buffer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "services/tracing/public/cpp/perfetto/traced_value_proto_writer.h"
#include "services/tracing/public/mojom/constants.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/core/shared_memory_arbiter.h"
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
  base::AutoLock lock(lock_);
  generator_functions_.push_back(generator);
}

void TraceEventMetadataSource::GenerateMetadata(
    std::unique_ptr<perfetto::TraceWriter> trace_writer) {
  DCHECK(origin_task_runner_->RunsTasksInCurrentSequence());

  auto trace_packet = trace_writer->NewTracePacket();
  ChromeEventBundleHandle event_bundle(trace_packet->set_chrome_events());

  base::AutoLock lock(lock_);
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
    const mojom::DataSourceConfig& data_source_config) {
  origin_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TraceEventMetadataSource::GenerateMetadata,
                                base::Unretained(this),
                                producer_client->CreateTraceWriter(
                                    data_source_config.target_buffer)));
}

void TraceEventMetadataSource::StopTracing(
    base::OnceClosure stop_complete_callback) {
  // We bounce a task off the origin_task_runner_ that the generator
  // callbacks are run from, to make sure that GenerateMetaData() has finished
  // running.
  origin_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                        std::move(stop_complete_callback));
}

void TraceEventMetadataSource::Flush(
    base::RepeatingClosure flush_complete_callback) {
  origin_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                        std::move(flush_complete_callback));
}

class TraceEventDataSource::ThreadLocalEventSink {
 public:
  ThreadLocalEventSink(std::unique_ptr<perfetto::TraceWriter> trace_writer,
                       bool thread_will_flush)
      : trace_writer_(std::move(trace_writer)),
        thread_will_flush_(thread_will_flush) {
#if DCHECK_IS_ON()
    static std::atomic<int32_t> id_counter(1);
    sink_id_ = id_counter.fetch_add(1, std::memory_order_relaxed);
#endif  // DCHECK_IS_ON()
  }

  ~ThreadLocalEventSink() {
    // Finalize the current message before posting the |trace_writer_| for
    // destruction, to avoid data races.
    event_bundle_ = ChromeEventBundleHandle();
    trace_packet_handle_ = perfetto::TraceWriter::TracePacketHandle();

    // Delete the TraceWriter on the sequence that Perfetto runs on, needed
    // as the ThreadLocalEventSink gets deleted on thread
    // shutdown and we can't safely call TaskRunnerHandle::Get() at that point
    // (which can happen as the TraceWriter destructor might make a Mojo call
    // and trigger it).
    ProducerClient::GetTaskRunner()->DeleteSoon(FROM_HERE,
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

    // TODO(oysteine): Temporary workaround for a specific trace event
    // which is added while a scheduler lock is held, and will deadlock
    // if Perfetto does a PostTask to commit a finished chunk.
    if (strcmp(trace_event->name(), "RealTimeDomain::DelayTillNextTask") == 0) {
      return;
    }

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

#if DCHECK_IS_ON()
      handle->chunk_seq = sink_id_;
#endif  // DCHECK_IS_ON()

      complete_event_stack_[current_stack_depth_] = std::move(*trace_event);
      handle->event_index = ++current_stack_depth_;
      handle->chunk_index = kMagicChunkIndex;
      return;
    }

    EnsureValidHandles();

    int name_index = 0;
    int category_name_index = 0;
    int arg_name_indices[base::trace_event::kTraceMaxNumArgs] = {0};

    // Populate any new string table parts first; has to be done before
    // the add_trace_events() call (as the string table is part of the outer
    // proto message).
    // If the TRACE_EVENT_FLAG_COPY flag is set, the char* pointers aren't
    // necessarily valid after the TRACE_EVENT* call, and so we need to store
    // the string every time.
    bool string_table_enabled =
        !(trace_event->flags() & TRACE_EVENT_FLAG_COPY) && thread_will_flush_;
    if (string_table_enabled) {
      name_index = GetStringTableIndexForString(trace_event->name());
      category_name_index =
          GetStringTableIndexForString(TraceLog::GetCategoryGroupName(
              trace_event->category_group_enabled()));

      for (int i = 0;
           i < base::trace_event::kTraceMaxNumArgs && trace_event->arg_name(i);
           ++i) {
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

    for (int i = 0;
         i < base::trace_event::kTraceMaxNumArgs && trace_event->arg_name(i);
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

    // If we know that the current thread will never send a Flush message
    // (meaning it's a thread without a messageloop that TraceLog knows about),
    // we need to finalize the packet right away so Perfetto can recover it.
    // We also enforce an upper bound on how many submessages we'll add
    // for a given TracePacket so they won't grow infinitely.
    if (!thread_will_flush_ ||
        current_eventcount_for_message_++ > kMaxEventsPerMessage) {
      event_bundle_ = ChromeEventBundleHandle();
      trace_packet_handle_ = perfetto::TraceWriter::TracePacketHandle();
    }
  }

  void UpdateDuration(base::trace_event::TraceEventHandle handle,
                      const base::TimeTicks& now,
                      const base::ThreadTicks& thread_now) {
    if (!handle.event_index || handle.chunk_index != kMagicChunkIndex) {
      return;
    }

#if DCHECK_IS_ON()
    DCHECK_EQ(handle.chunk_seq, sink_id_);
#endif  // DCHECK_IS_ON()

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

 private:
  std::unique_ptr<perfetto::TraceWriter> trace_writer_;
  const bool thread_will_flush_;
  ChromeEventBundleHandle event_bundle_;
  perfetto::TraceWriter::TracePacketHandle trace_packet_handle_;
  std::map<intptr_t, int> string_table_;
  int next_string_table_index_ = 0;
  size_t current_eventcount_for_message_ = 0;
  TraceEvent complete_event_stack_[kMaxCompleteEventDepth];
  uint32_t current_stack_depth_ = 0;
#if DCHECK_IS_ON()
  uint32_t sink_id_;
#endif  // DCHECK_IS_ON()
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
    : DataSourceBase(mojom::kTraceEventDataSourceName) {
}

TraceEventDataSource::~TraceEventDataSource() = default;

void TraceEventDataSource::StartTracing(
    ProducerClient* producer_client,
    const mojom::DataSourceConfig& data_source_config) {
  {
    base::AutoLock lock(lock_);

    DCHECK(!producer_client_);
    producer_client_ = producer_client;
    target_buffer_ = data_source_config.target_buffer;
  }

  RegisterTracedValueProtoWriter(true);

  TraceLog::GetInstance()->SetAddTraceEventOverrides(
      &TraceEventDataSource::OnAddTraceEvent,
      &TraceEventDataSource::FlushCurrentThread,
      &TraceEventDataSource::OnUpdateDuration);

  TraceLog::GetInstance()->SetEnabled(
      TraceConfig(data_source_config.trace_config), TraceLog::RECORDING_MODE);
}

void TraceEventDataSource::StopTracing(
    base::OnceClosure stop_complete_callback) {
  stop_complete_callback_ = std::move(stop_complete_callback);

  auto on_tracing_stopped_callback =
      [](TraceEventDataSource* data_source,
         const scoped_refptr<base::RefCountedString>&, bool has_more_events) {
        if (has_more_events) {
          return;
        }

        RegisterTracedValueProtoWriter(false);
        TraceLog::GetInstance()->SetAddTraceEventOverrides(nullptr, nullptr,
                                                           nullptr);

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

  if (producer_client_) {
    return new ThreadLocalEventSink(
        producer_client_->CreateTraceWriter(target_buffer_), thread_will_flush);
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

}  // namespace tracing
