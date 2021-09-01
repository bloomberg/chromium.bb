/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

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

#ifndef TENSORFLOW_CORE_PROFILER_CONVERT_OP_METRICS_TO_RECORD_H_
#define TENSORFLOW_CORE_PROFILER_CONVERT_OP_METRICS_TO_RECORD_H_

#include <vector>

#include "tensorflow/core/profiler/protobuf/op_metrics.pb.h"
#include "tensorflow/core/profiler/utils/math_utils.h"
#include "tensorflow/core/profiler/utils/time_utils.h"

namespace tensorflow {
namespace profiler {

std::vector<const OpMetrics*> SortedOpMetricsDb(const OpMetricsDb& metrics_db,
                                                int max_records = -1);

template <typename Record>
inline void SetExecutionTimes(const OpMetrics& metrics, Record* record) {
  record->set_occurrences(metrics.occurrences());
  record->set_total_time_in_us(PicosToMicros(metrics.time_ps()));
  record->set_avg_time_in_us(
      SafeDivide(record->total_time_in_us(), metrics.occurrences()));
  record->set_total_self_time_in_us(PicosToMicros(metrics.self_time_ps()));
  record->set_avg_self_time_in_us(
      SafeDivide(record->total_self_time_in_us(), metrics.occurrences()));
}

template <typename Record>
inline void SetTpuUnitFractions(const OpMetrics& metrics, Record* record) {
  record->set_dma_stall_fraction(
      SafeDivide(metrics.dma_stall_ps(), metrics.time_ps()));
}

template <typename Record>
inline void SetRankAndTimeFractions(double total_time_us,
                                    const Record& prev_record, Record* record) {
  record->set_rank(prev_record.rank() + 1);
  record->set_total_self_time_as_fraction(
      SafeDivide(record->total_self_time_in_us(), total_time_us));
  record->set_cumulative_total_self_time_as_fraction(
      prev_record.cumulative_total_self_time_as_fraction() +
      record->total_self_time_as_fraction());
}

template <typename Record>
inline void SetRankAndDeviceTimeFractions(double total_time_us,
                                          const Record& prev_record,
                                          Record* record) {
  record->set_rank(prev_record.rank() + 1);
  record->set_device_total_self_time_as_fraction(
      SafeDivide(record->total_self_time_in_us(), total_time_us));
  record->set_device_cumulative_total_self_time_as_fraction(
      prev_record.device_cumulative_total_self_time_as_fraction() +
      record->device_total_self_time_as_fraction());
}

template <typename Record>
inline void SetRankAndHostTimeFractions(double total_time_us,
                                        const Record& prev_record,
                                        Record* record) {
  record->set_rank(prev_record.rank() + 1);
  record->set_host_total_self_time_as_fraction(
      SafeDivide(record->total_self_time_in_us(), total_time_us));
  record->set_host_cumulative_total_self_time_as_fraction(
      prev_record.host_cumulative_total_self_time_as_fraction() +
      record->host_total_self_time_as_fraction());
}

template <typename Record>
inline void SetRooflineMetrics(const OpMetrics& metrics,
                               double ridge_point_operational_intensity,
                               Record* record) {
  using ::tensorflow::profiler::PicosToNanos;
  record->set_measured_flop_rate(
      SafeDivide(metrics.flops(), PicosToNanos(metrics.time_ps())));
  record->set_measured_memory_bw(
      SafeDivide(metrics.bytes_accessed(), PicosToNanos(metrics.time_ps())));
  record->set_operational_intensity(
      SafeDivide(metrics.flops(), metrics.bytes_accessed()));
  record->set_bound_by((metrics.bytes_accessed() != 0)
                           ? ((record->operational_intensity() >=
                               ridge_point_operational_intensity)
                                  ? "Compute"
                                  : "Memory")
                           : ((metrics.flops() != 0) ? "Compute" : "Unknown"));
}

}  // namespace profiler
}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_PROFILER_CONVERT_OP_METRICS_TO_RECORD_H_
