// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/service_proxy_impl.h"

#include "base/strings/string_number_conversions.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/platform_options.h"
#include "components/segmentation_platform/internal/scheduler/model_execution_scheduler.h"
#include "components/segmentation_platform/internal/selection/segment_selector_impl.h"
#include "components/segmentation_platform/internal/selection/segmentation_result_prefs.h"
#include "components/segmentation_platform/public/config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace segmentation_platform {

namespace {
// Adds a segment info into a map, and return a copy of it.
proto::SegmentInfo AddSegmentInfo(
    std::map<std::string, proto::SegmentInfo>* db_entries,
    Config* config,
    OptimizationTarget segment_id) {
  proto::SegmentInfo info;
  info.set_segment_id(segment_id);
  db_entries->insert(
      std::make_pair(base::NumberToString(static_cast<int>(segment_id)), info));
  config->segment_ids.emplace_back(segment_id);
  return info;
}

class MockModelExecutionScheduler : public ModelExecutionScheduler {
 public:
  MockModelExecutionScheduler() = default;
  MOCK_METHOD(void, RequestModelExecution, (OptimizationTarget));
  MOCK_METHOD(void, OnNewModelInfoReady, (const proto::SegmentInfo&));
  MOCK_METHOD(void, RequestModelExecutionForEligibleSegments, (bool));
  MOCK_METHOD(void,
              OnModelExecutionCompleted,
              (OptimizationTarget,
               (const std::pair<float, ModelExecutionStatus>&)));
};

}  // namespace

class FakeSegmentSelectorImpl : public SegmentSelectorImpl {
 public:
  FakeSegmentSelectorImpl(SegmentationResultPrefs* result_prefs,
                          const Config* config)
      : SegmentSelectorImpl(nullptr,
                            nullptr,
                            result_prefs,
                            config,
                            nullptr,
                            PlatformOptions::CreateDefault()) {}
  ~FakeSegmentSelectorImpl() override = default;

  void UpdateSelectedSegment(OptimizationTarget new_selection) override {
    new_selection_ = new_selection;
  }

  OptimizationTarget new_selection() const { return new_selection_; }

 private:
  OptimizationTarget new_selection_;
};

class ServiceProxyImplTest : public testing::Test,
                             public ServiceProxy::Observer {
 public:
  ServiceProxyImplTest() = default;
  ~ServiceProxyImplTest() override = default;

  void SetUp() override {
    auto config = std::make_unique<Config>();
    config->segmentation_key = "test";
    configs_.emplace_back(std::move(config));
    pref_service_.registry()->RegisterDictionaryPref(kSegmentationResultPref);
  }

  void SetUpProxy() {
    DCHECK(!db_);
    DCHECK(!segment_db_);

    auto db = std::make_unique<leveldb_proto::test::FakeDB<proto::SegmentInfo>>(
        &db_entries_);
    db_ = db.get();
    segment_db_ = std::make_unique<SegmentInfoDatabase>(std::move(db));

    result_prefs_ = std::make_unique<SegmentationResultPrefs>(&pref_service_);
    segment_selectors_["test"] = std::make_unique<FakeSegmentSelectorImpl>(
        result_prefs_.get(), configs_.at(0).get());
    service_proxy_impl_ = std::make_unique<ServiceProxyImpl>(
        segment_db_.get(), nullptr, &configs_, &segment_selectors_);
    service_proxy_impl_->AddObserver(this);
  }

  void TearDown() override {
    db_entries_.clear();
    db_ = nullptr;
    segment_db_.reset();
    configs_.clear();
    client_info_.clear();
  }

  void OnServiceStatusChanged(bool is_initialized, int status_flag) override {
    is_initialized_ = is_initialized;
    status_flag_ = status_flag;
  }

  void OnClientInfoAvailable(
      const std::vector<ServiceProxy::ClientInfo>& client_info) override {
    client_info_ = client_info;
  }

 protected:
  bool is_initialized_ = false;
  int status_flag_ = 0;

  std::map<std::string, proto::SegmentInfo> db_entries_;
  raw_ptr<leveldb_proto::test::FakeDB<proto::SegmentInfo>> db_{nullptr};
  std::unique_ptr<SegmentInfoDatabase> segment_db_;
  std::unique_ptr<ServiceProxyImpl> service_proxy_impl_;
  std::vector<ServiceProxy::ClientInfo> client_info_;
  std::vector<std::unique_ptr<Config>> configs_;
  base::flat_map<std::string, std::unique_ptr<SegmentSelectorImpl>>
      segment_selectors_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<SegmentationResultPrefs> result_prefs_;
};

TEST_F(ServiceProxyImplTest, GetServiceStatus) {
  SetUpProxy();
  service_proxy_impl_->GetServiceStatus();
  ASSERT_EQ(is_initialized_, false);
  ASSERT_EQ(status_flag_, 0);

  service_proxy_impl_->OnServiceStatusChanged(false, 1);
  ASSERT_EQ(is_initialized_, false);
  ASSERT_EQ(status_flag_, 1);

  service_proxy_impl_->OnServiceStatusChanged(true, 7);
  ASSERT_EQ(is_initialized_, true);
  ASSERT_EQ(status_flag_, 7);

  db_->LoadCallback(true);
  ASSERT_FALSE(client_info_.empty());
  ASSERT_EQ(client_info_.size(), 1u);
}

TEST_F(ServiceProxyImplTest, GetSegmentationInfoFromDB) {
  proto::SegmentInfo info = AddSegmentInfo(
      &db_entries_, configs_.at(0).get(),
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
  SetUpProxy();

  service_proxy_impl_->OnServiceStatusChanged(true, 7);
  db_->LoadCallback(true);
  ASSERT_EQ(client_info_.size(), 1u);
  ASSERT_EQ(client_info_.at(0).segmentation_key, "test");
  ASSERT_EQ(client_info_.at(0).segment_status.size(), 1u);
  ServiceProxy::SegmentStatus status = client_info_.at(0).segment_status.at(0);
  ASSERT_EQ(status.segment_id,
            OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
  ASSERT_EQ(status.can_execute_segment, false);
  ASSERT_TRUE(status.segment_metadata.empty());
  ASSERT_TRUE(status.prediction_result.empty());
}

TEST_F(ServiceProxyImplTest, ExecuteModel) {
  proto::SegmentInfo info = AddSegmentInfo(
      &db_entries_, configs_.at(0).get(),
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
  SetUpProxy();

  service_proxy_impl_->OnServiceStatusChanged(true, 7);
  db_->LoadCallback(true);

  MockModelExecutionScheduler scheduler;
  // Scheduler is not set, ExecuteModel() will do nothing.
  EXPECT_CALL(scheduler, RequestModelExecution(_)).Times(0);
  service_proxy_impl_->ExecuteModel(
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);

  service_proxy_impl_->SetModelExecutionScheduler(&scheduler);
  EXPECT_CALL(scheduler,
              RequestModelExecution(
                  OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB))
      .Times(1);
  service_proxy_impl_->ExecuteModel(
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);

  EXPECT_CALL(scheduler, RequestModelExecution(_)).Times(0);
  service_proxy_impl_->ExecuteModel(
      OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN);
}

TEST_F(ServiceProxyImplTest, OverwriteResult) {
  proto::SegmentInfo info = AddSegmentInfo(
      &db_entries_, configs_.at(0).get(),
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
  SetUpProxy();

  service_proxy_impl_->OnServiceStatusChanged(true, 7);
  db_->LoadCallback(true);

  MockModelExecutionScheduler scheduler;

  // Scheduler is not set, OverwriteValue() will do nothing.
  EXPECT_CALL(scheduler, OnModelExecutionCompleted(_, _)).Times(0);
  service_proxy_impl_->OverwriteResult(
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, 0.7);

  // Test with invalid values.
  service_proxy_impl_->SetModelExecutionScheduler(&scheduler);
  EXPECT_CALL(scheduler, OnModelExecutionCompleted(_, _)).Times(0);
  service_proxy_impl_->OverwriteResult(
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, 1.1);
  service_proxy_impl_->OverwriteResult(
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, -0.1);

  EXPECT_CALL(
      scheduler,
      OnModelExecutionCompleted(
          OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, _))
      .Times(1);
  service_proxy_impl_->OverwriteResult(
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB, 0.7);

  EXPECT_CALL(scheduler, OnModelExecutionCompleted(_, _)).Times(0);
  service_proxy_impl_->OverwriteResult(
      OptimizationTarget::OPTIMIZATION_TARGET_UNKNOWN, 0.7);
}

TEST_F(ServiceProxyImplTest, SetSelectSegment) {
  proto::SegmentInfo info = AddSegmentInfo(
      &db_entries_, configs_.at(0).get(),
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
  SetUpProxy();

  service_proxy_impl_->OnServiceStatusChanged(true, 7);
  db_->LoadCallback(true);

  service_proxy_impl_->SetSelectedSegment(
      "test", OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB);
  ASSERT_EQ(
      OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_NEW_TAB,
      static_cast<FakeSegmentSelectorImpl*>(segment_selectors_["test"].get())
          ->new_selection());
}
}  // namespace segmentation_platform
