// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/model_type_registry.h"

#include <utility>
#include <vector>

#include "base/deferred_sequenced_task_runner.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "sync/internal_api/public/activation_context.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/shared_model_type_processor.h"
#include "sync/internal_api/public/test/fake_model_type_service.h"
#include "sync/protocol/data_type_state.pb.h"
#include "sync/test/engine/fake_model_worker.h"
#include "sync/test/engine/mock_nudge_handler.h"
#include "sync/test/engine/test_directory_setter_upper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class ModelTypeRegistryTest : public ::testing::Test,
                              syncer_v2::FakeModelTypeService {
 public:
  ModelTypeRegistryTest();
  void SetUp() override;
  void TearDown() override;

  ModelTypeRegistry* registry();

  static sync_pb::DataTypeState MakeInitialDataTypeState(ModelType type) {
    sync_pb::DataTypeState state;
    state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromModelType(type));
    return state;
  }

  static std::unique_ptr<syncer_v2::ActivationContext> MakeActivationContext(
      const sync_pb::DataTypeState& data_type_state,
      std::unique_ptr<syncer_v2::ModelTypeProcessor> type_processor) {
    std::unique_ptr<syncer_v2::ActivationContext> context =
        base::WrapUnique(new syncer_v2::ActivationContext);
    context->data_type_state = data_type_state;
    context->type_processor = std::move(type_processor);
    return context;
  }

 protected:
  std::unique_ptr<syncer_v2::SharedModelTypeProcessor> MakeModelTypeProcessor(
      ModelType type) {
    return base::WrapUnique(
        new syncer_v2::SharedModelTypeProcessor(type, this));
  }

 private:
  syncable::Directory* directory();

  base::MessageLoop message_loop_;

  TestDirectorySetterUpper dir_maker_;
  std::vector<scoped_refptr<ModelSafeWorker> > workers_;
  std::unique_ptr<ModelTypeRegistry> registry_;
  MockNudgeHandler mock_nudge_handler_;
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

  registry_.reset(
      new ModelTypeRegistry(workers_, directory(), &mock_nudge_handler_));
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
  routing_info.insert(std::make_pair(APPS, GROUP_NON_BLOCKING));

  registry()->SetEnabledDirectoryTypes(routing_info);

  UpdateHandlerMap* update_handler_map = registry()->update_handler_map();
  // Apps is non-blocking type, SetEnabledDirectoryTypes shouldn't instantiate
  // update_handler for it.
  EXPECT_TRUE(update_handler_map->find(APPS) == update_handler_map->end());
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
  routing_info1.insert(std::make_pair(APPS, GROUP_NON_BLOCKING));

  registry()->SetEnabledDirectoryTypes(routing_info1);

  ModelSafeRoutingInfo routing_info2;
  routing_info2.insert(std::make_pair(NIGORI, GROUP_PASSIVE));
  routing_info2.insert(std::make_pair(BOOKMARKS, GROUP_UI));
  routing_info2.insert(std::make_pair(AUTOFILL, GROUP_DB));
  routing_info2.insert(std::make_pair(APPS, GROUP_NON_BLOCKING));

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
  routing_info1.insert(std::make_pair(APPS, GROUP_NON_BLOCKING));

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
  routing_info1.insert(std::make_pair(APPS, GROUP_NON_BLOCKING));

  registry()->SetEnabledDirectoryTypes(routing_info1);

  ModelSafeRoutingInfo routing_info2;
  registry()->SetEnabledDirectoryTypes(routing_info2);

  registry()->SetEnabledDirectoryTypes(routing_info1);
}

TEST_F(ModelTypeRegistryTest, NonBlockingTypes) {
  std::unique_ptr<syncer_v2::SharedModelTypeProcessor> themes_sync_processor =
      MakeModelTypeProcessor(syncer::THEMES);
  std::unique_ptr<syncer_v2::SharedModelTypeProcessor> sessions_sync_processor =
      MakeModelTypeProcessor(syncer::SESSIONS);

  EXPECT_TRUE(registry()->GetEnabledTypes().Empty());

  registry()->ConnectType(
      syncer::THEMES, MakeActivationContext(MakeInitialDataTypeState(THEMES),
                                            std::move(themes_sync_processor)));
  EXPECT_TRUE(registry()->GetEnabledTypes().Equals(
      ModelTypeSet(syncer::THEMES)));

  registry()->ConnectType(
      syncer::SESSIONS,
      MakeActivationContext(MakeInitialDataTypeState(SESSIONS),
                            std::move(sessions_sync_processor)));
  EXPECT_TRUE(registry()->GetEnabledTypes().Equals(
      ModelTypeSet(syncer::THEMES, syncer::SESSIONS)));

  registry()->DisconnectType(syncer::THEMES);
  EXPECT_TRUE(registry()->GetEnabledTypes().Equals(
      ModelTypeSet(syncer::SESSIONS)));

  // Allow ModelTypeRegistry destruction to delete the
  // Sessions' ModelTypeSyncWorker.
}

TEST_F(ModelTypeRegistryTest, NonBlockingTypesWithDirectoryTypes) {
  std::unique_ptr<syncer_v2::SharedModelTypeProcessor> themes_sync_processor =
      MakeModelTypeProcessor(syncer::THEMES);
  std::unique_ptr<syncer_v2::SharedModelTypeProcessor> sessions_sync_processor =
      MakeModelTypeProcessor(syncer::SESSIONS);

  ModelSafeRoutingInfo routing_info1;
  routing_info1.insert(std::make_pair(NIGORI, GROUP_PASSIVE));
  routing_info1.insert(std::make_pair(BOOKMARKS, GROUP_UI));
  routing_info1.insert(std::make_pair(AUTOFILL, GROUP_DB));
  routing_info1.insert(std::make_pair(THEMES, GROUP_NON_BLOCKING));
  routing_info1.insert(std::make_pair(SESSIONS, GROUP_NON_BLOCKING));

  ModelTypeSet directory_types(NIGORI, BOOKMARKS, AUTOFILL);

  ModelTypeSet current_types;
  EXPECT_TRUE(registry()->GetEnabledTypes().Empty());

  // Add the themes non-blocking type.
  registry()->ConnectType(
      syncer::THEMES, MakeActivationContext(MakeInitialDataTypeState(THEMES),
                                            std::move(themes_sync_processor)));
  current_types.Put(syncer::THEMES);
  EXPECT_TRUE(registry()->GetEnabledTypes().Equals(current_types));

  // Add some directory types.
  registry()->SetEnabledDirectoryTypes(routing_info1);
  current_types.PutAll(directory_types);
  EXPECT_TRUE(registry()->GetEnabledTypes().Equals(current_types));

  // Add sessions non-blocking type.
  registry()->ConnectType(
      syncer::SESSIONS,
      MakeActivationContext(MakeInitialDataTypeState(SESSIONS),
                            std::move(sessions_sync_processor)));
  current_types.Put(syncer::SESSIONS);
  EXPECT_TRUE(registry()->GetEnabledTypes().Equals(current_types));

  // Remove themes non-blocking type.
  registry()->DisconnectType(syncer::THEMES);
  current_types.Remove(syncer::THEMES);
  EXPECT_TRUE(registry()->GetEnabledTypes().Equals(current_types));

  // Clear all directory types.
  ModelSafeRoutingInfo routing_info2;
  registry()->SetEnabledDirectoryTypes(routing_info2);
  current_types.RemoveAll(directory_types);
  EXPECT_TRUE(registry()->GetEnabledTypes().Equals(current_types));
}

}  // namespace syncer
