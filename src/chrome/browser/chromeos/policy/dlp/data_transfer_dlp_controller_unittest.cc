// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/data_transfer_dlp_controller.h"

#include "base/optional.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "url/origin.h"

namespace policy {

namespace {

constexpr char kExample1Url[] = "https://www.example1.com";
constexpr char kExample2Url[] = "https://www.example2.com";

class MockDlpRulesManager : public DlpRulesManager {
 public:
  MockDlpRulesManager() = default;
  ~MockDlpRulesManager() override = default;

  MOCK_CONST_METHOD2(IsRestricted,
                     Level(const GURL& source, Restriction restriction));

  MOCK_CONST_METHOD3(IsRestrictedDestination,
                     Level(const GURL& source,
                           const GURL& destination,
                           Restriction restriction));

  MOCK_CONST_METHOD3(IsRestrictedComponent,
                     Level(const GURL& source,
                           const Component& destination,
                           Restriction restriction));

  MOCK_CONST_METHOD3(IsRestrictedAnyOfComponents,
                     Level(const GURL& source,
                           const std::vector<Component>& destinations,
                           Restriction restriction));
};

class MockDlpController : public DataTransferDlpController {
 public:
  explicit MockDlpController(const DlpRulesManager& dlp_rules_manager)
      : DataTransferDlpController(dlp_rules_manager) {}

  MOCK_METHOD2(NotifyBlockedPaste,
               void(const ui::DataTransferEndpoint* const data_src,
                    const ui::DataTransferEndpoint* const data_dst));

  MOCK_METHOD2(WarnOnPaste,
               void(const ui::DataTransferEndpoint* const data_src,
                    const ui::DataTransferEndpoint* const data_dst));

  MOCK_METHOD1(ShouldProceedOnWarn,
               bool(const ui::DataTransferEndpoint* const data_dst));
};

// Creates a new MockDlpRulesManager for the given |context|.
std::unique_ptr<KeyedService> BuildDlpRulesManager(
    content::BrowserContext* context) {
  return std::make_unique<::testing::StrictMock<MockDlpRulesManager>>();
}

base::Optional<ui::DataTransferEndpoint> CreateEndpoint(
    ui::EndpointType* type,
    bool notify_if_restricted) {
  if (type && *type == ui::EndpointType::kUrl) {
    return ui::DataTransferEndpoint(
        url::Origin::Create(GURL(kExample2Url)),
        /*notify_if_restricted=*/notify_if_restricted);
  } else if (type) {
    return ui::DataTransferEndpoint(
        *type,
        /*notify_if_restricted=*/notify_if_restricted);
  }
  return base::nullopt;
}

}  // namespace

class DataTransferDlpControllerTest
    : public ::testing::TestWithParam<
          std::tuple<base::Optional<ui::EndpointType>, bool>> {
 protected:
  DataTransferDlpControllerTest()
      : rules_manager_(), dlp_controller_(rules_manager_) {}

  ~DataTransferDlpControllerTest() override = default;

  content::BrowserTaskEnvironment task_environment_;
  ::testing::StrictMock<MockDlpRulesManager> rules_manager_;
  ::testing::StrictMock<MockDlpController> dlp_controller_;
  base::HistogramTester histogram_tester_;
};

TEST_F(DataTransferDlpControllerTest, NullSrc) {
  EXPECT_EQ(true, dlp_controller_.IsClipboardReadAllowed(nullptr, nullptr));
  EXPECT_EQ(true, dlp_controller_.IsDragDropAllowed(nullptr, nullptr,
                                                    /*is_drop=*/false));
  histogram_tester_.ExpectUniqueSample(
      GetDlpHistogramPrefix() + dlp::kClipboardReadBlockedUMA, false, 1);
  histogram_tester_.ExpectUniqueSample(
      GetDlpHistogramPrefix() + dlp::kDragDropBlockedUMA, false, 1);
}

TEST_F(DataTransferDlpControllerTest, ClipboardHistoryDst) {
  ui::DataTransferEndpoint data_src(url::Origin::Create(GURL(kExample1Url)));
  ui::DataTransferEndpoint data_dst(ui::EndpointType::kClipboardHistory);
  EXPECT_EQ(true, dlp_controller_.IsClipboardReadAllowed(&data_src, &data_dst));
  histogram_tester_.ExpectUniqueSample(
      GetDlpHistogramPrefix() + dlp::kClipboardReadBlockedUMA, false, 1);
}

// Create a version of the test class for parameterized testing.
using DlpControllerTest = DataTransferDlpControllerTest;

INSTANTIATE_TEST_SUITE_P(
    DlpClipboard,
    DlpControllerTest,
    ::testing::Combine(::testing::Values(base::nullopt,
                                         ui::EndpointType::kDefault,
                                         ui::EndpointType::kUnknownVm,
                                         ui::EndpointType::kBorealis,
                                         ui::EndpointType::kUrl),
                       testing::Bool()));

TEST_P(DlpControllerTest, Allow) {
  ui::DataTransferEndpoint data_src(url::Origin::Create(GURL(kExample1Url)));
  base::Optional<ui::EndpointType> endpoint_type;
  bool do_notify;
  std::tie(endpoint_type, do_notify) = GetParam();
  base::Optional<ui::DataTransferEndpoint> data_dst =
      CreateEndpoint(base::OptionalOrNullptr(endpoint_type), do_notify);
  auto* dst_ptr = base::OptionalOrNullptr(data_dst);

  // IsClipboardReadAllowed
  EXPECT_CALL(rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kAllow));

  EXPECT_EQ(true, dlp_controller_.IsClipboardReadAllowed(&data_src, dst_ptr));
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);

  // IsDragDropAllowed
  EXPECT_CALL(rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kAllow));

  EXPECT_EQ(true, dlp_controller_.IsDragDropAllowed(&data_src, dst_ptr,
                                                    /*is_drop=*/do_notify));
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);
  histogram_tester_.ExpectUniqueSample(
      GetDlpHistogramPrefix() + dlp::kClipboardReadBlockedUMA, false, 1);
  histogram_tester_.ExpectUniqueSample(
      GetDlpHistogramPrefix() + dlp::kDragDropBlockedUMA, false, 1);
}

TEST_P(DlpControllerTest, Block) {
  ui::DataTransferEndpoint data_src(url::Origin::Create(GURL(kExample1Url)));
  base::Optional<ui::EndpointType> endpoint_type;
  bool do_notify;
  std::tie(endpoint_type, do_notify) = GetParam();
  base::Optional<ui::DataTransferEndpoint> data_dst =
      CreateEndpoint(base::OptionalOrNullptr(endpoint_type), do_notify);
  auto* dst_ptr = base::OptionalOrNullptr(data_dst);

  // IsClipboardReadAllowed
  EXPECT_CALL(rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));
  if (do_notify || !dst_ptr)
    EXPECT_CALL(dlp_controller_, NotifyBlockedPaste);

  EXPECT_EQ(false, dlp_controller_.IsClipboardReadAllowed(&data_src, dst_ptr));
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);

  // IsDragDropAllowed
  EXPECT_CALL(rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));
  if (do_notify)
    EXPECT_CALL(dlp_controller_, NotifyBlockedPaste);

  EXPECT_EQ(false, dlp_controller_.IsDragDropAllowed(&data_src, dst_ptr,
                                                     /*is_drop=*/do_notify));
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);
  histogram_tester_.ExpectUniqueSample(
      GetDlpHistogramPrefix() + dlp::kClipboardReadBlockedUMA, true, 1);
  histogram_tester_.ExpectUniqueSample(
      GetDlpHistogramPrefix() + dlp::kDragDropBlockedUMA, true, 1);
}

TEST_P(DlpControllerTest, Warn) {
  ui::DataTransferEndpoint data_src(url::Origin::Create(GURL(kExample1Url)));
  base::Optional<ui::EndpointType> endpoint_type;
  bool do_notify;
  std::tie(endpoint_type, do_notify) = GetParam();
  base::Optional<ui::DataTransferEndpoint> data_dst =
      CreateEndpoint(base::OptionalOrNullptr(endpoint_type), do_notify);
  auto* dst_ptr = base::OptionalOrNullptr(data_dst);

  // ShouldProceedOnWarn returns false.
  EXPECT_CALL(rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kWarn));
  EXPECT_CALL(dlp_controller_, ShouldProceedOnWarn)
      .WillRepeatedly(testing::Return(false));
  bool show_warning = (do_notify || !dst_ptr);
  if (show_warning)
    EXPECT_CALL(dlp_controller_, WarnOnPaste);

  EXPECT_EQ(!show_warning,
            dlp_controller_.IsClipboardReadAllowed(&data_src, dst_ptr));
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);

  // ShouldProceedOnWarn returns true.
  EXPECT_CALL(rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kWarn));
  EXPECT_CALL(dlp_controller_, ShouldProceedOnWarn)
      .WillRepeatedly(testing::Return(true));
  EXPECT_EQ(true, dlp_controller_.IsClipboardReadAllowed(&data_src, dst_ptr));
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kClipboardReadBlockedUMA, false,
      show_warning ? 1 : 2);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kClipboardReadBlockedUMA, true,
      show_warning ? 1 : 0);
}

// Create a version of the test class for parameterized testing.
using DlpControllerVMsTest = DataTransferDlpControllerTest;

INSTANTIATE_TEST_SUITE_P(
    DlpClipboard,
    DlpControllerVMsTest,
    ::testing::Combine(::testing::Values(ui::EndpointType::kArc,
                                         ui::EndpointType::kCrostini,
                                         ui::EndpointType::kPluginVm),
                       testing::Bool()));

TEST_P(DlpControllerVMsTest, Allow) {
  ui::DataTransferEndpoint data_src(url::Origin::Create(GURL(kExample1Url)));
  base::Optional<ui::EndpointType> endpoint_type;
  bool do_notify;
  std::tie(endpoint_type, do_notify) = GetParam();
  ASSERT_TRUE(endpoint_type.has_value());
  ui::DataTransferEndpoint data_dst(endpoint_type.value(), do_notify);

  // IsClipboardReadAllowed
  EXPECT_CALL(rules_manager_, IsRestrictedComponent)
      .WillOnce(testing::Return(DlpRulesManager::Level::kAllow));

  EXPECT_EQ(true, dlp_controller_.IsClipboardReadAllowed(&data_src, &data_dst));
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);

  // IsDragDropAllowed
  EXPECT_CALL(rules_manager_, IsRestrictedComponent)
      .WillOnce(testing::Return(DlpRulesManager::Level::kAllow));

  EXPECT_EQ(true, dlp_controller_.IsDragDropAllowed(&data_src, &data_dst,
                                                    /*is_drop=*/do_notify));
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);
  histogram_tester_.ExpectUniqueSample(
      GetDlpHistogramPrefix() + dlp::kClipboardReadBlockedUMA, false, 1);
  histogram_tester_.ExpectUniqueSample(
      GetDlpHistogramPrefix() + dlp::kDragDropBlockedUMA, false, 1);
}

TEST_P(DlpControllerVMsTest, Block) {
  ui::DataTransferEndpoint data_src(url::Origin::Create(GURL(kExample1Url)));
  base::Optional<ui::EndpointType> endpoint_type;
  bool do_notify;
  std::tie(endpoint_type, do_notify) = GetParam();
  ASSERT_TRUE(endpoint_type.has_value());
  ui::DataTransferEndpoint data_dst(endpoint_type.value(), do_notify);

  // IsClipboardReadAllowed
  EXPECT_CALL(rules_manager_, IsRestrictedComponent)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));
  if (do_notify)
    EXPECT_CALL(dlp_controller_, NotifyBlockedPaste);

  EXPECT_EQ(false,
            dlp_controller_.IsClipboardReadAllowed(&data_src, &data_dst));
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);

  // IsDragDropAllowed
  EXPECT_CALL(rules_manager_, IsRestrictedComponent)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));
  if (do_notify)
    EXPECT_CALL(dlp_controller_, NotifyBlockedPaste);

  EXPECT_EQ(false, dlp_controller_.IsDragDropAllowed(&data_src, &data_dst,
                                                     /*is_drop=*/do_notify));
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);
  histogram_tester_.ExpectUniqueSample(
      GetDlpHistogramPrefix() + dlp::kClipboardReadBlockedUMA, true, 1);
  histogram_tester_.ExpectUniqueSample(
      GetDlpHistogramPrefix() + dlp::kDragDropBlockedUMA, true, 1);
}

TEST_P(DlpControllerVMsTest, Warn) {
  ui::DataTransferEndpoint data_src(url::Origin::Create(GURL(kExample1Url)));
  base::Optional<ui::EndpointType> endpoint_type;
  bool do_notify;
  std::tie(endpoint_type, do_notify) = GetParam();
  ASSERT_TRUE(endpoint_type.has_value());
  ui::DataTransferEndpoint data_dst(endpoint_type.value(), do_notify);

  // IsClipboardReadAllowed
  EXPECT_CALL(rules_manager_, IsRestrictedComponent)
      .WillOnce(testing::Return(DlpRulesManager::Level::kWarn));
  if (do_notify)
    EXPECT_CALL(dlp_controller_, WarnOnPaste);

  EXPECT_EQ(true, dlp_controller_.IsClipboardReadAllowed(&data_src, &data_dst));
  testing::Mock::VerifyAndClearExpectations(&dlp_controller_);
  histogram_tester_.ExpectUniqueSample(
      GetDlpHistogramPrefix() + dlp::kClipboardReadBlockedUMA, false, 1);
}

}  // namespace policy
