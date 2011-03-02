// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/tracer.h"

#include <list>

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "base/rand_util.h"
#include "base/ref_counted.h"
#include "base/stl_util-inl.h"
#include "base/synchronization/condition_variable.h"
#include "base/threading/thread.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_local.h"
#include "base/time.h"

namespace remoting {

namespace {

class OutputLogger {
 public:
  OutputLogger()
      : thread_("logging_thread"),
        stopped_(false),
        wake_(&lock_) {
    thread_.Start();
    thread_.message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &OutputLogger::PrintLogs));
  }

  void OutputTrace(TraceBuffer* buffer) {
    scoped_ptr<TraceBuffer> buffer_ref_(buffer);
    base::AutoLock l(lock_);

    // Drop messages if we're overwhelming the logger.
    if (buffers_.size() < 10) {
      buffers_.push_front(buffer_ref_.release());
      wake_.Signal();
    } else {
      // TODO(ajwong): Remove this cause it just overwhelms the logging more.
      LOG(WARNING) << "Message dropped.";
    }
  }

  void LogOneTrace(TraceBuffer* buffer) {
    VLOG(1) << "Trace: " << buffer->name();
    base::Time last_timestamp;
    for (int i = 0; i < buffer->record_size(); ++i) {
      const TraceRecord& record = buffer->record(i);
      base::Time timestamp = base::Time::FromInternalValue(record.timestamp());
      if (i == 0) {
        VLOG(1) << "  TS: " << record.timestamp()
                << " msg: " << record.annotation();
      } else {
        base::TimeDelta delta = timestamp - last_timestamp;
        VLOG(1) << "  TS: " << record.timestamp()
                << " msg: " << record.annotation()
                << " [ " << delta.InMilliseconds() << "ms ]";
      }
      last_timestamp = timestamp;
    }
  }

  void PrintLogs() {
    while(!stopped_) {
      TraceBuffer* buffer = NULL;
      {
        base::AutoLock l(lock_);
        if (buffers_.empty()) {
          wake_.Wait();
        }
        // Check again since we might have woken for a stop signal.
        if (buffers_.size() != 0) {
          buffer = buffers_.back();
          buffers_.pop_back();
        }
      }

      if (buffer) {
        LogOneTrace(buffer);
        delete buffer;
      }
    }
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<OutputLogger>;

  ~OutputLogger() {
    {
      base::AutoLock l(lock_);
      stopped_ = true;
      wake_.Signal();
    }

    thread_.Stop();
    STLDeleteElements(&buffers_);
  }

  base::Lock lock_;
  base::Thread thread_;
  bool stopped_;
  base::ConditionVariable wake_;
  std::list<TraceBuffer*> buffers_;
};

static base::LazyInstance<OutputLogger> g_output_logger(
    base::LINKER_INITIALIZED);
static base::LazyInstance<base::ThreadLocalPointer<TraceContext> >
    g_thread_local_trace_context(base::LINKER_INITIALIZED);

}  // namespace

Tracer::Tracer(const std::string& name, double sample_percent) {
  if (sample_percent > base::RandDouble()) {
    buffer_.reset(new TraceBuffer());
    buffer_->set_name(name);
  }
}

void Tracer::PrintString(const std::string& s) {
  base::AutoLock l(lock_);
  if (!buffer_.get()) {
    return;
  }

  TraceRecord* record = buffer_->add_record();
  record->set_annotation(s);
  record->set_timestamp(base::Time::Now().ToInternalValue());

  // Take the pointer for the current messageloop as identifying for the
  // current thread.
  record->set_thread_id(static_cast<uint64>(base::PlatformThread::CurrentId()));
}

Tracer::~Tracer() {
  base::AutoLock l(lock_);

  if (buffer_.get()) {
    g_output_logger.Get().OutputTrace(buffer_.release());
  }
}

// static
Tracer* TraceContext::tracer() {
  return Get()->GetTracerInternal();
}

// static
void TraceContext::PushTracer(Tracer* tracer) {
  Get()->PushTracerInternal(tracer);
}

// static
void TraceContext::PopTracer() {
  Get()->PopTracerInternal();
}

// static
TraceContext* TraceContext::Get() {
  TraceContext* context =
      g_thread_local_trace_context.Get().Get();
  if (context == NULL) {
    context = new TraceContext();
    context->PushTracerInternal(new Tracer("default", 0.0));
    g_thread_local_trace_context.Get().Set(context);
  }
  return context;
}

TraceContext::TraceContext() {}

TraceContext::~TraceContext() {}

void TraceContext::PushTracerInternal(Tracer* tracer) {
  tracers_.push_back(make_scoped_refptr(tracer));
}

void TraceContext::PopTracerInternal() { tracers_.pop_back(); }

Tracer* TraceContext::GetTracerInternal() { return tracers_.back(); }


}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::OutputLogger);
