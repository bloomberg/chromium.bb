/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include "tensorflow/lite/profiling/memory_usage_monitor.h"

#include "absl/synchronization/notification.h"
#include "tensorflow/lite/minimal_logging.h"
#include "tensorflow/lite/profiling/memory_info.h"

namespace tflite {
namespace profiling {
namespace memory {

constexpr float MemoryUsageMonitor::kInvalidMemUsageMB;

MemoryUsageMonitor::MemoryUsageMonitor(int sampling_interval_ms,
                                       std::unique_ptr<Sampler> sampler)
    : sampler_(std::move(sampler)),
      is_supported_(false),
      sampling_interval_(absl::Milliseconds(sampling_interval_ms)) {
  is_supported_ = (sampler_ != nullptr && sampler_->IsSupported());
  if (!is_supported_) {
    TFLITE_LOG(TFLITE_LOG_INFO,
               "Getting memory usage isn't supported on this platform!\n");
    return;
  }
}

void MemoryUsageMonitor::Start() {
  if (!is_supported_) return;
  if (check_memory_thd_ != nullptr) {
    TFLITE_LOG(TFLITE_LOG_INFO, "Memory monitoring has already started!\n");
    return;
  }

  stop_signal_.reset(new absl::Notification());
  check_memory_thd_.reset(new std::thread(([this]() {
    // Note we retrieve the memory usage at the very beginning of the thread.
    while (true) {
      const auto mem_info = sampler_->GetMemoryUsage();
      if (mem_info.max_rss_kb > peak_max_rss_kb_) {
        peak_max_rss_kb_ = mem_info.max_rss_kb;
      }
      if (stop_signal_->HasBeenNotified()) break;
      sampler_->SleepFor(sampling_interval_);
    }
  })));
}

void MemoryUsageMonitor::Stop() {
  if (!is_supported_) return;
  if (check_memory_thd_ == nullptr) {
    TFLITE_LOG(TFLITE_LOG_INFO,
               "Memory monitoring hasn't started yet or has stopped!\n");
    return;
  }
  StopInternal();
}

void MemoryUsageMonitor::StopInternal() {
  if (check_memory_thd_ == nullptr) return;
  stop_signal_->Notify();
  if (check_memory_thd_ != nullptr) {
    check_memory_thd_->join();
  }
  stop_signal_.reset(nullptr);
  check_memory_thd_.reset(nullptr);
}

}  // namespace memory
}  // namespace profiling
}  // namespace tflite
