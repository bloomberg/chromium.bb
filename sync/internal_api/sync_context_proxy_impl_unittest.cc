// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/sync_context_proxy_impl.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "sync/internal_api/public/activation_context.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/shared_model_type_processor.h"
#include "sync/internal_api/public/sync_context.h"
#include "sync/internal_api/public/test/fake_model_type_service.h"
#include "sync/sessions/model_type_registry.h"
#include "sync/test/engine/mock_nudge_handler.h"
#include "sync/test/engine/test_directory_setter_upper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer_v2 {

class SyncContextProxyImplTest : public ::testing::Test, FakeModelTypeService {
 public:
  SyncContextProxyImplTest()
      : sync_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        type_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

  void SetUp() override {
    dir_maker_.SetUp();
    registry_.reset(new syncer::ModelTypeRegistry(
        workers_, dir_maker_.directory(), &nudge_handler_));
    context_proxy_.reset(
        new SyncContextProxyImpl(sync_task_runner_, registry_->AsWeakPtr()));
  }

  void TearDown() override {
    context_proxy_.reset();
    registry_.reset();
    dir_maker_.TearDown();
  }

  // The sync thread could be shut down at any time without warning.  This
  // function simulates such an event.
  void DisableSync() { registry_.reset(); }

  void Start(SharedModelTypeProcessor* processor) {
    processor->Start(base::Bind(&SyncContextProxyImplTest::StartDone,
                                base::Unretained(this)));
  }

  void StartDone(syncer::SyncError error,
                 scoped_ptr<ActivationContext> context) {
    context_proxy_->ConnectTypeToSync(syncer::THEMES, std::move(context));
  }

  scoped_ptr<SharedModelTypeProcessor> CreateModelTypeProcessor() {
    scoped_ptr<SharedModelTypeProcessor> processor =
        make_scoped_ptr(new SharedModelTypeProcessor(syncer::THEMES, this));
    processor->OnMetadataLoaded(make_scoped_ptr(new MetadataBatch()));
    return processor;
  }

 private:
  base::MessageLoop loop_;
  scoped_refptr<base::SingleThreadTaskRunner> sync_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> type_task_runner_;

  std::vector<scoped_refptr<syncer::ModelSafeWorker>> workers_;
  syncer::TestDirectorySetterUpper dir_maker_;
  syncer::MockNudgeHandler nudge_handler_;
  scoped_ptr<syncer::ModelTypeRegistry> registry_;

  scoped_ptr<SyncContextProxyImpl> context_proxy_;
};

// Try to connect a type to a SyncContext that has already shut down.
TEST_F(SyncContextProxyImplTest, FailToConnect1) {
  scoped_ptr<SharedModelTypeProcessor> processor = CreateModelTypeProcessor();
  DisableSync();
  Start(processor.get());

  base::RunLoop run_loop_;
  run_loop_.RunUntilIdle();
  EXPECT_FALSE(processor->IsConnected());
}

// Try to connect a type to a SyncContext as it shuts down.
TEST_F(SyncContextProxyImplTest, FailToConnect2) {
  scoped_ptr<SharedModelTypeProcessor> processor = CreateModelTypeProcessor();
  Start(processor.get());
  DisableSync();

  base::RunLoop run_loop_;
  run_loop_.RunUntilIdle();
  EXPECT_FALSE(processor->IsConnected());
}

// Tests the case where the type's sync proxy shuts down first.
TEST_F(SyncContextProxyImplTest, TypeDisconnectsFirst) {
  scoped_ptr<SharedModelTypeProcessor> processor = CreateModelTypeProcessor();
  Start(processor.get());

  base::RunLoop run_loop_;
  run_loop_.RunUntilIdle();

  EXPECT_TRUE(processor->IsConnected());
  processor.reset();
}

// Tests the case where the sync thread shuts down first.
TEST_F(SyncContextProxyImplTest, SyncDisconnectsFirst) {
  scoped_ptr<SharedModelTypeProcessor> processor = CreateModelTypeProcessor();
  Start(processor.get());

  base::RunLoop run_loop_;
  run_loop_.RunUntilIdle();

  EXPECT_TRUE(processor->IsConnected());
  DisableSync();
}

}  // namespace syncer_v2
