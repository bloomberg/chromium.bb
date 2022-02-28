// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/content/content_tracing_manager.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_config.h"
#include "components/feedback/feedback_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tracing_controller.h"

namespace {

// Only once trace manager can exist at a time.
ContentTracingManager* g_tracing_manager = nullptr;
// Trace IDs start at 1 and increase.
int g_next_trace_id = 1;
// Name of the file to store the tracing data as.
const base::FilePath::CharType kTracingFilename[] =
    FILE_PATH_LITERAL("tracing.json");
}  // namespace

ContentTracingManager::ContentTracingManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!g_tracing_manager);
  g_tracing_manager = this;
  StartTracing();
}

ContentTracingManager::~ContentTracingManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(g_tracing_manager == this);
  g_tracing_manager = nullptr;
}

int ContentTracingManager::RequestTrace() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Return the current trace if one is being collected.
  if (current_trace_id_)
    return current_trace_id_;

  current_trace_id_ = g_next_trace_id;
  ++g_next_trace_id;
  content::TracingController::GetInstance()->StopTracing(
      content::TracingController::CreateStringEndpoint(
          base::BindOnce(&ContentTracingManager::OnTraceDataCollected,
                         weak_ptr_factory_.GetWeakPtr())));
  return current_trace_id_;
}

bool ContentTracingManager::GetTraceData(int id, TraceDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // If a trace is being collected currently, send it via callback when
  // complete.
  if (current_trace_id_) {
    // Only allow one trace data request at a time.
    if (!trace_callback_) {
      trace_callback_ = std::move(callback);
      return true;
    } else {
      return false;
    }
  } else {
    auto data = trace_data_.find(id);
    if (data == trace_data_.end())
      return false;

    // Always return the data asynchronously, so the behavior is consistent.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), data->second));
    return true;
  }
}

void ContentTracingManager::DiscardTraceData(int id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  trace_data_.erase(id);

  // If the trace is discarded before it is complete, clean up the accumulators.
  if (id == current_trace_id_) {
    current_trace_id_ = 0;

    // If the trace has already been requested, provide an empty string.
    if (trace_callback_)
      std::move(trace_callback_).Run(scoped_refptr<base::RefCountedString>());
  }
}

void ContentTracingManager::StartTracing() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::TracingController::GetInstance()->StartTracing(
      base::trace_event::TraceConfig(),
      content::TracingController::StartTracingDoneCallback());
}

void ContentTracingManager::OnTraceDataCollected(
    std::unique_ptr<std::string> trace_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!current_trace_id_)
    return;

  std::string output_val;
  feedback_util::ZipString(base::FilePath(kTracingFilename), *trace_data,
                           &output_val);

  scoped_refptr<base::RefCountedString> output(
      base::RefCountedString::TakeString(&output_val));

  trace_data_[current_trace_id_] = output;

  if (trace_callback_)
    std::move(trace_callback_).Run(output);

  current_trace_id_ = 0;

  // Tracing has to be restarted asynchronous, so the ContentTracingManager can
  // clean up.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ContentTracingManager::StartTracing,
                                weak_ptr_factory_.GetWeakPtr()));
}

// static
std::unique_ptr<ContentTracingManager> ContentTracingManager::Create() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_tracing_manager)
    return nullptr;
  return base::WrapUnique(new ContentTracingManager);
}

ContentTracingManager* ContentTracingManager::Get() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return g_tracing_manager;
}
