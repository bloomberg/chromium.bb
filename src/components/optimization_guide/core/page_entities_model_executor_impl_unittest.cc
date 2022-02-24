// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/page_entities_model_executor_impl.h"

#include "base/observer_list.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/page_entities_model_metadata.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

using ::testing::ElementsAre;

class ModelObserverTracker : public TestOptimizationGuideModelProvider {
 public:
  void AddObserverForOptimizationTargetModel(
      proto::OptimizationTarget target,
      const absl::optional<proto::Any>& model_metadata,
      OptimizationTargetModelObserver* observer) override {
    registered_model_metadata_.insert_or_assign(target, model_metadata);
    registered_observers_.AddObserver(observer);
  }

  void RemoveObserverForOptimizationTargetModel(
      proto::OptimizationTarget target,
      OptimizationTargetModelObserver* observer) override {
    registered_observers_.RemoveObserver(observer);
  }

  bool DidRegisterForTarget(
      proto::OptimizationTarget target,
      absl::optional<proto::Any>* out_model_metadata) const {
    auto it = registered_model_metadata_.find(target);
    if (it == registered_model_metadata_.end())
      return false;
    *out_model_metadata = registered_model_metadata_.at(target);
    return true;
  }

  void PushModelInfoToObservers(const ModelInfo& model_info) {
    for (auto& observer : registered_observers_) {
      observer.OnModelUpdated(proto::OPTIMIZATION_TARGET_PAGE_ENTITIES,
                              model_info);
    }
  }

 private:
  base::flat_map<proto::OptimizationTarget, absl::optional<proto::Any>>
      registered_model_metadata_;
  base::ObserverList<OptimizationTargetModelObserver> registered_observers_;
};

class PageEntitiesModelExecutorImplTest : public testing::Test {
 public:
  void SetUp() override {
    model_observer_tracker_ = std::make_unique<ModelObserverTracker>();
    model_executor_ = std::make_unique<PageEntitiesModelExecutorImpl>(
        model_observer_tracker_.get());

    // Wait for PageEntitiesModelExecutor to set everything up.
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    model_executor_.reset();
    model_observer_tracker_.reset();

    // Wait for PageEntitiesModelExecutor to clean everything up.
    task_environment_.RunUntilIdle();
  }

  absl::optional<std::vector<ScoredEntityMetadata>> ExecuteHumanReadableModel(
      const std::string& text) {
    absl::optional<std::vector<ScoredEntityMetadata>> entity_metadata;

    base::RunLoop run_loop;
    model_executor_->HumanReadableExecuteModelWithInput(
        text, base::BindOnce(
                  [](base::RunLoop* run_loop,
                     absl::optional<std::vector<ScoredEntityMetadata>>*
                         out_entity_metadata,
                     const absl::optional<std::vector<ScoredEntityMetadata>>&
                         entity_metadata) {
                    *out_entity_metadata = entity_metadata;
                    run_loop->Quit();
                  },
                  &run_loop, &entity_metadata));
    run_loop.Run();

    // Sort the result by score to make validating the output easier.
    if (entity_metadata) {
      std::sort(
          entity_metadata->begin(), entity_metadata->end(),
          [](const ScoredEntityMetadata& a, const ScoredEntityMetadata& b) {
            return a.score > b.score;
          });
    }
    return entity_metadata;
  }

  absl::optional<EntityMetadata> GetMetadataForEntityId(
      const std::string& entity_id) {
    absl::optional<EntityMetadata> entity_metadata;

    base::RunLoop run_loop;
    model_executor_->GetMetadataForEntityId(
        entity_id,
        base::BindOnce(
            [](base::RunLoop* run_loop,
               absl::optional<EntityMetadata>* out_entity_metadata,
               const absl::optional<EntityMetadata>& entity_metadata) {
              *out_entity_metadata = entity_metadata;
              run_loop->Quit();
            },
            &run_loop, &entity_metadata));
    run_loop.Run();

    return entity_metadata;
  }

  ModelObserverTracker* model_observer_tracker() const {
    return model_observer_tracker_.get();
  }

  base::FilePath GetModelTestDataDir() {
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    return source_root_dir.AppendASCII("components")
        .AppendASCII("optimization_guide")
        .AppendASCII("internal")
        .AppendASCII("testdata");
  }

  void PushModelInfoToObservers(const ModelInfo& model_info) {
    model_observer_tracker_->PushModelInfoToObservers(model_info);
    task_environment_.RunUntilIdle();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ModelObserverTracker> model_observer_tracker_;
  std::unique_ptr<PageEntitiesModelExecutorImpl> model_executor_;
};

TEST_F(PageEntitiesModelExecutorImplTest, CreateNoMetadata) {
  std::unique_ptr<ModelInfo> model_info = TestModelInfoBuilder().Build();
  ASSERT_TRUE(model_info);
  PushModelInfoToObservers(*model_info);

  // We expect that there will be no model to evaluate even for this input that
  // has output in the test model.
  EXPECT_EQ(ExecuteHumanReadableModel("Taylor Swift singer"), absl::nullopt);
}

TEST_F(PageEntitiesModelExecutorImplTest, CreateMetadataWrongType) {
  proto::Any any;
  any.set_type_url(any.GetTypeName());
  proto::FieldTrial garbage;
  garbage.SerializeToString(any.mutable_value());

  proto::PredictionModel model;
  model.mutable_model()->set_download_url(
      FilePathToString(GetModelTestDataDir().AppendASCII("model.tflite")));
  model.mutable_model_info()->set_version(123);
  *model.mutable_model_info()->mutable_model_metadata() = any;
  std::unique_ptr<ModelInfo> model_info = ModelInfo::Create(model);
  ASSERT_TRUE(model_info);
  PushModelInfoToObservers(*model_info);

  // We expect that there will be no model to evaluate even for this input that
  // has output in the test model.
  EXPECT_EQ(ExecuteHumanReadableModel("Taylor Swift singer"), absl::nullopt);
}

TEST_F(PageEntitiesModelExecutorImplTest, CreateNoSlices) {
  proto::Any any;
  proto::PageEntitiesModelMetadata metadata;
  any.set_type_url(metadata.GetTypeName());
  metadata.SerializeToString(any.mutable_value());

  proto::PredictionModel model;
  model.mutable_model()->set_download_url(
      FilePathToString(GetModelTestDataDir().AppendASCII("model.tflite")));
  model.mutable_model_info()->set_version(123);
  *model.mutable_model_info()->mutable_model_metadata() = any;
  std::unique_ptr<ModelInfo> model_info = ModelInfo::Create(model);
  ASSERT_TRUE(model_info);
  PushModelInfoToObservers(*model_info);

  // We expect that there will be no model to evaluate even for this input that
  // has output in the test model.
  EXPECT_EQ(ExecuteHumanReadableModel("Taylor Swift singer"), absl::nullopt);
}

TEST_F(PageEntitiesModelExecutorImplTest, CreateMissingFiles) {
  proto::Any any;
  proto::PageEntitiesModelMetadata metadata;
  metadata.add_slice("global");
  any.set_type_url(metadata.GetTypeName());
  metadata.SerializeToString(any.mutable_value());

  base::FilePath dir_path = GetModelTestDataDir();
  base::flat_set<std::string> expected_additional_files = {
      FilePathToString(dir_path.AppendASCII("model_metadata.pb")),
      FilePathToString(dir_path.AppendASCII("word_embeddings")),
      FilePathToString(dir_path.AppendASCII("global-entities_names")),
      FilePathToString(dir_path.AppendASCII("global-entities_metadata")),
      FilePathToString(dir_path.AppendASCII("global-entities_names_filter")),
      FilePathToString(dir_path.AppendASCII("global-entities_prefixes_filter")),
  };
  // Remove one file for each iteration and make sure it fails.
  for (const auto& missing_file_name : expected_additional_files) {
    // Make a copy of the expected files and remove the one file from the set.
    base::flat_set<std::string> additional_files = expected_additional_files;
    additional_files.erase(missing_file_name);

    proto::PredictionModel model;
    model.mutable_model()->set_download_url(
        FilePathToString(dir_path.AppendASCII("model.tflite")));
    model.mutable_model_info()->set_version(123);
    *model.mutable_model_info()->mutable_model_metadata() = any;
    for (const auto& additional_file : additional_files) {
      model.mutable_model_info()->add_additional_files()->set_file_path(
          additional_file);
    }
    std::unique_ptr<ModelInfo> model_info = ModelInfo::Create(model);
    ASSERT_TRUE(model_info);
    PushModelInfoToObservers(*model_info);

    // We expect that there will be no model to evaluate even for this input
    // that has output in the test model.
    EXPECT_EQ(ExecuteHumanReadableModel("Taylor Swift singer"), absl::nullopt);
  }
}

TEST_F(PageEntitiesModelExecutorImplTest, GetMetadataForEntityIdNoModel) {
  EXPECT_EQ(GetMetadataForEntityId("/m/0dl567"), absl::nullopt);
}

TEST_F(PageEntitiesModelExecutorImplTest, ExecuteHumanReadableModelNoModel) {
  EXPECT_EQ(ExecuteHumanReadableModel("Taylor Swift singer"), absl::nullopt);
}

TEST_F(PageEntitiesModelExecutorImplTest,
       SetsUpModelCorrectlyBasedOnFeatureParams) {
  absl::optional<proto::Any> registered_model_metadata;
  EXPECT_TRUE(model_observer_tracker()->DidRegisterForTarget(
      proto::OPTIMIZATION_TARGET_PAGE_ENTITIES, &registered_model_metadata));
  EXPECT_TRUE(registered_model_metadata.has_value());
  absl::optional<proto::PageEntitiesModelMetadata>
      page_entities_model_metadata =
          ParsedAnyMetadata<proto::PageEntitiesModelMetadata>(
              *registered_model_metadata);
  EXPECT_TRUE(page_entities_model_metadata.has_value());
}

}  // namespace
}  // namespace optimization_guide
