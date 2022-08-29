// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/segmentation_platform/public/service_proxy.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace segmentation_platform {
namespace {

// Observer that waits for service initialization.
class WaitServiceInitializedObserver : public ServiceProxy::Observer {
 public:
  explicit WaitServiceInitializedObserver(base::OnceClosure closure)
      : closure_(std::move(closure)) {}
  void OnServiceStatusChanged(bool initialized, int status_flags) override {
    if (initialized) {
      std::move(closure_).Run();
    }
  }

 private:
  base::OnceClosure closure_;
};

}  // namespace
class SegmentationPlatformServiceFactoryTest : public PlatformTest {
 public:
  SegmentationPlatformServiceFactoryTest() {
    scoped_feature_list_.InitWithFeatures(
        {optimization_guide::features::kOptimizationTargetPrediction,
         features::kSegmentationPlatformFeature},
        {});
  }
  ~SegmentationPlatformServiceFactoryTest() override = default;

  void SetUp() override {
    PlatformTest::SetUp();

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        SegmentationPlatformServiceFactory::GetInstance(),
        SegmentationPlatformServiceFactory::GetDefaultFactory());
    browser_state_ = builder.Build();

    auto* service = SegmentationPlatformServiceFactory::GetForBrowserState(
        browser_state_.get());
    WaitForServiceInit(service);

    ChromeBrowserState* otr_browser_state =
        browser_state_->CreateOffTheRecordBrowserStateWithTestingFactories(
            {std::make_pair(
                SegmentationPlatformServiceFactory::GetInstance(),
                SegmentationPlatformServiceFactory::GetDefaultFactory())});
    ASSERT_FALSE(SegmentationPlatformServiceFactory::GetForBrowserState(
        otr_browser_state));
  }

 protected:
  void WaitForServiceInit(SegmentationPlatformService* service) {
    base::RunLoop wait_for_init;
    WaitServiceInitializedObserver wait_observer(wait_for_init.QuitClosure());
    service->GetServiceProxy()->AddObserver(&wait_observer);

    wait_for_init.Run();
    while (!service->IsPlatformInitialized()) {
      base::RunLoop().RunUntilIdle();
    }

    service->GetServiceProxy()->RemoveObserver(&wait_observer);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<TestChromeBrowserState> browser_state_;
};

TEST_F(SegmentationPlatformServiceFactoryTest, Test) {
  // TODO(crbug.com/1333641): Add test for the API once the initialization is
  // fixed.
}

}  // namespace segmentation_platform
