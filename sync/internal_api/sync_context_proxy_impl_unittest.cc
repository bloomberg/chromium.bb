// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "sync/engine/model_type_sync_proxy_impl.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/sync_context.h"
#include "sync/internal_api/sync_context_proxy_impl.h"
#include "sync/sessions/model_type_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class SyncContextProxyImplTest : public ::testing::Test {
 public:
  SyncContextProxyImplTest()
      : sync_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        type_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        registry_(new ModelTypeRegistry()),
        context_proxy_(sync_task_runner_, registry_->AsWeakPtr()) {}

  // The sync thread could be shut down at any time without warning.  This
  // function simulates such an event.
  void DisableSync() { registry_.reset(); }

  scoped_ptr<SyncContextProxy> GetProxy() { return context_proxy_.Clone(); }

 private:
  base::MessageLoop loop_;
  scoped_refptr<base::SequencedTaskRunner> sync_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> type_task_runner_;
  scoped_ptr<ModelTypeRegistry> registry_;
  SyncContextProxyImpl context_proxy_;
};

// Try to connect a type to a SyncContext that has already shut down.
TEST_F(SyncContextProxyImplTest, FailToConnect1) {
  ModelTypeSyncProxyImpl themes_sync_proxy(syncer::THEMES);
  DisableSync();
  themes_sync_proxy.Enable(GetProxy());

  base::RunLoop run_loop_;
  run_loop_.RunUntilIdle();
  EXPECT_FALSE(themes_sync_proxy.IsConnected());
}

// Try to connect a type to a SyncContext as it shuts down.
TEST_F(SyncContextProxyImplTest, FailToConnect2) {
  ModelTypeSyncProxyImpl themes_sync_proxy(syncer::THEMES);
  themes_sync_proxy.Enable(GetProxy());
  DisableSync();

  base::RunLoop run_loop_;
  run_loop_.RunUntilIdle();
  EXPECT_FALSE(themes_sync_proxy.IsConnected());
}

// Tests the case where the type's sync proxy shuts down first.
TEST_F(SyncContextProxyImplTest, TypeDisconnectsFirst) {
  scoped_ptr<ModelTypeSyncProxyImpl> themes_sync_proxy(
      new ModelTypeSyncProxyImpl(syncer::THEMES));
  themes_sync_proxy->Enable(GetProxy());

  base::RunLoop run_loop_;
  run_loop_.RunUntilIdle();

  EXPECT_TRUE(themes_sync_proxy->IsConnected());
  themes_sync_proxy.reset();
}

// Tests the case where the sync thread shuts down first.
TEST_F(SyncContextProxyImplTest, SyncDisconnectsFirst) {
  scoped_ptr<ModelTypeSyncProxyImpl> themes_sync_proxy(
      new ModelTypeSyncProxyImpl(syncer::THEMES));
  themes_sync_proxy->Enable(GetProxy());

  base::RunLoop run_loop_;
  run_loop_.RunUntilIdle();

  EXPECT_TRUE(themes_sync_proxy->IsConnected());
  DisableSync();
}

}  // namespace syncer
