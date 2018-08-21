// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/oom_intervention_impl.h"

#include "base/metrics/histogram_macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/controller/crash_memory_metrics_reporter_impl.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"

namespace blink {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class RendererInterventionEnabledStatus {
  kDetectionOnlyEnabled = 0,
  kTriggerEnabled = 1,
  kDisabledFailedMemoryMetricsFetch = 2,
  kMaxValue = kDisabledFailedMemoryMetricsFetch
};

void RecordEnabledStatus(RendererInterventionEnabledStatus status) {
  UMA_HISTOGRAM_ENUMERATION(
      "Memory.Experimental.OomIntervention.RendererEnabledStatus", status);
}

}  // namespace

// static
void OomInterventionImpl::Create(mojom::blink::OomInterventionRequest request) {
  mojo::MakeStrongBinding(std::make_unique<OomInterventionImpl>(),
                          std::move(request));
}

OomInterventionImpl::OomInterventionImpl()
    : timer_(Platform::Current()->MainThread()->GetTaskRunner(),
             this,
             &OomInterventionImpl::Check) {}

OomInterventionImpl::~OomInterventionImpl() {}

void OomInterventionImpl::StartDetection(
    mojom::blink::OomInterventionHostPtr host,
    mojom::blink::DetectionArgsPtr detection_args,
    bool trigger_intervention) {
  host_ = std::move(host);

  // Disable intervention if we cannot get memory details of current process.
  if (CrashMemoryMetricsReporterImpl::Instance().ResetFileDiscriptors()) {
    RecordEnabledStatus(
        RendererInterventionEnabledStatus::kDisabledFailedMemoryMetricsFetch);
    return;
  }

  RecordEnabledStatus(
      trigger_intervention
          ? RendererInterventionEnabledStatus::kTriggerEnabled
          : RendererInterventionEnabledStatus::kDetectionOnlyEnabled);

  detection_args_ = std::move(detection_args);
  trigger_intervention_ = trigger_intervention;

  timer_.Start(TimeDelta(), TimeDelta::FromSeconds(1), FROM_HERE);
}

OomInterventionMetrics OomInterventionImpl::GetCurrentMemoryMetrics() {
  return CrashMemoryMetricsReporterImpl::Instance().GetCurrentMemoryMetrics();
}

void OomInterventionImpl::Check(TimerBase*) {
  DCHECK(host_);

  OomInterventionMetrics current_memory = GetCurrentMemoryMetrics();
  bool oom_detected = false;

  oom_detected |= detection_args_->blink_workload_threshold > 0 &&
                  current_memory.current_blink_usage_kb * 1024 >
                      detection_args_->blink_workload_threshold;
  oom_detected |= detection_args_->private_footprint_threshold > 0 &&
                  current_memory.current_private_footprint_kb * 1024 >
                      detection_args_->private_footprint_threshold;
  oom_detected |=
      detection_args_->swap_threshold > 0 &&
      current_memory.current_swap_kb * 1024 > detection_args_->swap_threshold;
  oom_detected |= detection_args_->virtual_memory_thresold > 0 &&
                  current_memory.current_vm_size_kb * 1024 >
                      detection_args_->virtual_memory_thresold;

  if (oom_detected) {
    host_->OnHighMemoryUsage(trigger_intervention_);

    if (trigger_intervention_) {
      // The ScopedPagePauser is destroyed when the intervention is declined and
      // mojo strong binding is disconnected.
      pauser_.reset(new ScopedPagePauser);
    }
    timer_.Stop();
  }
  CrashMemoryMetricsReporterImpl::Instance().WriteIntoSharedMemory(
      current_memory);
}

}  // namespace blink
