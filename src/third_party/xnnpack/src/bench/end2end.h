// Copyright 2019 Google LLC
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include "models/models.h"
#include <benchmark/benchmark.h>


#define BENCHMARK_FP32_END2END(benchmark_fn) \
  BENCHMARK_CAPTURE(benchmark_fn, mobilenet_v1, models::FP32MobileNetV1)->Unit(benchmark::kMicrosecond)->UseRealTime(); \
  BENCHMARK_CAPTURE(benchmark_fn, mobilenet_v2, models::FP32MobileNetV2)->Unit(benchmark::kMicrosecond)->UseRealTime(); \
  BENCHMARK_CAPTURE(benchmark_fn, mobilenet_v3_large, models::FP32MobileNetV3Large)->Unit(benchmark::kMicrosecond)->UseRealTime(); \
  BENCHMARK_CAPTURE(benchmark_fn, mobilenet_v3_small, models::FP32MobileNetV3Small)->Unit(benchmark::kMicrosecond)->UseRealTime();

#define BENCHMARK_QS8_END2END(benchmark_fn) \
  BENCHMARK_CAPTURE(benchmark_fn, mobilenet_v1, models::QS8MobileNetV1)->Unit(benchmark::kMicrosecond)->UseRealTime(); \
  BENCHMARK_CAPTURE(benchmark_fn, mobilenet_v2, models::QS8MobileNetV2)->Unit(benchmark::kMicrosecond)->UseRealTime();

#define BENCHMARK_QU8_END2END(benchmark_fn) \
  BENCHMARK_CAPTURE(benchmark_fn, mobilenet_v1, models::QU8MobileNetV1)->Unit(benchmark::kMicrosecond)->UseRealTime(); \
  BENCHMARK_CAPTURE(benchmark_fn, mobilenet_v2, models::QU8MobileNetV2)->Unit(benchmark::kMicrosecond)->UseRealTime();
