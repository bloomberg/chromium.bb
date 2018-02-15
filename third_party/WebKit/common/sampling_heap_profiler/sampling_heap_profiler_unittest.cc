// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/WebKit/public/common/sampling_heap_profiler/sampling_heap_profiler.h"

#include <memory>

#include "base/debug/stack_trace.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

class SamplingHeapProfilerTest : public ::testing::Test {};

class SamplesCollector : public SamplingHeapProfiler::SamplesObserver {
 public:
  explicit SamplesCollector(size_t watch_size) : watch_size_(watch_size) {}

  void SampleAdded(uint32_t id, size_t size, size_t count) override {
    if (size < watch_size_ || !count)
      return;
    sample_id_ = id;
    sample_added = true;
  }

  void SampleRemoved(uint32_t id) override {
    if (id == sample_id_)
      sample_removed = true;
  }

  bool sample_added = false;
  bool sample_removed = false;

 private:
  size_t watch_size_;
  uint32_t sample_id_ = 0;
};

TEST_F(SamplingHeapProfilerTest, CollectSamples) {
  SamplesCollector collector(sizeof(testing::Message));
  SamplingHeapProfiler* profiler = SamplingHeapProfiler::GetInstance();
  profiler->SuppressRandomnessForTest();
  profiler->SetSamplingInterval(256);
  profiler->Start();
  profiler->AddSamplesObserver(&collector);
  std::unique_ptr<testing::Message> data(new testing::Message());
  data.reset();
  profiler->Stop();
  profiler->RemoveSamplesObserver(&collector);
  CHECK(collector.sample_added);
  CHECK(collector.sample_removed);
}

}  // namespace
}  // namespace blink
