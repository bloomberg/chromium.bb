// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_ui_selector.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/permissions/test/test_permissions_client.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace permissions {

namespace {
using QuietUiReason = PermissionUiSelector::QuietUiReason;
}

class PermissionRequestManagerTest
    : public content::RenderViewHostTestHarness,
      public ::testing::WithParamInterface<bool> {
 public:
  PermissionRequestManagerTest()
      : request1_(RequestType::kGeolocation,
                  PermissionRequestGestureType::GESTURE),
        request2_(RequestType::kMultipleDownloads,
                  PermissionRequestGestureType::NO_GESTURE),
        request_mic_(RequestType::kMicStream,
                     PermissionRequestGestureType::NO_GESTURE),
        request_camera_(RequestType::kCameraStream,
                        PermissionRequestGestureType::NO_GESTURE),
#if !defined(OS_ANDROID)
        request_ptz_(RequestType::kCameraPanTiltZoom,
                     PermissionRequestGestureType::NO_GESTURE),
#endif
        iframe_request_same_domain_(GURL("https://www.google.com/some/url"),
                                    RequestType::kMidiSysex),
        iframe_request_other_domain_(GURL("https://www.youtube.com"),
                                     RequestType::kGeolocation),
        iframe_request_camera_other_domain_(GURL("https://www.youtube.com"),
                                            RequestType::kStorageAccess),
        iframe_request_mic_other_domain_(GURL("https://www.youtube.com"),
                                         RequestType::kMicStream) {
    feature_list_.InitWithFeatureState(permissions::features::kPermissionChip,
                                       GetParam());
  }

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    NavigateAndCommit(GURL(permissions::MockPermissionRequest::kDefaultOrigin));

    PermissionRequestManager::CreateForWebContents(web_contents());
    manager_ = PermissionRequestManager::FromWebContents(web_contents());
    prompt_factory_ = std::make_unique<MockPermissionPromptFactory>(manager_);
  }

  void TearDown() override {
    prompt_factory_ = nullptr;
    content::RenderViewHostTestHarness::TearDown();
  }

  void Accept() {
    manager_->Accept();
    base::RunLoop().RunUntilIdle();
  }

  void Deny() {
    manager_->Deny();
    base::RunLoop().RunUntilIdle();
  }

  void Closing() {
    manager_->Closing();
    base::RunLoop().RunUntilIdle();
  }

  void WaitForFrameLoad() {
    // PermissionRequestManager ignores all parameters. Yay?
    manager_->DOMContentLoaded(nullptr);
    base::RunLoop().RunUntilIdle();
  }

  void WaitForBubbleToBeShown() {
    manager_->DocumentOnLoadCompletedInMainFrame(main_rfh());
    base::RunLoop().RunUntilIdle();
  }

  void MockTabSwitchAway() {
    manager_->OnVisibilityChanged(content::Visibility::HIDDEN);
  }

  void MockTabSwitchBack() {
    manager_->OnVisibilityChanged(content::Visibility::VISIBLE);
  }

  virtual void NavigationEntryCommitted(
      const content::LoadCommittedDetails& details) {
    manager_->NavigationEntryCommitted(details);
  }

  std::unique_ptr<MockPermissionRequest> CreateAndAddRequest(
      RequestType type,
      bool should_be_seen,
      int expected_request_count) {
    std::unique_ptr<MockPermissionRequest> request =
        std::make_unique<MockPermissionRequest>(
            type, PermissionRequestGestureType::GESTURE);
    manager_->AddRequest(web_contents()->GetMainFrame(), request.get());
    WaitForBubbleToBeShown();
    if (should_be_seen) {
      EXPECT_TRUE(prompt_factory_->RequestTypeSeen(type));
    } else {
      EXPECT_FALSE(prompt_factory_->RequestTypeSeen(type));
    }
    EXPECT_EQ(prompt_factory_->TotalRequestCount(), expected_request_count);

    return request;
  }

  void WaitAndAcceptPromptForRequest(MockPermissionRequest* request) {
    WaitForBubbleToBeShown();

    EXPECT_FALSE(request->finished());
    EXPECT_TRUE(prompt_factory_->is_visible());
    ASSERT_EQ(prompt_factory_->request_count(), 1);

    Accept();
    EXPECT_TRUE(request->granted());
  }

 protected:
  MockPermissionRequest request1_;
  MockPermissionRequest request2_;
  MockPermissionRequest request_mic_;
  MockPermissionRequest request_camera_;
#if !defined(OS_ANDROID)
  MockPermissionRequest request_ptz_;
#endif
  MockPermissionRequest iframe_request_same_domain_;
  MockPermissionRequest iframe_request_other_domain_;
  MockPermissionRequest iframe_request_camera_other_domain_;
  MockPermissionRequest iframe_request_mic_other_domain_;
  PermissionRequestManager* manager_;
  std::unique_ptr<MockPermissionPromptFactory> prompt_factory_;
  TestPermissionsClient client_;
  base::test::ScopedFeatureList feature_list_;
};

////////////////////////////////////////////////////////////////////////////////
// General
////////////////////////////////////////////////////////////////////////////////

TEST_P(PermissionRequestManagerTest, NoRequests) {
  WaitForBubbleToBeShown();
  EXPECT_FALSE(prompt_factory_->is_visible());
}

TEST_P(PermissionRequestManagerTest, SingleRequest) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);

  Accept();
  EXPECT_TRUE(request1_.granted());
}

TEST_P(PermissionRequestManagerTest, SequentialRequests) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());

  Accept();
  EXPECT_TRUE(request1_.granted());
  EXPECT_FALSE(prompt_factory_->is_visible());

  manager_->AddRequest(web_contents()->GetMainFrame(), &request2_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  Accept();
  EXPECT_FALSE(prompt_factory_->is_visible());
  EXPECT_TRUE(request2_.granted());
}

TEST_P(PermissionRequestManagerTest, ForgetRequestsOnPageNavigation) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request2_);
  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_other_domain_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);

  NavigateAndCommit(GURL("http://www2.google.com/"));
  WaitForBubbleToBeShown();

  EXPECT_FALSE(prompt_factory_->is_visible());
  EXPECT_TRUE(request1_.finished());
  EXPECT_TRUE(request2_.finished());
  EXPECT_TRUE(iframe_request_other_domain_.finished());
}

TEST_P(PermissionRequestManagerTest, RequestsDontNeedUserGesture) {
  WaitForFrameLoad();
  WaitForBubbleToBeShown();
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_other_domain_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request2_);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(prompt_factory_->is_visible());
}

TEST_P(PermissionRequestManagerTest, RequestsNotSupported) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  Accept();
  EXPECT_TRUE(request1_.granted());

  manager_->set_web_contents_supports_permission_requests(false);

  manager_->AddRequest(web_contents()->GetMainFrame(), &request2_);
  EXPECT_TRUE(request2_.cancelled());
}

////////////////////////////////////////////////////////////////////////////////
// Requests grouping
////////////////////////////////////////////////////////////////////////////////

// Most requests should never be grouped.
TEST_P(PermissionRequestManagerTest, TwoRequestsUngrouped) {
  // Grouping for chip feature is tested in ThreeRequestsStackOrderChip.
  if (GetParam())
    return;

  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request2_);

  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request1_.granted());

  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request2_.granted());

  ASSERT_EQ(prompt_factory_->show_count(), 2);
}

TEST_P(PermissionRequestManagerTest, ThreeRequestsStackOrderChip) {
  if (!GetParam())
    return;

  // Test new permissions order, requests shouldn't be grouped.
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request2_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_mic_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_mic_.granted());
  EXPECT_FALSE(request2_.granted());
  EXPECT_FALSE(request1_.granted());
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request2_.granted());
  EXPECT_FALSE(request1_.granted());
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request1_.granted());
}

// Test new permissions order by adding requests one at a time.
TEST_P(PermissionRequestManagerTest, ThreeRequestsOneByOneStackOrderChip) {
  if (!GetParam())
    return;

  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();

  manager_->AddRequest(web_contents()->GetMainFrame(), &request2_);
  WaitForBubbleToBeShown();

  manager_->AddRequest(web_contents()->GetMainFrame(), &request_mic_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request_mic_.granted());
  EXPECT_FALSE(request2_.granted());
  EXPECT_FALSE(request1_.granted());
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request2_.granted());
  EXPECT_FALSE(request1_.granted());
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  EXPECT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  EXPECT_TRUE(request1_.granted());
}

// Only mic/camera requests from the same origin should be grouped.
TEST_P(PermissionRequestManagerTest, MicCameraGrouped) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_mic_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_camera_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 2);

  Accept();
  EXPECT_TRUE(request_mic_.granted());
  EXPECT_TRUE(request_camera_.granted());
}

// If mic/camera requests come from different origins, they should not be
// grouped.
TEST_P(PermissionRequestManagerTest, MicCameraDifferentOrigins) {
  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_mic_other_domain_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_camera_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
}

#if !defined(OS_ANDROID)
// Only camera/ptz requests from the same origin should be grouped.
TEST_P(PermissionRequestManagerTest, CameraPtzGrouped) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_camera_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_ptz_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 2);

  Accept();
  EXPECT_TRUE(request_camera_.granted());
  EXPECT_TRUE(request_ptz_.granted());
}

TEST_P(PermissionRequestManagerTest, CameraPtzDifferentOrigins) {
  // If camera/ptz requests come from different origins, they should not be
  // grouped.
  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_camera_other_domain_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_ptz_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
}

// Only mic/camera/ptz requests from the same origin should be grouped.
TEST_P(PermissionRequestManagerTest, MicCameraPtzGrouped) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_mic_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_camera_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_ptz_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 3);

  Accept();
  EXPECT_TRUE(request_mic_.granted());
  EXPECT_TRUE(request_camera_.granted());
  EXPECT_TRUE(request_ptz_.granted());
}

// If mic/camera/ptz requests come from different origins, they should not be
// grouped.
TEST_P(PermissionRequestManagerTest, MicCameraPtzDifferentOrigins) {
  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_mic_other_domain_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_camera_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_ptz_);
  WaitForBubbleToBeShown();

  // Requests should be split into two groups and each one will contain less
  // than 3 requests (1 request + 2 request for current logic and 2 requests + 1
  // request for chip).
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_LT(prompt_factory_->request_count(), 3);
  Accept();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_LT(prompt_factory_->request_count(), 3);
}
#endif  // !defined(OS_ANDROID)

// Tests mix of grouped media requests and non-groupable request.
TEST_P(PermissionRequestManagerTest, MixOfMediaAndNotMediaRequests) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_camera_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_mic_);
  WaitForBubbleToBeShown();

  // Requests should be split into two groups and each one will contain less
  // than 3 requests (1 request + 2 request for current logic and 2 requests + 1
  // request for chip).
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_LT(prompt_factory_->request_count(), 3);
  Accept();
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_LT(prompt_factory_->request_count(), 3);
  Accept();
}

////////////////////////////////////////////////////////////////////////////////
// Tab switching
////////////////////////////////////////////////////////////////////////////////

TEST_P(PermissionRequestManagerTest, TwoRequestsTabSwitch) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_mic_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request_camera_);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 2);

  MockTabSwitchAway();
#if defined(OS_ANDROID)
  EXPECT_TRUE(prompt_factory_->is_visible());
#else
  EXPECT_FALSE(prompt_factory_->is_visible());
#endif

  MockTabSwitchBack();
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 2);

  Accept();
  EXPECT_TRUE(request_mic_.granted());
  EXPECT_TRUE(request_camera_.granted());
}

TEST_P(PermissionRequestManagerTest, PermissionRequestWhileTabSwitchedAway) {
  MockTabSwitchAway();
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(prompt_factory_->is_visible());

  MockTabSwitchBack();
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
}

////////////////////////////////////////////////////////////////////////////////
// Duplicated requests
////////////////////////////////////////////////////////////////////////////////

TEST_P(PermissionRequestManagerTest, SameRequestRejected) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  EXPECT_FALSE(request1_.finished());

  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  Accept();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(request1_.granted());
  EXPECT_FALSE(prompt_factory_->is_visible());
}

TEST_P(PermissionRequestManagerTest, DuplicateRequest) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  manager_->AddRequest(web_contents()->GetMainFrame(), &request2_);

  auto dupe_request = request1_.CreateDuplicateRequest();
  manager_->AddRequest(web_contents()->GetMainFrame(), dupe_request.get());
  EXPECT_FALSE(dupe_request->finished());
  EXPECT_FALSE(request1_.finished());

  auto dupe_request2 = request2_.CreateDuplicateRequest();
  manager_->AddRequest(web_contents()->GetMainFrame(), dupe_request2.get());
  EXPECT_FALSE(dupe_request2->finished());
  EXPECT_FALSE(request2_.finished());

  WaitForBubbleToBeShown();
  Accept();
  if (GetParam()) {
    EXPECT_TRUE(dupe_request2->finished());
    EXPECT_TRUE(request2_.finished());
  } else {
    EXPECT_TRUE(dupe_request->finished());
    EXPECT_TRUE(request1_.finished());
  }

  WaitForBubbleToBeShown();
  Accept();
  if (GetParam()) {
    EXPECT_TRUE(dupe_request->finished());
    EXPECT_TRUE(request1_.finished());
  } else {
    EXPECT_TRUE(dupe_request2->finished());
    EXPECT_TRUE(request2_.finished());
  }
}

////////////////////////////////////////////////////////////////////////////////
// Requests from iframes
////////////////////////////////////////////////////////////////////////////////

TEST_P(PermissionRequestManagerTest, MainFrameNoRequestIFrameRequest) {
  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_same_domain_);
  WaitForBubbleToBeShown();
  WaitForFrameLoad();

  EXPECT_TRUE(prompt_factory_->is_visible());
  Closing();
  EXPECT_TRUE(iframe_request_same_domain_.finished());
}

TEST_P(PermissionRequestManagerTest, MainFrameAndIFrameRequestSameDomain) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_same_domain_);
  WaitForFrameLoad();
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(1, prompt_factory_->request_count());
  Closing();
  if (GetParam()) {
    EXPECT_TRUE(iframe_request_same_domain_.finished());
    EXPECT_FALSE(request1_.finished());
  } else {
    EXPECT_TRUE(request1_.finished());
    EXPECT_FALSE(iframe_request_same_domain_.finished());
  }

  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(1, prompt_factory_->request_count());

  Closing();
  EXPECT_FALSE(prompt_factory_->is_visible());
  if (GetParam())
    EXPECT_TRUE(request1_.finished());
  else
    EXPECT_TRUE(iframe_request_same_domain_.finished());
}

TEST_P(PermissionRequestManagerTest, MainFrameAndIFrameRequestOtherDomain) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_other_domain_);
  WaitForFrameLoad();
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  Closing();
  if (GetParam()) {
    EXPECT_TRUE(iframe_request_other_domain_.finished());
    EXPECT_FALSE(request1_.finished());
  } else {
    EXPECT_TRUE(request1_.finished());
    EXPECT_FALSE(iframe_request_other_domain_.finished());
  }

  EXPECT_TRUE(prompt_factory_->is_visible());
  Closing();
  EXPECT_TRUE(iframe_request_other_domain_.finished());
  if (GetParam())
    EXPECT_TRUE(request1_.finished());
  else
    EXPECT_TRUE(iframe_request_other_domain_.finished());
}

TEST_P(PermissionRequestManagerTest, IFrameRequestWhenMainRequestVisible) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());

  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_same_domain_);
  WaitForFrameLoad();
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  Closing();
  if (GetParam()) {
    EXPECT_TRUE(iframe_request_same_domain_.finished());
    EXPECT_FALSE(request1_.finished());
  } else {
    EXPECT_TRUE(request1_.finished());
    EXPECT_FALSE(iframe_request_same_domain_.finished());
  }

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);
  Closing();
  EXPECT_TRUE(iframe_request_same_domain_.finished());
  if (GetParam())
    EXPECT_TRUE(request1_.finished());
  else
    EXPECT_TRUE(iframe_request_same_domain_.finished());
}

TEST_P(PermissionRequestManagerTest,
       IFrameRequestOtherDomainWhenMainRequestVisible) {
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(prompt_factory_->is_visible());

  manager_->AddRequest(web_contents()->GetMainFrame(),
                       &iframe_request_other_domain_);
  WaitForFrameLoad();
  Closing();
  if (GetParam()) {
    EXPECT_TRUE(iframe_request_other_domain_.finished());
    EXPECT_FALSE(request1_.finished());
  } else {
    EXPECT_TRUE(request1_.finished());
    EXPECT_FALSE(iframe_request_other_domain_.finished());
  }

  EXPECT_TRUE(prompt_factory_->is_visible());
  Closing();
  if (GetParam())
    EXPECT_TRUE(request1_.finished());
  else
    EXPECT_TRUE(iframe_request_other_domain_.finished());
}

////////////////////////////////////////////////////////////////////////////////
// UMA logging
////////////////////////////////////////////////////////////////////////////////

// This code path (calling Accept on a non-merged bubble, with no accepted
// permission) would never be used in actual Chrome, but its still tested for
// completeness.
TEST_P(PermissionRequestManagerTest, UMAForSimpleDeniedBubbleAlternatePath) {
  base::HistogramTester histograms;

  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  // No need to test UMA for showing prompts again, they were tested in
  // UMAForSimpleAcceptedBubble.

  Deny();
  histograms.ExpectUniqueSample(PermissionUmaUtil::kPermissionsPromptDenied,
                                static_cast<base::HistogramBase::Sample>(
                                    RequestTypeForUma::PERMISSION_GEOLOCATION),
                                1);
}

TEST_P(PermissionRequestManagerTest, UMAForTabSwitching) {
  base::HistogramTester histograms;

  manager_->AddRequest(web_contents()->GetMainFrame(), &request1_);
  WaitForBubbleToBeShown();
  histograms.ExpectUniqueSample(PermissionUmaUtil::kPermissionsPromptShown,
                                static_cast<base::HistogramBase::Sample>(
                                    RequestTypeForUma::PERMISSION_GEOLOCATION),
                                1);

  MockTabSwitchAway();
  MockTabSwitchBack();
  histograms.ExpectUniqueSample(PermissionUmaUtil::kPermissionsPromptShown,
                                static_cast<base::HistogramBase::Sample>(
                                    RequestTypeForUma::PERMISSION_GEOLOCATION),
                                1);
}

////////////////////////////////////////////////////////////////////////////////
// UI selectors
////////////////////////////////////////////////////////////////////////////////

// Simulate a PermissionUiSelector that simply returns a predefined |ui_to_use|
// every time.
class MockNotificationPermissionUiSelector : public PermissionUiSelector {
 public:
  explicit MockNotificationPermissionUiSelector(
      absl::optional<QuietUiReason> quiet_ui_reason,
      absl::optional<PermissionUmaUtil::PredictionGrantLikelihood>
          prediction_likelihood,
      bool async)
      : quiet_ui_reason_(quiet_ui_reason),
        prediction_likelihood_(prediction_likelihood),
        async_(async) {}

  void SelectUiToUse(PermissionRequest* request,
                     DecisionMadeCallback callback) override {
    Decision decision(quiet_ui_reason_, Decision::ShowNoWarning());
    if (async_) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), decision));
    } else {
      std::move(callback).Run(decision);
    }
  }

  bool IsPermissionRequestSupported(RequestType request_type) override {
    return request_type == RequestType::kNotifications ||
           request_type == RequestType::kGeolocation;
  }

  absl::optional<PermissionUmaUtil::PredictionGrantLikelihood>
  PredictedGrantLikelihoodForUKM() override {
    return prediction_likelihood_;
  }

  static void CreateForManager(
      PermissionRequestManager* manager,
      absl::optional<QuietUiReason> quiet_ui_reason,
      bool async,
      absl::optional<PermissionUmaUtil::PredictionGrantLikelihood>
          prediction_likelihood = absl::nullopt) {
    manager->add_permission_ui_selector_for_testing(
        std::make_unique<MockNotificationPermissionUiSelector>(
            quiet_ui_reason, prediction_likelihood, async));
  }

 private:
  absl::optional<QuietUiReason> quiet_ui_reason_;
  absl::optional<PermissionUmaUtil::PredictionGrantLikelihood>
      prediction_likelihood_;
  bool async_;
};

// Same as the MockNotificationPermissionUiSelector but handling only the
// Camera stream request type
class MockCameraStreamPermissionUiSelector
    : public MockNotificationPermissionUiSelector {
 public:
  explicit MockCameraStreamPermissionUiSelector(
      absl::optional<QuietUiReason> quiet_ui_reason,
      absl::optional<PermissionUmaUtil::PredictionGrantLikelihood>
          prediction_likelihood,
      bool async)
      : MockNotificationPermissionUiSelector(quiet_ui_reason,
                                             prediction_likelihood,
                                             async) {}

  bool IsPermissionRequestSupported(RequestType request_type) override {
    return request_type == RequestType::kCameraStream;
  }

  static void CreateForManager(
      PermissionRequestManager* manager,
      absl::optional<QuietUiReason> quiet_ui_reason,
      bool async,
      absl::optional<PermissionUmaUtil::PredictionGrantLikelihood>
          prediction_likelihood = absl::nullopt) {
    manager->add_permission_ui_selector_for_testing(
        std::make_unique<MockCameraStreamPermissionUiSelector>(
            quiet_ui_reason, prediction_likelihood, async));
  }
};

TEST_P(PermissionRequestManagerTest,
       UiSelectorNotUsedForPermissionsOtherThanNotification) {
  manager_->clear_permission_ui_selector_for_testing();
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_, PermissionUiSelector::QuietUiReason::kEnabledInPrefs,
      false /* async */);

  manager_->AddRequest(web_contents()->GetMainFrame(), &request_camera_);
  WaitForBubbleToBeShown();

  ASSERT_TRUE(prompt_factory_->is_visible());
  ASSERT_TRUE(prompt_factory_->RequestTypeSeen(request_camera_.request_type()));
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();

  EXPECT_TRUE(request_camera_.granted());
}

TEST_P(PermissionRequestManagerTest, UiSelectorUsedForNotifications) {
  const struct {
    absl::optional<PermissionUiSelector::QuietUiReason> quiet_ui_reason;
    bool async;
  } kTests[] = {
      {QuietUiReason::kEnabledInPrefs, true},
      {PermissionUiSelector::Decision::UseNormalUi(), true},
      {QuietUiReason::kEnabledInPrefs, false},
      {PermissionUiSelector::Decision::UseNormalUi(), false},
  };

  for (const auto& test : kTests) {
    manager_->clear_permission_ui_selector_for_testing();
    MockNotificationPermissionUiSelector::CreateForManager(
        manager_, test.quiet_ui_reason, test.async);

    MockPermissionRequest request(RequestType::kNotifications,
                                  PermissionRequestGestureType::GESTURE);

    manager_->AddRequest(web_contents()->GetMainFrame(), &request);
    WaitForBubbleToBeShown();

    EXPECT_TRUE(prompt_factory_->is_visible());
    EXPECT_TRUE(prompt_factory_->RequestTypeSeen(request.request_type()));
    EXPECT_EQ(!!test.quiet_ui_reason,
              manager_->ShouldCurrentRequestUseQuietUI());
    Accept();

    EXPECT_TRUE(request.granted());
  }
}

TEST_P(PermissionRequestManagerTest,
       UiSelectionHappensSeparatelyForEachRequest) {
  manager_->clear_permission_ui_selector_for_testing();
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_, QuietUiReason::kEnabledInPrefs, true);
  MockPermissionRequest request1(RequestType::kNotifications,
                                 PermissionRequestGestureType::GESTURE);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request1);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();

  MockPermissionRequest request2(RequestType::kNotifications,
                                 PermissionRequestGestureType::GESTURE);
  manager_->clear_permission_ui_selector_for_testing();
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_, PermissionUiSelector::Decision::UseNormalUi(), true);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request2);
  WaitForBubbleToBeShown();
  EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();
}

TEST_P(PermissionRequestManagerTest, MultipleUiSelectors) {

  const struct {
    std::vector<absl::optional<QuietUiReason>> quiet_ui_reasons;
    std::vector<bool> simulate_delayed_decision;
    absl::optional<QuietUiReason> expected_reason;
  } kTests[] = {
      // Simple sync selectors, first one should take priority.
      {{QuietUiReason::kTriggeredByCrowdDeny, QuietUiReason::kEnabledInPrefs},
       {false, false},
       QuietUiReason::kTriggeredByCrowdDeny},
      // First selector is async but should still take priority even if it
      // returns later.
      {{QuietUiReason::kTriggeredByCrowdDeny, QuietUiReason::kEnabledInPrefs},
       {true, false},
       QuietUiReason::kTriggeredByCrowdDeny},
      // The first selector that has a quiet ui decision should be used.
      {{absl::nullopt, absl::nullopt,
        QuietUiReason::kTriggeredDueToAbusiveContent,
        QuietUiReason::kEnabledInPrefs},
       {false, true, true, false},
       QuietUiReason::kTriggeredDueToAbusiveContent},
      // If all selectors return a normal ui, it should use a normal ui.
      {{absl::nullopt, absl::nullopt}, {false, true}, absl::nullopt},

      // Use a bunch of selectors both async and sync.
      {{absl::nullopt, absl::nullopt, absl::nullopt, absl::nullopt,
        absl::nullopt, QuietUiReason::kTriggeredDueToAbusiveRequests,
        absl::nullopt, QuietUiReason::kEnabledInPrefs},
       {false, true, false, true, true, true, false, false},
       QuietUiReason::kTriggeredDueToAbusiveRequests},
      // Use a bunch of selectors all sync.
      {{absl::nullopt, absl::nullopt, absl::nullopt, absl::nullopt,
        absl::nullopt, QuietUiReason::kTriggeredDueToAbusiveRequests,
        absl::nullopt, QuietUiReason::kEnabledInPrefs},
       {false, false, false, false, false, false, false, false},
       QuietUiReason::kTriggeredDueToAbusiveRequests},
      // Use a bunch of selectors all async.
      {{absl::nullopt, absl::nullopt, absl::nullopt, absl::nullopt,
        absl::nullopt, QuietUiReason::kTriggeredDueToAbusiveRequests,
        absl::nullopt, QuietUiReason::kEnabledInPrefs},
       {true, true, true, true, true, true, true, true},
       QuietUiReason::kTriggeredDueToAbusiveRequests},
  };

  for (const auto& test : kTests) {
    manager_->clear_permission_ui_selector_for_testing();
    for (size_t i = 0; i < test.quiet_ui_reasons.size(); ++i) {
      MockNotificationPermissionUiSelector::CreateForManager(
          manager_, test.quiet_ui_reasons[i],
          test.simulate_delayed_decision[i]);
    }

    MockPermissionRequest request(RequestType::kNotifications,
                                  PermissionRequestGestureType::GESTURE);

    manager_->AddRequest(web_contents()->GetMainFrame(), &request);
    WaitForBubbleToBeShown();

    EXPECT_TRUE(prompt_factory_->is_visible());
    EXPECT_TRUE(prompt_factory_->RequestTypeSeen(request.request_type()));
    if (test.expected_reason.has_value()) {
      EXPECT_EQ(test.expected_reason, manager_->ReasonForUsingQuietUi());
    } else {
      EXPECT_FALSE(manager_->ShouldCurrentRequestUseQuietUI());
    }

    Accept();
    EXPECT_TRUE(request.granted());
  }
}

TEST_P(PermissionRequestManagerTest, SelectorsPredictionLikelihood) {
  using PredictionLikelihood = PermissionUmaUtil::PredictionGrantLikelihood;
  const auto VeryLikely = PredictionLikelihood::
      PermissionPrediction_Likelihood_DiscretizedLikelihood_VERY_LIKELY;
  const auto Neutral = PredictionLikelihood::
      PermissionPrediction_Likelihood_DiscretizedLikelihood_NEUTRAL;

  const struct {
    std::vector<bool> enable_quiet_uis;
    std::vector<absl::optional<PredictionLikelihood>> prediction_likelihoods;
    absl::optional<PredictionLikelihood> expected_prediction_likelihood;
  } kTests[] = {
      // Sanity check: prediction likelihood is populated correctly.
      {{true}, {VeryLikely}, VeryLikely},
      {{false}, {Neutral}, Neutral},

      // Prediction likelihood is populated only if the selector was considered.
      {{true, true}, {absl::nullopt, VeryLikely}, absl::nullopt},
      {{false, true}, {absl::nullopt, VeryLikely}, VeryLikely},
      {{false, false}, {absl::nullopt, VeryLikely}, VeryLikely},

      // First considered selector is preserved.
      {{true, true}, {Neutral, VeryLikely}, Neutral},
      {{false, true}, {Neutral, VeryLikely}, Neutral},
      {{false, false}, {Neutral, VeryLikely}, Neutral},
  };

  for (const auto& test : kTests) {
    manager_->clear_permission_ui_selector_for_testing();
    for (size_t i = 0; i < test.enable_quiet_uis.size(); ++i) {
      MockNotificationPermissionUiSelector::CreateForManager(
          manager_,
          test.enable_quiet_uis[i]
              ? absl::optional<QuietUiReason>(QuietUiReason::kEnabledInPrefs)
              : absl::nullopt,
          false /* async */, test.prediction_likelihoods[i]);
    }

    MockPermissionRequest request(RequestType::kNotifications,
                                  PermissionRequestGestureType::GESTURE);

    manager_->AddRequest(web_contents()->GetMainFrame(), &request);
    WaitForBubbleToBeShown();

    EXPECT_TRUE(prompt_factory_->is_visible());
    EXPECT_TRUE(prompt_factory_->RequestTypeSeen(request.request_type()));
    EXPECT_EQ(test.expected_prediction_likelihood,
              manager_->prediction_grant_likelihood_for_testing());

    Accept();
    EXPECT_TRUE(request.granted());
  }
}

TEST_P(PermissionRequestManagerTest, SelectorRequestTypes) {
  const struct {
    RequestType request_type;
    bool should_request_use_quiet_ui;
  } kTests[] = {
      {RequestType::kNotifications, true},
      {RequestType::kGeolocation, true},
      {RequestType::kCameraStream, false},
  };
  manager_->clear_permission_ui_selector_for_testing();
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_, QuietUiReason::kEnabledInPrefs, true);
  for (const auto& test : kTests) {
    MockPermissionRequest request(test.request_type,
                                  PermissionRequestGestureType::GESTURE);
    manager_->AddRequest(web_contents()->GetMainFrame(), &request);
    WaitForBubbleToBeShown();
    EXPECT_EQ(test.should_request_use_quiet_ui,
              manager_->ShouldCurrentRequestUseQuietUI());
    Accept();
  }
  // Adding a mock PermissionUiSelector that handles Camera stream.
  MockCameraStreamPermissionUiSelector::CreateForManager(
      manager_, QuietUiReason::kEnabledInPrefs, true);
  // Now the RequestType::kCameraStream should show a quiet UI as well
  MockPermissionRequest request2(RequestType::kCameraStream,
                                 PermissionRequestGestureType::GESTURE);
  manager_->AddRequest(web_contents()->GetMainFrame(), &request2);
  WaitForBubbleToBeShown();
  EXPECT_TRUE(manager_->ShouldCurrentRequestUseQuietUI());
  Accept();
}

////////////////////////////////////////////////////////////////////////////////
// Quiet UI chip. Low priority for Notifications & Geolocation.
////////////////////////////////////////////////////////////////////////////////

TEST_P(PermissionRequestManagerTest, NotificationsSingleBubbleAndChipRequest) {
  MockPermissionRequest request(RequestType::kNotifications,
                                PermissionRequestGestureType::GESTURE);

  manager_->AddRequest(web_contents()->GetMainFrame(), &request);
  WaitForBubbleToBeShown();

  EXPECT_TRUE(prompt_factory_->is_visible());
  ASSERT_EQ(prompt_factory_->request_count(), 1);

  Accept();
  EXPECT_TRUE(request.granted());
  EXPECT_EQ(prompt_factory_->show_count(), 1);
}

// Quiet UI feature is disabled. Chip is disabled. No low priority requests, the
// first request is always shown.
//
// Permissions requested in order:
// 1. Notification (non abusive)
// 2. Geolocation
// 3. Camera
//
// Prompt display order:
// 1. Notification request shown
// 2. Geolocation request shown
// 3. Camera request shown
TEST_P(PermissionRequestManagerTest,
       NotificationsGeolocationCameraBubbleRequest) {
  // permissions::features::kPermissionChip is enabled based on `GetParam()`.
  // That test is only for the default bubble.
  if (GetParam())
    return;

  std::unique_ptr<MockPermissionRequest> request_notifications =
      CreateAndAddRequest(RequestType::kNotifications, /*should_be_seen=*/true,
                          1);
  std::unique_ptr<MockPermissionRequest> request_geolocation =
      CreateAndAddRequest(RequestType::kGeolocation, /*should_be_seen=*/false,
                          1);
  std::unique_ptr<MockPermissionRequest> request_camera = CreateAndAddRequest(
      RequestType::kCameraStream, /*should_be_seen=*/false, 1);

  for (auto* kRequest : {request_notifications.get(), request_geolocation.get(),
                         request_camera.get()}) {
    WaitAndAcceptPromptForRequest(kRequest);
  }

  EXPECT_EQ(prompt_factory_->show_count(), 3);
}

// Quiet UI feature is disabled, no low priority requests, the last request is
// always shown.
//
// Permissions requested in order:
// 1. Notification (non abusive)
// 2. Geolocation
// 3. Camera
//
// Prompt display order:
// 1. Notifications request shown but is preempted
// 2. Geolocation request shown but is preempted
// 3. Camera request shown
// 4. Geolocation request shown again
// 5. Notifications request shown again
TEST_P(PermissionRequestManagerTest,
       NotificationsGeolocationCameraChipRequest) {
  // permissions::features::kPermissionChip is enabled based on `GetParam()`.
  // That test is only for the chip UI.
  if (!GetParam())
    return;

  std::unique_ptr<MockPermissionRequest> request_notifications =
      CreateAndAddRequest(RequestType::kNotifications, /*should_be_seen=*/true,
                          1);
  std::unique_ptr<MockPermissionRequest> request_geolocation =
      CreateAndAddRequest(RequestType::kGeolocation, /*should_be_seen=*/true,
                          2);
  std::unique_ptr<MockPermissionRequest> request_camera = CreateAndAddRequest(
      RequestType::kCameraStream, /*should_be_seen=*/true, 3);

  for (auto* kRequest : {request_camera.get(), request_geolocation.get(),
                         request_notifications.get()}) {
    WaitAndAcceptPromptForRequest(kRequest);
  }

  EXPECT_EQ(prompt_factory_->show_count(), 5);
}

// Quiet UI feature is disabled, no low priority requests, the last request is
// always shown.
//
// Permissions requested in order:
// 1. Camera
// 2. Notification (non abusive)
// 3. Geolocation
//
// Prompt display order:
// 1. Camera request shown but is preempted
// 2. Notifications request shown but is preempted
// 3. Geolocation request shown
// 4. Notifications request shown again
// 5. Camera request shown again
TEST_P(PermissionRequestManagerTest,
       CameraNotificationsGeolocationChipRequest) {
  // permissions::features::kPermissionChip is enabled based on `GetParam()`.
  // That test is only for the chip.
  if (!GetParam())
    return;

  std::unique_ptr<MockPermissionRequest> request_camera = CreateAndAddRequest(
      RequestType::kCameraStream, /*should_be_seen=*/true, 1);
  std::unique_ptr<MockPermissionRequest> request_notifications =
      CreateAndAddRequest(RequestType::kNotifications, /*should_be_seen=*/true,
                          2);
  std::unique_ptr<MockPermissionRequest> request_geolocation =
      CreateAndAddRequest(RequestType::kGeolocation, /*should_be_seen=*/true,
                          3);

  for (auto* kRequest : {request_geolocation.get(), request_notifications.get(),
                         request_camera.get()}) {
    WaitAndAcceptPromptForRequest(kRequest);
  }

  EXPECT_EQ(prompt_factory_->show_count(), 5);
}

class PermissionRequestManagerTestQuietChip
    : public PermissionRequestManagerTest {
 public:
  PermissionRequestManagerTestQuietChip() {
    feature_list_.InitWithFeatureState(
        permissions::features::kPermissionQuietChip, true);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verifies that the quiet UI chip is not ignored if another request came in
// less than 8.5 seconds after.
// Permissions requested in order:
// 1. Notification (abusive)
// 2. After less than 8.5 seconds Geolocation
//
// Prompt display order:
// 1. Notifications request shown but is preempted because of quiet UI.
// 2. Geolocation request shown
// 3. Notifications request shown again
TEST_P(PermissionRequestManagerTestQuietChip,
       AbusiveNotificationsGeolocationQuietUIChipRequest) {
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_,
      PermissionUiSelector::QuietUiReason::kTriggeredDueToAbusiveRequests,
      false /* async */);

  std::unique_ptr<MockPermissionRequest> request_notifications =
      CreateAndAddRequest(RequestType::kNotifications, /*should_be_seen=*/true,
                          1);

  // Less then 8.5 seconds.
  manager_->set_current_request_first_display_time_for_testing(
      base::Time::Now() - base::TimeDelta::FromMilliseconds(5000));

  std::unique_ptr<MockPermissionRequest> request_geolocation =
      CreateAndAddRequest(RequestType::kGeolocation, /*should_be_seen=*/true,
                          2);

  WaitAndAcceptPromptForRequest(request_geolocation.get());
  WaitAndAcceptPromptForRequest(request_notifications.get());

  EXPECT_EQ(prompt_factory_->show_count(), 3);
}

// Verifies that the quiet UI chip is ignored if another request came in more
// than 8.5 seconds after.
//
// Permissions requested in order:
// 1. Notification (abusive)
// 2. After more than 8.5 seconds Geolocation
//
// Prompt display order:
// 1. Notifications request shown but is preempted because of quiet UI.
// 2. Geolocation request shown
TEST_P(PermissionRequestManagerTestQuietChip,
       AbusiveNotificationsShownLongEnough) {
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_,
      PermissionUiSelector::QuietUiReason::kTriggeredDueToAbusiveRequests,
      false /* async */);

  std::unique_ptr<MockPermissionRequest> request_notifications =
      CreateAndAddRequest(RequestType::kNotifications, /*should_be_seen=*/true,
                          1);

  // More then 8.5 seconds.
  manager_->set_current_request_first_display_time_for_testing(
      base::Time::Now() - base::TimeDelta::FromMilliseconds(9000));

  std::unique_ptr<MockPermissionRequest> request_geolocation =
      CreateAndAddRequest(RequestType::kGeolocation, /*should_be_seen=*/true,
                          2);

  // The second permission was requested after 8.5 second window, the quiet UI
  // Notifiations request for an abusive origin is automatically ignored.
  EXPECT_FALSE(request_notifications->granted());
  EXPECT_TRUE(request_notifications->finished());

  WaitAndAcceptPromptForRequest(request_geolocation.get());

  EXPECT_EQ(prompt_factory_->show_count(), 2);
}

// Verifies that the quiet UI chip is not ignored if another request came in
// more than 8.5 seconds after. Verify different requests priority. Camera
// request is shown despite being requested last.
//
// Permissions requested in order:
// 1. Notification (abusive)
// 2. After less than 8.5 seconds Geolocation
// 3. Camera
//
// Prompt display order:
// 1. Notifications request shown but is preempted because of quiet UI.
// 2. Geolocation request shown but is preempted because of low priority.
// 3. Camera request shown
// 4. Geolocation request shown again
// 5. Notifications quiet UI request shown again
TEST_P(PermissionRequestManagerTestQuietChip,
       AbusiveNotificationsShownLongEnoughCamera) {
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_,
      PermissionUiSelector::QuietUiReason::kTriggeredDueToAbusiveRequests,
      false /* async */);

  std::unique_ptr<MockPermissionRequest> request_notifications =
      CreateAndAddRequest(RequestType::kNotifications, /*should_be_seen=*/true,
                          1);
  // Less then 8.5 seconds.
  manager_->set_current_request_first_display_time_for_testing(
      base::Time::Now() - base::TimeDelta::FromMilliseconds(5000));

  std::unique_ptr<MockPermissionRequest> request_geolocation =
      CreateAndAddRequest(RequestType::kGeolocation, /*should_be_seen=*/true,
                          2);
  std::unique_ptr<MockPermissionRequest> request_camera = CreateAndAddRequest(
      RequestType::kCameraStream, /*should_be_seen=*/true, 3);

  // The second permission was requested in 8.5 second window, the quiet UI
  // Notifiations request for an abusive origin is not automatically ignored.
  EXPECT_FALSE(request_notifications->granted());
  EXPECT_FALSE(request_notifications->finished());

  for (auto* kRequest : {request_camera.get(), request_geolocation.get(),
                         request_notifications.get()}) {
    WaitAndAcceptPromptForRequest(kRequest);
  }

  EXPECT_EQ(prompt_factory_->show_count(), 5);
}

// Verifies that the quiet UI chip is not ignored if another request came in
// more than 8.5 seconds after. Verify different requests priority. Camera
// request is not preemted.
//
// Permissions requested in order:
// 1. Camera
// 2. Notification (abusive)
// 3. After less than 8.5 seconds Geolocation
//
// Prompt display order:
// 1. Camera request shown
// 2. Geolocation request shown
// 3. Camera request shown
TEST_P(PermissionRequestManagerTestQuietChip,
       CameraAbusiveNotificationsGeolocation) {
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_,
      PermissionUiSelector::QuietUiReason::kTriggeredDueToAbusiveRequests,
      false /* async */);

  std::unique_ptr<MockPermissionRequest> request_camera = CreateAndAddRequest(
      RequestType::kCameraStream, /*should_be_seen=*/true, 1);

  // Quiet UI is not shown because Camera has higher priority.
  std::unique_ptr<MockPermissionRequest> request_notifications =
      CreateAndAddRequest(RequestType::kNotifications, /*should_be_seen=*/false,
                          1);
  // Less then 8.5 seconds.
  manager_->set_current_request_first_display_time_for_testing(
      base::Time::Now() - base::TimeDelta::FromMilliseconds(5000));

  // Geolocation is not shown because Camera has higher priority.
  std::unique_ptr<MockPermissionRequest> request_geolocation =
      CreateAndAddRequest(RequestType::kGeolocation, /*should_be_seen=*/false,
                          1);

  // The second permission after quiet UI was requested in 8.5 second window,
  // the quiet UI Notifiations request for an abusive origin is not
  // automatically ignored.
  EXPECT_FALSE(request_notifications->granted());
  EXPECT_FALSE(request_notifications->finished());

  for (auto* kRequest : {request_camera.get(), request_geolocation.get(),
                         request_notifications.get()}) {
    WaitAndAcceptPromptForRequest(kRequest);
  }

  EXPECT_EQ(prompt_factory_->show_count(), 3);
}

// Verifies that the quiet UI chip is not ignored if another request came in
// more than 8.5 seconds after. Verify different requests priority. Camera
// request is not preemted.
//
// Permissions requested in order:
// 1. Camera
// 2. Notification (abusive)
// 3. After less than 8.5 seconds Geolocation
// 4. MIDI
//
// Prompt display order:
// 1. Camera request shown
// 2. MIDI request shown (or MIDI and then Camera, the order depends on
// `GetParam()`)
// 3. Geolocation request shown
// 4. Notifications request shown
// If Chip is enabled MIDI will replace Camera, hence 5 prompts will be
// shown. Otherwise 4.
TEST_P(PermissionRequestManagerTestQuietChip,
       CameraAbusiveNotificationsGeolocationMIDI) {
  MockNotificationPermissionUiSelector::CreateForManager(
      manager_,
      PermissionUiSelector::QuietUiReason::kTriggeredDueToAbusiveRequests,
      false /* async */);

  std::unique_ptr<MockPermissionRequest> request_camera = CreateAndAddRequest(
      RequestType::kCameraStream, /*should_be_seen=*/true, 1);

  // Quiet UI is not shown because Camera has higher priority.
  std::unique_ptr<MockPermissionRequest> request_notifications =
      CreateAndAddRequest(RequestType::kNotifications, /*should_be_seen=*/false,
                          1);
  // Less then 8.5 seconds.
  manager_->set_current_request_first_display_time_for_testing(
      base::Time::Now() - base::TimeDelta::FromMilliseconds(5000));

  // Geolocation is not shown because Camera has higher priority.
  std::unique_ptr<MockPermissionRequest> request_geolocation =
      CreateAndAddRequest(RequestType::kGeolocation, /*should_be_seen=*/false,
                          1);

  std::unique_ptr<MockPermissionRequest> request_midi;

  // If Chip is enabled, MIDI should be shown, otherwise MIDI should not be
  // shown.
  if (GetParam()) {
    request_midi = CreateAndAddRequest(RequestType::kMidiSysex,
                                       /*should_be_seen=*/true, 2);
  } else {
    request_midi = CreateAndAddRequest(RequestType::kMidiSysex,
                                       /*should_be_seen=*/false, 1);
  }

  // The second permission after quiet UI was requested in 8.5 second window,
  // the quiet UI Notifiations request for an abusive origin is not
  // automatically ignored.
  EXPECT_FALSE(request_notifications->granted());
  EXPECT_FALSE(request_notifications->finished());

  WaitAndAcceptPromptForRequest(GetParam() ? request_midi.get()
                                           : request_camera.get());
  WaitAndAcceptPromptForRequest(GetParam() ? request_camera.get()
                                           : request_midi.get());
  WaitAndAcceptPromptForRequest(request_geolocation.get());
  WaitAndAcceptPromptForRequest(request_notifications.get());

  EXPECT_EQ(prompt_factory_->show_count(), GetParam() ? 5 : 4);
}

// Verifies that non abusive chip behaves similar to others when Quiet UI Chip
// is enabled.
//
// Permissions requested in order:
// 1. Camera
// 2. Notification (non abusive)
// 3. After less than 8.5 seconds Geolocation
// 4. MIDI
//
// Prompt display order:
// 1. Camera request shown
// 2. MIDI request shown (or MIDI and then Camera, the order depends on
// `GetParam()`)
// 3. Geolocation request shown
// 4. Notifications request shown
// If Chip is enabled MIDI will replace Camera, hence 5 prompts will be
// shown. Otherwise 4.
TEST_P(PermissionRequestManagerTestQuietChip,
       CameraNonAbusiveNotificationsGeolocationMIDI) {
  std::unique_ptr<MockPermissionRequest> request_camera = CreateAndAddRequest(
      RequestType::kCameraStream, /*should_be_seen=*/true, 1);

  // Quiet UI is not shown because Camera has higher priority.
  std::unique_ptr<MockPermissionRequest> request_notifications =
      CreateAndAddRequest(RequestType::kNotifications, /*should_be_seen=*/false,
                          1);
  // Less then 8.5 seconds.
  manager_->set_current_request_first_display_time_for_testing(
      base::Time::Now() - base::TimeDelta::FromMilliseconds(5000));

  // Geolocation is not shown because Camera has higher priority.
  std::unique_ptr<MockPermissionRequest> request_geolocation =
      CreateAndAddRequest(RequestType::kGeolocation, /*should_be_seen=*/false,
                          1);

  std::unique_ptr<MockPermissionRequest> request_midi;

  // If Chip is enabled, MIDI should be shown, otherwise MIDI should not be
  // shown.
  if (GetParam()) {
    request_midi = CreateAndAddRequest(RequestType::kMidiSysex,
                                       /*should_be_seen=*/true, 2);
  } else {
    request_midi = CreateAndAddRequest(RequestType::kMidiSysex,
                                       /*should_be_seen=*/false, 1);
  }

  // The second permission after quiet UI was requested in 8.5 second window,
  // the quiet UI Notifiations request for an abusive origin is not
  // automatically ignored.
  EXPECT_FALSE(request_notifications->granted());
  EXPECT_FALSE(request_notifications->finished());

  WaitAndAcceptPromptForRequest(GetParam() ? request_midi.get()
                                           : request_camera.get());
  WaitAndAcceptPromptForRequest(GetParam() ? request_camera.get()
                                           : request_midi.get());
  WaitAndAcceptPromptForRequest(request_geolocation.get());
  WaitAndAcceptPromptForRequest(request_notifications.get());

  EXPECT_EQ(prompt_factory_->show_count(), GetParam() ? 5 : 4);
}

INSTANTIATE_TEST_SUITE_P(All,
                         PermissionRequestManagerTest,
                         ::testing::Values(false, true));
INSTANTIATE_TEST_SUITE_P(All,
                         PermissionRequestManagerTestQuietChip,
                         ::testing::Values(false, true));

}  // namespace permissions
