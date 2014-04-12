// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/non_blocking_type_processor.h"
#include "sync/internal_api/sync_core.h"
#include "sync/internal_api/sync_core_proxy_impl.h"
#include "sync/sessions/model_type_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class SyncCoreProxyImplTest : public ::testing::Test {
 public:
  SyncCoreProxyImplTest()
      : sync_task_runner_(base::MessageLoopProxy::current()),
        type_task_runner_(base::MessageLoopProxy::current()),
        core_(new SyncCore(&registry_)),
        core_proxy_(
            sync_task_runner_,
            core_->AsWeakPtr()) {}

  // The sync thread could be shut down at any time without warning.  This
  // function simulates such an event.
  void DisableSync() {
    core_.reset();
  }

  SyncCoreProxy* GetProxy() {
    return &core_proxy_;
  }

 private:
  base::MessageLoop loop_;
  scoped_refptr<base::SequencedTaskRunner> sync_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> type_task_runner_;
  ModelTypeRegistry registry_;
  scoped_ptr<SyncCore> core_;
  SyncCoreProxyImpl core_proxy_;
};

// Try to connect a type to a SyncCore that has already shut down.
TEST_F(SyncCoreProxyImplTest, FailToConnect1) {
  NonBlockingTypeProcessor themes_processor(syncer::THEMES);
  DisableSync();
  themes_processor.Enable(GetProxy());

  base::RunLoop run_loop_;
  run_loop_.RunUntilIdle();
  EXPECT_FALSE(themes_processor.IsEnabled());
}

// Try to connect a type to a SyncCore as it shuts down.
TEST_F(SyncCoreProxyImplTest, FailToConnect2) {
  NonBlockingTypeProcessor themes_processor(syncer::THEMES);
  themes_processor.Enable(GetProxy());
  DisableSync();

  base::RunLoop run_loop_;
  run_loop_.RunUntilIdle();
  EXPECT_FALSE(themes_processor.IsEnabled());
}

// Tests the case where the type's processor shuts down first.
TEST_F(SyncCoreProxyImplTest, TypeDisconnectsFirst) {
  scoped_ptr<NonBlockingTypeProcessor> themes_processor
      (new NonBlockingTypeProcessor(syncer::THEMES));
  themes_processor->Enable(GetProxy());

  base::RunLoop run_loop_;
  run_loop_.RunUntilIdle();

  EXPECT_TRUE(themes_processor->IsEnabled());
  themes_processor.reset();
}

// Tests the case where the sync thread shuts down first.
TEST_F(SyncCoreProxyImplTest, SyncDisconnectsFirst) {
  scoped_ptr<NonBlockingTypeProcessor> themes_processor
      (new NonBlockingTypeProcessor(syncer::THEMES));
  themes_processor->Enable(GetProxy());

  base::RunLoop run_loop_;
  run_loop_.RunUntilIdle();

  EXPECT_TRUE(themes_processor->IsEnabled());
  DisableSync();
}

}  // namespace syncer
