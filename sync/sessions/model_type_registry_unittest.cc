// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/deferred_sequenced_task_runner.h"
#include "base/message_loop/message_loop.h"
#include "sync/engine/non_blocking_type_processor.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/sessions/model_type_registry.h"
#include "sync/test/engine/fake_model_worker.h"
#include "sync/test/engine/test_directory_setter_upper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class ModelTypeRegistryTest : public ::testing::Test {
 public:
  ModelTypeRegistryTest();
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  ModelTypeRegistry* registry();

  static DataTypeState MakeInitialDataTypeState(ModelType type) {
    DataTypeState state;
    state.progress_marker.set_data_type_id(
        GetSpecificsFieldNumberFromModelType(type));
    state.next_client_id = 0;
    return state;
  }

 private:
  syncable::Directory* directory();

  base::MessageLoop message_loop_;

  TestDirectorySetterUpper dir_maker_;
  std::vector<scoped_refptr<ModelSafeWorker> > workers_;
  scoped_ptr<ModelTypeRegistry> registry_;
};

ModelTypeRegistryTest::ModelTypeRegistryTest() {}

void ModelTypeRegistryTest::SetUp() {
  dir_maker_.SetUp();
  scoped_refptr<ModelSafeWorker> passive_worker(
      new FakeModelWorker(GROUP_PASSIVE));
  scoped_refptr<ModelSafeWorker> ui_worker(
      new FakeModelWorker(GROUP_UI));
  scoped_refptr<ModelSafeWorker> db_worker(
      new FakeModelWorker(GROUP_DB));
  workers_.push_back(passive_worker);
  workers_.push_back(ui_worker);
  workers_.push_back(db_worker);

  registry_.reset(new ModelTypeRegistry(workers_, directory()));
}

void ModelTypeRegistryTest::TearDown() {
  registry_.reset();
  workers_.clear();
  dir_maker_.TearDown();
}

ModelTypeRegistry* ModelTypeRegistryTest::registry() {
  return registry_.get();
}

syncable::Directory* ModelTypeRegistryTest::directory() {
  return dir_maker_.directory();
}

// Create some directory update handlers and commit contributors.
//
// We don't get to inspect any of the state we're modifying.  This test is
// useful only for detecting crashes or memory leaks.
TEST_F(ModelTypeRegistryTest, SetEnabledDirectoryTypes_Once) {
  ModelSafeRoutingInfo routing_info;
  routing_info.insert(std::make_pair(NIGORI, GROUP_PASSIVE));
  routing_info.insert(std::make_pair(BOOKMARKS, GROUP_UI));
  routing_info.insert(std::make_pair(AUTOFILL, GROUP_DB));

  registry()->SetEnabledDirectoryTypes(routing_info);
}

// Try two different routing info settings.
//
// We don't get to inspect any of the state we're modifying.  This test is
// useful only for detecting crashes or memory leaks.
TEST_F(ModelTypeRegistryTest, SetEnabledDirectoryTypes_Repeatedly) {
  ModelSafeRoutingInfo routing_info1;
  routing_info1.insert(std::make_pair(NIGORI, GROUP_PASSIVE));
  routing_info1.insert(std::make_pair(BOOKMARKS, GROUP_PASSIVE));
  routing_info1.insert(std::make_pair(AUTOFILL, GROUP_PASSIVE));

  registry()->SetEnabledDirectoryTypes(routing_info1);

  ModelSafeRoutingInfo routing_info2;
  routing_info2.insert(std::make_pair(NIGORI, GROUP_PASSIVE));
  routing_info2.insert(std::make_pair(BOOKMARKS, GROUP_UI));
  routing_info2.insert(std::make_pair(AUTOFILL, GROUP_DB));

  registry()->SetEnabledDirectoryTypes(routing_info2);
}

// Test removing all types from the list.
//
// We don't get to inspect any of the state we're modifying.  This test is
// useful only for detecting crashes or memory leaks.
TEST_F(ModelTypeRegistryTest, SetEnabledDirectoryTypes_Clear) {
  ModelSafeRoutingInfo routing_info1;
  routing_info1.insert(std::make_pair(NIGORI, GROUP_PASSIVE));
  routing_info1.insert(std::make_pair(BOOKMARKS, GROUP_UI));
  routing_info1.insert(std::make_pair(AUTOFILL, GROUP_DB));

  registry()->SetEnabledDirectoryTypes(routing_info1);

  ModelSafeRoutingInfo routing_info2;
  registry()->SetEnabledDirectoryTypes(routing_info2);
}

// Test disabling then re-enabling some directory types.
//
// We don't get to inspect any of the state we're modifying.  This test is
// useful only for detecting crashes or memory leaks.
TEST_F(ModelTypeRegistryTest, SetEnabledDirectoryTypes_OffAndOn) {
  ModelSafeRoutingInfo routing_info1;
  routing_info1.insert(std::make_pair(NIGORI, GROUP_PASSIVE));
  routing_info1.insert(std::make_pair(BOOKMARKS, GROUP_UI));
  routing_info1.insert(std::make_pair(AUTOFILL, GROUP_DB));

  registry()->SetEnabledDirectoryTypes(routing_info1);

  ModelSafeRoutingInfo routing_info2;
  registry()->SetEnabledDirectoryTypes(routing_info2);

  registry()->SetEnabledDirectoryTypes(routing_info1);
}

TEST_F(ModelTypeRegistryTest, NonBlockingTypes) {
  NonBlockingTypeProcessor themes_processor(syncer::THEMES);
  NonBlockingTypeProcessor sessions_processor(syncer::SESSIONS);
  scoped_refptr<base::DeferredSequencedTaskRunner> task_runner =
      new base::DeferredSequencedTaskRunner(base::MessageLoopProxy::current());

  EXPECT_TRUE(registry()->GetEnabledTypes().Empty());

  registry()->InitializeNonBlockingType(syncer::THEMES,
                                        MakeInitialDataTypeState(THEMES),
                                        task_runner,
                                        themes_processor.AsWeakPtrForUI());
  EXPECT_TRUE(registry()->GetEnabledTypes().Equals(
      ModelTypeSet(syncer::THEMES)));

  registry()->InitializeNonBlockingType(syncer::SESSIONS,
                                        MakeInitialDataTypeState(SESSIONS),
                                        task_runner,
                                        sessions_processor.AsWeakPtrForUI());
  EXPECT_TRUE(registry()->GetEnabledTypes().Equals(
      ModelTypeSet(syncer::THEMES, syncer::SESSIONS)));

  registry()->RemoveNonBlockingType(syncer::THEMES);
  EXPECT_TRUE(registry()->GetEnabledTypes().Equals(
      ModelTypeSet(syncer::SESSIONS)));

  // Allow ModelTypeRegistry destruction to delete the
  // Sessions' NonBlockingTypeProcessorCore.
}

TEST_F(ModelTypeRegistryTest, NonBlockingTypesWithDirectoryTypes) {
  NonBlockingTypeProcessor themes_processor(syncer::THEMES);
  NonBlockingTypeProcessor sessions_processor(syncer::SESSIONS);
  scoped_refptr<base::DeferredSequencedTaskRunner> task_runner =
      new base::DeferredSequencedTaskRunner(base::MessageLoopProxy::current());

  ModelSafeRoutingInfo routing_info1;
  routing_info1.insert(std::make_pair(NIGORI, GROUP_PASSIVE));
  routing_info1.insert(std::make_pair(BOOKMARKS, GROUP_UI));
  routing_info1.insert(std::make_pair(AUTOFILL, GROUP_DB));

  ModelTypeSet current_types;
  EXPECT_TRUE(registry()->GetEnabledTypes().Empty());

  // Add the themes non-blocking type.
  registry()->InitializeNonBlockingType(syncer::THEMES,
                                        MakeInitialDataTypeState(THEMES),
                                        task_runner,
                                        themes_processor.AsWeakPtrForUI());
  current_types.Put(syncer::THEMES);
  EXPECT_TRUE(registry()->GetEnabledTypes().Equals(current_types));

  // Add some directory types.
  registry()->SetEnabledDirectoryTypes(routing_info1);
  current_types.PutAll(GetRoutingInfoTypes(routing_info1));
  EXPECT_TRUE(registry()->GetEnabledTypes().Equals(current_types));

  // Add sessions non-blocking type.
  registry()->InitializeNonBlockingType(syncer::SESSIONS,
                                        MakeInitialDataTypeState(SESSIONS),
                                        task_runner,
                                        sessions_processor.AsWeakPtrForUI());
  current_types.Put(syncer::SESSIONS);
  EXPECT_TRUE(registry()->GetEnabledTypes().Equals(current_types));

  // Remove themes non-blocking type.
  registry()->RemoveNonBlockingType(syncer::THEMES);
  current_types.Remove(syncer::THEMES);
  EXPECT_TRUE(registry()->GetEnabledTypes().Equals(current_types));

  // Clear all directory types.
  ModelSafeRoutingInfo routing_info2;
  registry()->SetEnabledDirectoryTypes(routing_info2);
  current_types.RemoveAll(GetRoutingInfoTypes(routing_info1));
  EXPECT_TRUE(registry()->GetEnabledTypes().Equals(current_types));
}

TEST_F(ModelTypeRegistryTest, DeletionOrdering) {
  scoped_ptr<NonBlockingTypeProcessor> themes_processor(
      new NonBlockingTypeProcessor(syncer::THEMES));
  scoped_ptr<NonBlockingTypeProcessor> sessions_processor(
      new NonBlockingTypeProcessor(syncer::SESSIONS));
  scoped_refptr<base::DeferredSequencedTaskRunner> task_runner =
      new base::DeferredSequencedTaskRunner(base::MessageLoopProxy::current());

  EXPECT_TRUE(registry()->GetEnabledTypes().Empty());

  registry()->InitializeNonBlockingType(syncer::THEMES,
                                        MakeInitialDataTypeState(THEMES),
                                        task_runner,
                                        themes_processor->AsWeakPtrForUI());
  registry()->InitializeNonBlockingType(syncer::SESSIONS,
                                        MakeInitialDataTypeState(SESSIONS),
                                        task_runner,
                                        sessions_processor->AsWeakPtrForUI());
  EXPECT_TRUE(registry()->GetEnabledTypes().Equals(
      ModelTypeSet(syncer::THEMES, syncer::SESSIONS)));

  // Tear down themes processing, starting with the ProcessorCore.
  registry()->RemoveNonBlockingType(syncer::THEMES);
  themes_processor.reset();

  // Tear down sessions processing, starting with the Processor.
  sessions_processor.reset();
  registry()->RemoveNonBlockingType(syncer::SESSIONS);

  EXPECT_TRUE(registry()->GetEnabledTypes().Empty());
}

}  // namespace syncer
