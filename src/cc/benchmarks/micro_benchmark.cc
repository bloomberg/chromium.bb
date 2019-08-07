// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/benchmarks/micro_benchmark.h"

#include <memory>

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/values.h"
#include "cc/benchmarks/micro_benchmark_impl.h"

namespace cc {

MicroBenchmark::MicroBenchmark(DoneCallback callback)
    : callback_(std::move(callback)),
      is_done_(false),
      processed_for_benchmark_impl_(false),
      id_(0) {}

MicroBenchmark::~MicroBenchmark() = default;

bool MicroBenchmark::IsDone() const {
  return is_done_;
}

void MicroBenchmark::DidUpdateLayers(LayerTreeHost* layer_tree_host) {}

void MicroBenchmark::NotifyDone(std::unique_ptr<base::Value> result) {
  std::move(callback_).Run(std::move(result));
  is_done_ = true;
}

void MicroBenchmark::RunOnLayer(PictureLayer* layer) {}

bool MicroBenchmark::ProcessMessage(std::unique_ptr<base::Value> value) {
  return false;
}

bool MicroBenchmark::ProcessedForBenchmarkImpl() const {
  return processed_for_benchmark_impl_;
}

std::unique_ptr<MicroBenchmarkImpl> MicroBenchmark::GetBenchmarkImpl(
    scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner) {
  DCHECK(!processed_for_benchmark_impl_);
  processed_for_benchmark_impl_ = true;
  return CreateBenchmarkImpl(origin_task_runner);
}

std::unique_ptr<MicroBenchmarkImpl> MicroBenchmark::CreateBenchmarkImpl(
    scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner) {
  return base::WrapUnique<MicroBenchmarkImpl>(nullptr);
}

}  // namespace cc
