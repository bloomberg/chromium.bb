// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/model_load_manager.h"

#include <memory>

#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/driver/fake_data_type_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace syncer {

namespace {

ConfigureContext BuildConfigureContext(SyncMode sync_mode = SyncMode::kFull) {
  ConfigureContext context;
  context.sync_mode = sync_mode;
  context.cache_guid = "test_cache_guid";
  return context;
}

}  // namespace

class MockModelLoadManagerDelegate : public ModelLoadManagerDelegate {
 public:
  MockModelLoadManagerDelegate() = default;
  ~MockModelLoadManagerDelegate() override = default;
  MOCK_METHOD(void, OnAllDataTypesReadyForConfigure, (), (override));
  MOCK_METHOD(void,
              OnSingleDataTypeWillStop,
              (ModelType, const SyncError& error),
              (override));
};

class SyncModelLoadManagerTest : public testing::Test {
 public:
  SyncModelLoadManagerTest() = default;

  FakeDataTypeController* GetController(ModelType model_type) {
    auto it = controllers_.find(model_type);
    if (it == controllers_.end()) {
      return nullptr;
    }
    return static_cast<FakeDataTypeController*>(it->second.get());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
  testing::NiceMock<MockModelLoadManagerDelegate> delegate_;
  DataTypeController::TypeMap controllers_;
};

// Start a type and make sure ModelLoadManager callst the |Start|
// method and calls the callback when it is done.
TEST_F(SyncModelLoadManagerTest, SimpleModelStart) {
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  controllers_[APPS] = std::make_unique<FakeDataTypeController>(APPS);
  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  ModelTypeSet types(BOOKMARKS, APPS);
  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure());

  EXPECT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::NOT_RUNNING);
  EXPECT_EQ(GetController(APPS)->state(), DataTypeController::NOT_RUNNING);

  // Initialize() kicks off model loading.
  model_load_manager.Initialize(/*desired_types=*/types,
                                /*preferred_types=*/types,
                                BuildConfigureContext());

  EXPECT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
  EXPECT_EQ(GetController(APPS)->state(), DataTypeController::MODEL_LOADED);
}

// Start a type, let it finish and then call stop.
TEST_F(SyncModelLoadManagerTest, StopAfterFinish) {
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  ModelTypeSet types;
  types.Put(BOOKMARKS);
  EXPECT_CALL(delegate_, OnSingleDataTypeWillStop(BOOKMARKS, _));

  model_load_manager.Initialize(/*desired_types=*/types,
                                /*preferred_types=*/types,
                                BuildConfigureContext());

  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);

  model_load_manager.Stop(ShutdownReason::STOP_SYNC_AND_KEEP_DATA);
  EXPECT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::NOT_RUNNING);
  EXPECT_EQ(0, GetController(BOOKMARKS)->model()->clear_metadata_call_count());
}

// Test that a model that failed to load is reported and stopped properly.
TEST_F(SyncModelLoadManagerTest, ModelLoadFail) {
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  GetController(BOOKMARKS)->model()->SimulateModelError(
      ModelError(FROM_HERE, "Test error"));
  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  ModelTypeSet types;
  types.Put(BOOKMARKS);
  EXPECT_CALL(delegate_, OnSingleDataTypeWillStop(BOOKMARKS, _));

  model_load_manager.Initialize(/*desired_types=*/types,
                                /*preferred_types=*/types,
                                BuildConfigureContext());

  EXPECT_EQ(DataTypeController::FAILED, GetController(BOOKMARKS)->state());
}

// Test that a runtime error is handled by stopping the type.
TEST_F(SyncModelLoadManagerTest, StopAfterConfiguration) {
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  ModelTypeSet types;
  types.Put(BOOKMARKS);

  model_load_manager.Initialize(/*desired_types=*/types,
                                /*preferred_types=*/types,
                                BuildConfigureContext());

  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);

  testing::Mock::VerifyAndClearExpectations(&delegate_);
  EXPECT_CALL(delegate_, OnSingleDataTypeWillStop(BOOKMARKS, _));
  GetController(BOOKMARKS)->model()->SimulateModelError(
      ModelError(FROM_HERE, "Test error"));
}

// Test that OnAllDataTypesReadyForConfigure is called when all datatypes that
// require LoadModels before configuration are loaded.
TEST_F(SyncModelLoadManagerTest, OnAllDataTypesReadyForConfigure) {
  // Create two controllers with delayed model load.
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  controllers_[APPS] = std::make_unique<FakeDataTypeController>(APPS);
  GetController(BOOKMARKS)->model()->EnableManualModelStart();
  GetController(APPS)->model()->EnableManualModelStart();

  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  ModelTypeSet types(BOOKMARKS, APPS);
  // OnAllDataTypesReadyForConfigure shouldn't be called, APPS data type is not
  // loaded yet.
  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure()).Times(0);

  model_load_manager.Initialize(/*desired_types=*/types,
                                /*preferred_types=*/types,
                                BuildConfigureContext());

  EXPECT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_STARTING);
  EXPECT_EQ(GetController(APPS)->state(), DataTypeController::MODEL_STARTING);

  // Finish loading BOOKMARKS, but APPS are still loading.
  GetController(BOOKMARKS)->model()->SimulateModelStartFinished();
  EXPECT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);

  testing::Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure());
  // Finish loading APPS. This should trigger OnAllDataTypesReadyForConfigure.
  GetController(APPS)->model()->SimulateModelStartFinished();
  EXPECT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
  EXPECT_EQ(GetController(APPS)->state(), DataTypeController::MODEL_LOADED);

  // Call ModelLoadManager::Initialize with reduced set of datatypes.
  // All datatypes in reduced set are already loaded.
  // OnAllDataTypesReadyForConfigure() should be called.
  testing::Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure());

  ModelTypeSet reduced_types(APPS);
  model_load_manager.Initialize(/*desired_types=*/reduced_types,
                                /*preferred_types=*/reduced_types,
                                BuildConfigureContext());

  EXPECT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::NOT_RUNNING);
  EXPECT_EQ(GetController(APPS)->state(), DataTypeController::MODEL_LOADED);
  EXPECT_EQ(1, GetController(BOOKMARKS)->model()->clear_metadata_call_count());
}

// Test that OnAllDataTypesReadyForConfigure() is called correctly after
// LoadModels fails for one of datatypes.
TEST_F(SyncModelLoadManagerTest,
       OnAllDataTypesReadyForConfigure_FailedLoadModels) {
  controllers_[APPS] = std::make_unique<FakeDataTypeController>(APPS);
  GetController(APPS)->model()->EnableManualModelStart();

  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  ModelTypeSet types(APPS);
  // OnAllDataTypesReadyForConfigure shouldn't be called, APPS data type is not
  // loaded yet.
  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure()).Times(0);

  model_load_manager.Initialize(/*desired_types=*/types,
                                /*preferred_types=*/types,
                                BuildConfigureContext());

  EXPECT_EQ(GetController(APPS)->state(), DataTypeController::MODEL_STARTING);

  testing::Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure());
  // Simulate model load error for APPS and finish loading it. This should
  // trigger OnAllDataTypesReadyForConfigure.
  GetController(APPS)->model()->SimulateModelError(
      ModelError(FROM_HERE, "Test error"));
  EXPECT_EQ(GetController(APPS)->state(), DataTypeController::FAILED);
}

// Test that if one of the types fails while another is still being loaded then
// OnAllDataTypesReadyForConfgiure is still called correctly.
TEST_F(SyncModelLoadManagerTest,
       OnAllDataTypesReadyForConfigure_TypeFailedAfterLoadModels) {
  // Create two controllers with delayed model load. Both should block
  // configuration.
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  controllers_[APPS] = std::make_unique<FakeDataTypeController>(APPS);
  GetController(BOOKMARKS)->model()->EnableManualModelStart();
  GetController(APPS)->model()->EnableManualModelStart();

  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  ModelTypeSet types(BOOKMARKS, APPS);

  // Apps will finish loading but bookmarks won't.
  // OnAllDataTypesReadyForConfigure shouldn't be called.
  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure()).Times(0);

  model_load_manager.Initialize(/*desired_types=*/types,
                                /*preferred_types=*/types,
                                BuildConfigureContext());

  GetController(APPS)->model()->SimulateModelStartFinished();

  EXPECT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_STARTING);
  EXPECT_EQ(GetController(APPS)->state(), DataTypeController::MODEL_LOADED);

  testing::Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure()).Times(0);

  EXPECT_CALL(delegate_, OnSingleDataTypeWillStop(APPS, _));
  // Apps datatype reports failure.
  GetController(APPS)->model()->SimulateModelError(
      ModelError(FROM_HERE, "Test error"));

  testing::Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure());
  // Finish loading BOOKMARKS. This should trigger
  // OnAllDataTypesReadyForConfigure().
  GetController(BOOKMARKS)->model()->SimulateModelStartFinished();
  EXPECT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
}

// Test that Stop clears metadata for disabled type.
TEST_F(SyncModelLoadManagerTest, StopClearMetadata) {
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  ModelLoadManager model_load_manager(&controllers_, &delegate_);

  ASSERT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::NOT_RUNNING);

  ModelTypeSet types(BOOKMARKS);

  // Initialize() kicks off model loading.
  model_load_manager.Initialize(/*desired_types=*/types,
                                /*preferred_types=*/types,
                                BuildConfigureContext());

  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);

  model_load_manager.Stop(ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA);

  EXPECT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::NOT_RUNNING);
  EXPECT_EQ(1, GetController(BOOKMARKS)->model()->clear_metadata_call_count());
}

// Test that stopping a single type clears the metadata for the disabled type.
TEST_F(SyncModelLoadManagerTest, StopDataType) {
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  ModelLoadManager model_load_manager(&controllers_, &delegate_);

  ASSERT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::NOT_RUNNING);

  // Initialize() kicks off model loading.
  model_load_manager.Initialize(
      /*desired_types=*/ModelTypeSet(BOOKMARKS),
      /*preferred_types=*/ModelTypeSet(BOOKMARKS), BuildConfigureContext());

  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);

  model_load_manager.StopDatatype(
      BOOKMARKS, ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA,
      SyncError(FROM_HERE, syncer::SyncError::UNREADY_ERROR,
                "Data type is unready.", BOOKMARKS));

  EXPECT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::NOT_RUNNING);
  EXPECT_EQ(1, GetController(BOOKMARKS)->model()->clear_metadata_call_count());
}

// Test that stopping a single type is ignored when the type is not running.
TEST_F(SyncModelLoadManagerTest, StopDataType_NotRunning) {
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  ModelLoadManager model_load_manager(&controllers_, &delegate_);

  ASSERT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::NOT_RUNNING);

  model_load_manager.StopDatatype(
      BOOKMARKS, ShutdownReason::DISABLE_SYNC_AND_CLEAR_DATA,
      SyncError(FROM_HERE, syncer::SyncError::UNREADY_ERROR,
                "Data type is unready.", BOOKMARKS));

  // The state should still be not running.
  EXPECT_EQ(GetController(BOOKMARKS)->state(), DataTypeController::NOT_RUNNING);
}

// Test that Initialize stops controllers with KEEP_METADATA for preferred
// types.
TEST_F(SyncModelLoadManagerTest, KeepsMetadataForPreferredDataType) {
  // Initialize the manager with two data types.
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  controllers_[APPS] = std::make_unique<FakeDataTypeController>(APPS);
  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  ModelTypeSet preferred_types(BOOKMARKS, APPS);
  ModelTypeSet desired_types = preferred_types;

  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure());

  model_load_manager.Initialize(desired_types, preferred_types,
                                BuildConfigureContext());

  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
  ASSERT_EQ(GetController(APPS)->state(), DataTypeController::MODEL_LOADED);
  testing::Mock::VerifyAndClearExpectations(&delegate_);

  // Stop one data type without disabling sync.
  desired_types.Remove(APPS);

  EXPECT_CALL(delegate_, OnSingleDataTypeWillStop(APPS, _));
  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure());

  model_load_manager.Initialize(desired_types, preferred_types,
                                BuildConfigureContext());

  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
  ASSERT_EQ(GetController(APPS)->state(), DataTypeController::NOT_RUNNING);
  EXPECT_EQ(0, GetController(APPS)->model()->clear_metadata_call_count());
}

// Test that Initialize stops controllers with CLEAR_METADATA for
// no-longer-preferred types.
TEST_F(SyncModelLoadManagerTest, ClearsMetadataForNotPreferredDataType) {
  // Initialize the manager with two data types.
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(BOOKMARKS);
  controllers_[APPS] = std::make_unique<FakeDataTypeController>(APPS);
  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  ModelTypeSet preferred_types(BOOKMARKS, APPS);
  ModelTypeSet desired_types = preferred_types;

  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure());

  model_load_manager.Initialize(desired_types, preferred_types,
                                BuildConfigureContext());

  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
  ASSERT_EQ(GetController(APPS)->state(), DataTypeController::MODEL_LOADED);
  testing::Mock::VerifyAndClearExpectations(&delegate_);

  // Disable one data type.
  preferred_types.Remove(APPS);
  desired_types.Remove(APPS);

  EXPECT_CALL(delegate_, OnSingleDataTypeWillStop(APPS, _));
  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure());

  model_load_manager.Initialize(desired_types, preferred_types,
                                BuildConfigureContext());

  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
  ASSERT_EQ(GetController(APPS)->state(), DataTypeController::NOT_RUNNING);
  EXPECT_EQ(1, GetController(APPS)->model()->clear_metadata_call_count());
}

TEST_F(SyncModelLoadManagerTest, SwitchFromOnDiskToInMemoryRestartsTypes) {
  // Initialize the manager with two data types.
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(
      BOOKMARKS, /*enable_transport_only_model=*/true);
  controllers_[APPS] = std::make_unique<FakeDataTypeController>(
      APPS, /*enable_transport_only_model=*/true);

  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  ModelTypeSet preferred_types(BOOKMARKS, APPS);
  ModelTypeSet desired_types = preferred_types;

  ConfigureContext configure_context;
  configure_context.sync_mode = SyncMode::kFull;
  configure_context.cache_guid = "test_cache_guid";

  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure());

  model_load_manager.Initialize(desired_types, preferred_types,
                                configure_context);

  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
  ASSERT_EQ(GetController(APPS)->state(), DataTypeController::MODEL_LOADED);
  testing::Mock::VerifyAndClearExpectations(&delegate_);

  // Switch to in-memory storage.
  configure_context.sync_mode = SyncMode::kTransportOnly;
  desired_types.Remove(APPS);
  preferred_types.Remove(APPS);

  // Data types should get restarted.
  EXPECT_CALL(delegate_, OnSingleDataTypeWillStop(APPS, _));
  EXPECT_CALL(delegate_, OnSingleDataTypeWillStop(BOOKMARKS, _));
  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure());

  model_load_manager.Initialize(desired_types, preferred_types,
                                configure_context);

  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
  ASSERT_EQ(GetController(APPS)->state(), DataTypeController::NOT_RUNNING);
  // Since we switched to in-memory storage, the metadata for the now-disabled
  // type should NOT get cleared.
  EXPECT_EQ(0, GetController(APPS)->model()->clear_metadata_call_count());
}

TEST_F(SyncModelLoadManagerTest,
       SwitchFromTransportOnlyToFullSyncRestartsTypes) {
  // Initialize the manager with two data types.
  controllers_[BOOKMARKS] = std::make_unique<FakeDataTypeController>(
      BOOKMARKS, /*enable_transport_only_model=*/true);
  controllers_[APPS] = std::make_unique<FakeDataTypeController>(
      APPS, /*enable_transport_only_model=*/true);

  ModelLoadManager model_load_manager(&controllers_, &delegate_);
  ModelTypeSet preferred_types(BOOKMARKS, APPS);
  ModelTypeSet desired_types = preferred_types;

  ConfigureContext configure_context;
  configure_context.sync_mode = SyncMode::kTransportOnly;
  configure_context.cache_guid = "test_cache_guid";

  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure());

  model_load_manager.Initialize(desired_types, preferred_types,
                                configure_context);

  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
  ASSERT_EQ(GetController(APPS)->state(), DataTypeController::MODEL_LOADED);
  testing::Mock::VerifyAndClearExpectations(&delegate_);

  // Switch to full-sync mode.
  configure_context.sync_mode = SyncMode::kFull;
  desired_types.Remove(APPS);
  preferred_types.Remove(APPS);

  // Data types should get restarted.
  EXPECT_CALL(delegate_, OnSingleDataTypeWillStop(APPS, _));
  EXPECT_CALL(delegate_, OnSingleDataTypeWillStop(BOOKMARKS, _));
  EXPECT_CALL(delegate_, OnAllDataTypesReadyForConfigure());

  model_load_manager.Initialize(desired_types, preferred_types,
                                configure_context);

  ASSERT_EQ(GetController(BOOKMARKS)->state(),
            DataTypeController::MODEL_LOADED);
  ASSERT_EQ(GetController(APPS)->state(), DataTypeController::NOT_RUNNING);
  // The metadata for the now-disabled type should get cleared.
  EXPECT_EQ(1, GetController(APPS)
                   ->model(SyncMode::kTransportOnly)
                   ->clear_metadata_call_count());
}

}  // namespace syncer
