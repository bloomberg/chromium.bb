// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "sync/api/fake_model_type_change_processor.h"
#include "sync/api/fake_model_type_service.h"
#include "sync/internal_api/public/test/data_type_error_handler_mock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer_v2 {

// A mock MTCP that lets us know when DisableSync is called.
class MockModelTypeChangeProcessor : public FakeModelTypeChangeProcessor {
 public:
  explicit MockModelTypeChangeProcessor(const base::Closure& disabled_callback)
      : disabled_callback_(disabled_callback) {}
  ~MockModelTypeChangeProcessor() override {}

  void DisableSync() override { disabled_callback_.Run(); }

 private:
  base::Closure disabled_callback_;
};

class MockModelTypeService : public FakeModelTypeService {
 public:
  MockModelTypeService()
      : FakeModelTypeService(base::Bind(&MockModelTypeService::CreateProcessor,
                                        base::Unretained(this))) {}
  ~MockModelTypeService() override {}

  void CreateChangeProcessor() { ModelTypeService::CreateChangeProcessor(); }

  MockModelTypeChangeProcessor* change_processor() const {
    return static_cast<MockModelTypeChangeProcessor*>(
        ModelTypeService::change_processor());
  }

  bool on_processor_set_called() const { return on_processor_set_called_; }
  void clear_on_processor_set_called() { on_processor_set_called_ = false; }
  bool processor_disable_sync_called() const {
    return processor_disable_sync_called_;
  }

 private:
  std::unique_ptr<ModelTypeChangeProcessor> CreateProcessor(
      syncer::ModelType type,
      ModelTypeService* service) {
    return base::WrapUnique(new MockModelTypeChangeProcessor(
        base::Bind(&MockModelTypeService::OnProcessorDisableSync,
                   base::Unretained(this))));
  }

  void OnChangeProcessorSet() override { on_processor_set_called_ = true; }

  void OnProcessorDisableSync() { processor_disable_sync_called_ = true; }

  bool on_processor_set_called_ = false;
  bool processor_disable_sync_called_ = false;
};

class ModelTypeServiceTest : public ::testing::Test {
 public:
  ModelTypeServiceTest() {}
  ~ModelTypeServiceTest() override {}

  void OnSyncStarting() {
    service_.OnSyncStarting(
        &error_handler_, base::Bind(&ModelTypeServiceTest::OnProcessorStarted,
                                    base::Unretained(this)));
  }

  bool start_callback_called() const { return start_callback_called_; }
  MockModelTypeService* service() { return &service_; }

 private:
  void OnProcessorStarted(
      syncer::SyncError error,
      std::unique_ptr<ActivationContext> activation_context) {
    start_callback_called_ = true;
  }

  bool start_callback_called_;
  syncer::DataTypeErrorHandlerMock error_handler_;
  MockModelTypeService service_;
};

// CreateChangeProcessor should construct a processor and call
// OnChangeProcessorSet if and only if one doesn't already exist.
TEST_F(ModelTypeServiceTest, CreateChangeProcessor) {
  EXPECT_FALSE(service()->change_processor());
  EXPECT_FALSE(service()->on_processor_set_called());
  service()->CreateChangeProcessor();
  ModelTypeChangeProcessor* processor = service()->change_processor();
  EXPECT_TRUE(processor);
  EXPECT_TRUE(service()->on_processor_set_called());

  // A second call shouldn't make a new processor.
  service()->clear_on_processor_set_called();
  service()->CreateChangeProcessor();
  EXPECT_EQ(processor, service()->change_processor());
  EXPECT_FALSE(service()->on_processor_set_called());
}

// OnSyncStarting should create a processor and call OnSyncStarting on it.
TEST_F(ModelTypeServiceTest, OnSyncStarting) {
  EXPECT_FALSE(service()->change_processor());
  OnSyncStarting();
  EXPECT_TRUE(service()->change_processor());
  // FakeModelTypeProcessor is the one that calls the callback, so if it was
  // called then we know the call on the processor was made.
  EXPECT_TRUE(start_callback_called());
}

// DisableSync should call DisableSync on the processor and then delete it.
TEST_F(ModelTypeServiceTest, DisableSync) {
  service()->CreateChangeProcessor();
  EXPECT_TRUE(service()->change_processor());
  EXPECT_FALSE(service()->processor_disable_sync_called());

  service()->DisableSync();
  EXPECT_FALSE(service()->change_processor());
  EXPECT_TRUE(service()->processor_disable_sync_called());
}

// ResolveConflicts should return USE_REMOTE unless the remote data is deleted.
TEST_F(ModelTypeServiceTest, DefaultConflictResolution) {
  EntityData local_data;
  EntityData remote_data;

  // There is no deleted/deleted case because that's not a conflict.

  local_data.specifics.mutable_preference()->set_value("value");
  EXPECT_FALSE(local_data.is_deleted());
  EXPECT_TRUE(remote_data.is_deleted());
  EXPECT_EQ(ConflictResolution::USE_LOCAL,
            service()->ResolveConflict(local_data, remote_data).type());

  remote_data.specifics.mutable_preference()->set_value("value");
  EXPECT_FALSE(local_data.is_deleted());
  EXPECT_FALSE(remote_data.is_deleted());
  EXPECT_EQ(ConflictResolution::USE_REMOTE,
            service()->ResolveConflict(local_data, remote_data).type());

  local_data.specifics.clear_preference();
  EXPECT_TRUE(local_data.is_deleted());
  EXPECT_FALSE(remote_data.is_deleted());
  EXPECT_EQ(ConflictResolution::USE_REMOTE,
            service()->ResolveConflict(local_data, remote_data).type());
}

}  // namespace syncer_v2
