// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/oom_intervention_impl.h"

#include <unistd.h>

#include "base/files/file_util.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/common/oom_intervention/oom_intervention_types.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

const uint64_t kTestBlinkThreshold = 80 * 1024;
const uint64_t kTestPMFThreshold = 160 * 1024;
const uint64_t kTestSwapThreshold = 500 * 1024;

class MockOomInterventionHost : public mojom::blink::OomInterventionHost {
 public:
  MockOomInterventionHost(mojom::blink::OomInterventionHostRequest request)
      : binding_(this, std::move(request)) {}
  ~MockOomInterventionHost() override = default;

  void OnHighMemoryUsage(bool intervention_triggered) override {}

 private:
  mojo::Binding<mojom::blink::OomInterventionHost> binding_;
};

// Mock intervention class that has custom method for fetching metrics.
class MockOomInterventionImpl : public OomInterventionImpl {
 public:
  MockOomInterventionImpl() {}
  ~MockOomInterventionImpl() override {}

  // If metrics are set by calling this method, then GetCurrentMemoryMetrics()
  // will return the given metrics, else it will calculate metrics from
  // providers.
  void SetMetrics(OomInterventionMetrics metrics) {
    metrics_ = std::make_unique<OomInterventionMetrics>();
    *metrics_ = metrics;
  }

 private:
  OomInterventionMetrics GetCurrentMemoryMetrics() override {
    if (metrics_)
      return *metrics_;
    return OomInterventionImpl::GetCurrentMemoryMetrics();
  }

  std::unique_ptr<OomInterventionMetrics> metrics_;
};

}  // namespace

class OomInterventionImplTest : public testing::Test {
 public:
  void SetUp() override {
    intervention_ = std::make_unique<MockOomInterventionImpl>();
  }

  Page* DetectOnceOnBlankPage() {
    WebViewImpl* web_view = web_view_helper_.InitializeAndLoad("about::blank");
    Page* page = web_view->MainFrameImpl()->GetFrame()->GetPage();
    EXPECT_FALSE(page->Paused());
    mojom::blink::OomInterventionHostPtr host_ptr;
    MockOomInterventionHost mock_host(mojo::MakeRequest(&host_ptr));
    base::UnsafeSharedMemoryRegion shm =
        base::UnsafeSharedMemoryRegion::Create(sizeof(OomInterventionMetrics));

    mojom::blink::DetectionArgsPtr args(mojom::blink::DetectionArgs::New());
    args->blink_workload_threshold = kTestBlinkThreshold;
    args->private_footprint_threshold = kTestPMFThreshold;
    args->swap_threshold = kTestSwapThreshold;

    intervention_->StartDetection(std::move(host_ptr), std::move(shm),
                                  std::move(args),
                                  true /*trigger_intervention*/);
    test::RunDelayedTasks(TimeDelta::FromSeconds(1));
    return page;
  }

 protected:
  std::unique_ptr<MockOomInterventionImpl> intervention_;
  FrameTestHelpers::WebViewHelper web_view_helper_;
};

TEST_F(OomInterventionImplTest, NoDetectionOnBelowThreshold) {
  OomInterventionMetrics mock_metrics = {};
  // Set value less than the threshold to not trigger intervention.
  mock_metrics.current_blink_usage_kb = (kTestBlinkThreshold / 1024) - 1;
  mock_metrics.current_private_footprint_kb = (kTestPMFThreshold / 1024) - 1;
  mock_metrics.current_swap_kb = (kTestSwapThreshold / 1024) - 1;
  intervention_->SetMetrics(mock_metrics);

  Page* page = DetectOnceOnBlankPage();

  EXPECT_FALSE(page->Paused());
}

TEST_F(OomInterventionImplTest, BlinkThresholdDetection) {
  OomInterventionMetrics mock_metrics = {};
  // Set value more than the threshold to not trigger intervention.
  mock_metrics.current_blink_usage_kb = (kTestBlinkThreshold / 1024) + 1;
  mock_metrics.current_private_footprint_kb = (kTestPMFThreshold / 1024) - 1;
  mock_metrics.current_swap_kb = (kTestSwapThreshold / 1024) - 1;
  intervention_->SetMetrics(mock_metrics);

  Page* page = DetectOnceOnBlankPage();

  EXPECT_TRUE(page->Paused());
  intervention_.reset();
  EXPECT_FALSE(page->Paused());
}

TEST_F(OomInterventionImplTest, PmfThresholdDetection) {
  OomInterventionMetrics mock_metrics = {};
  mock_metrics.current_blink_usage_kb = (kTestBlinkThreshold / 1024) - 1;
  // Set value more than the threshold to not trigger intervention.
  mock_metrics.current_private_footprint_kb = (kTestPMFThreshold / 1024) + 1;
  mock_metrics.current_swap_kb = (kTestSwapThreshold / 1024) - 1;
  intervention_->SetMetrics(mock_metrics);

  Page* page = DetectOnceOnBlankPage();

  EXPECT_TRUE(page->Paused());
  intervention_.reset();
  EXPECT_FALSE(page->Paused());
}

TEST_F(OomInterventionImplTest, SwapThresholdDetection) {
  OomInterventionMetrics mock_metrics = {};
  mock_metrics.current_blink_usage_kb = (kTestBlinkThreshold / 1024) - 1;
  mock_metrics.current_private_footprint_kb = (kTestPMFThreshold / 1024) - 1;
  // Set value more than the threshold to not trigger intervention.
  mock_metrics.current_swap_kb = (kTestSwapThreshold / 1024) + 1;
  intervention_->SetMetrics(mock_metrics);

  Page* page = DetectOnceOnBlankPage();

  EXPECT_TRUE(page->Paused());
  intervention_.reset();
  EXPECT_FALSE(page->Paused());
}

TEST_F(OomInterventionImplTest, CalculatePMFAndSwap) {
  const char kStatmFile[] = "100 40 25 0 0";
  const char kStatusFile[] =
      "First:  1\n Second: 2 kB\nVmSwap: 10 kB \n Third: 10 kB\n Last: 8";
  base::FilePath statm_path;
  EXPECT_TRUE(base::CreateTemporaryFile(&statm_path));
  EXPECT_EQ(static_cast<int>(sizeof(kStatmFile)),
            base::WriteFile(statm_path, kStatmFile, sizeof(kStatmFile)));
  base::File statm_file(statm_path,
                        base::File::FLAG_OPEN | base::File::FLAG_READ);
  base::FilePath status_path;
  EXPECT_TRUE(base::CreateTemporaryFile(&status_path));
  EXPECT_EQ(static_cast<int>(sizeof(kStatusFile)),
            base::WriteFile(status_path, kStatusFile, sizeof(kStatusFile)));
  base::File status_file(status_path,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);

  intervention_->statm_fd_.reset(statm_file.TakePlatformFile());
  intervention_->status_fd_.reset(status_file.TakePlatformFile());

  mojom::blink::OomInterventionHostPtr host_ptr;
  MockOomInterventionHost mock_host(mojo::MakeRequest(&host_ptr));
  base::UnsafeSharedMemoryRegion shm =
      base::UnsafeSharedMemoryRegion::Create(sizeof(OomInterventionMetrics));
  mojom::blink::DetectionArgsPtr args(mojom::blink::DetectionArgs::New());
  intervention_->StartDetection(std::move(host_ptr), std::move(shm),
                                std::move(args),
                                false /*trigger_intervention*/);

  intervention_->Check(nullptr);
  OomInterventionMetrics* metrics = static_cast<OomInterventionMetrics*>(
      intervention_->shared_metrics_buffer_.memory());
  uint64_t swap_kb = 10;
  uint64_t private_footprint_kb = (40 - 25) * getpagesize() / 1024 + swap_kb;
  EXPECT_EQ(private_footprint_kb, metrics->current_private_footprint_kb);
  EXPECT_EQ(swap_kb, metrics->current_swap_kb);
}

}  // namespace blink
