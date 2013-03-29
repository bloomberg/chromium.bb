// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/sync_session.h"

#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "sync/engine/syncer_types.h"
#include "sync/engine/throttled_data_type_tracker.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/base/model_type_invalidation_map_test_util.h"
#include "sync/sessions/status_controller.h"
#include "sync/syncable/syncable_id.h"
#include "sync/syncable/syncable_write_transaction.h"
#include "sync/test/engine/fake_model_worker.h"
#include "sync/test/engine/test_directory_setter_upper.h"
#include "sync/test/fake_extensions_activity_monitor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

using syncable::WriteTransaction;

namespace sessions {
namespace {

class SyncSessionTest : public testing::Test,
                        public SyncSession::Delegate {
 public:
  SyncSessionTest() : controller_invocations_allowed_(false) {}

  SyncSession* MakeSession() {
    return new SyncSession(context_.get(), this, SyncSourceInfo());
  }

  virtual void SetUp() {
    routes_.clear();
    routes_[BOOKMARKS] = GROUP_UI;
    routes_[AUTOFILL] = GROUP_DB;
    scoped_refptr<ModelSafeWorker> passive_worker(
        new FakeModelWorker(GROUP_PASSIVE));
    scoped_refptr<ModelSafeWorker> ui_worker(
        new FakeModelWorker(GROUP_UI));
    scoped_refptr<ModelSafeWorker> db_worker(
        new FakeModelWorker(GROUP_DB));
    workers_.clear();
    workers_.push_back(passive_worker);
    workers_.push_back(ui_worker);
    workers_.push_back(db_worker);

    std::vector<ModelSafeWorker*> workers;
    GetWorkers(&workers);

    context_.reset(
        new SyncSessionContext(
            NULL,
            NULL,
            workers,
            &extensions_activity_monitor_,
            throttled_data_type_tracker_.get(),
            std::vector<SyncEngineEventListener*>(),
            NULL,
            NULL,
            true,  // enable keystore encryption
            "fake_invalidator_client_id"));
    context_->set_routing_info(routes_);

    session_.reset(MakeSession());
    throttled_data_type_tracker_.reset(new ThrottledDataTypeTracker(NULL));
  }
  virtual void TearDown() {
    session_.reset();
    context_.reset();
  }

  virtual void OnSilencedUntil(const base::TimeTicks& silenced_until) OVERRIDE {
    FailControllerInvocationIfDisabled("OnSilencedUntil");
  }
  virtual bool IsSyncingCurrentlySilenced() OVERRIDE {
    FailControllerInvocationIfDisabled("IsSyncingCurrentlySilenced");
    return false;
  }
  virtual void OnReceivedLongPollIntervalUpdate(
      const base::TimeDelta& new_interval) OVERRIDE {
    FailControllerInvocationIfDisabled("OnReceivedLongPollIntervalUpdate");
  }
  virtual void OnReceivedShortPollIntervalUpdate(
      const base::TimeDelta& new_interval) OVERRIDE {
    FailControllerInvocationIfDisabled("OnReceivedShortPollIntervalUpdate");
  }
  virtual void OnReceivedSessionsCommitDelay(
      const base::TimeDelta& new_delay) OVERRIDE {
    FailControllerInvocationIfDisabled("OnReceivedSessionsCommitDelay");
  }
  virtual void OnShouldStopSyncingPermanently() OVERRIDE {
    FailControllerInvocationIfDisabled("OnShouldStopSyncingPermanently");
  }
  virtual void OnSyncProtocolError(
      const sessions::SyncSessionSnapshot& snapshot) OVERRIDE {
    FailControllerInvocationIfDisabled("SyncProtocolError");
  }

  void GetWorkers(std::vector<ModelSafeWorker*>* out) const {
    out->clear();
    for (std::vector<scoped_refptr<ModelSafeWorker> >::const_iterator it =
             workers_.begin(); it != workers_.end(); ++it) {
      out->push_back(it->get());
    }
  }
  void GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) const {
    *out = routes_;
  }

  StatusController* status() { return session_->mutable_status_controller(); }
 protected:
  void FailControllerInvocationIfDisabled(const std::string& msg) {
    if (!controller_invocations_allowed_)
      FAIL() << msg;
  }

  ModelTypeSet ParamsMeaningAllEnabledTypes() {
    ModelTypeSet request_params(BOOKMARKS, AUTOFILL);
    return request_params;
  }

  ModelTypeSet ParamsMeaningJustOneEnabledType() {
    return ModelTypeSet(AUTOFILL);
  }

  MessageLoop message_loop_;
  bool controller_invocations_allowed_;
  scoped_ptr<SyncSession> session_;
  scoped_ptr<SyncSessionContext> context_;
  std::vector<scoped_refptr<ModelSafeWorker> > workers_;
  ModelSafeRoutingInfo routes_;
  FakeExtensionsActivityMonitor extensions_activity_monitor_;
  scoped_ptr<ThrottledDataTypeTracker> throttled_data_type_tracker_;
};

TEST_F(SyncSessionTest, MoreToDownloadIfDownloadFailed) {
  status()->set_updates_request_types(ParamsMeaningAllEnabledTypes());

  status()->set_last_download_updates_result(NETWORK_IO_ERROR);

  // When DownloadUpdatesCommand fails, these should be false.
  EXPECT_FALSE(status()->ServerSaysNothingMoreToDownload());
  EXPECT_FALSE(status()->download_updates_succeeded());
}

TEST_F(SyncSessionTest, MoreToDownloadIfGotChangesRemaining) {
  status()->set_updates_request_types(ParamsMeaningAllEnabledTypes());

  // When the server returns changes_remaining, that means there's
  // more to download.
  status()->set_last_download_updates_result(SYNCER_OK);
  status()->mutable_updates_response()->mutable_get_updates()
     ->set_changes_remaining(1000L);
  EXPECT_FALSE(status()->ServerSaysNothingMoreToDownload());
  EXPECT_TRUE(status()->download_updates_succeeded());
}

TEST_F(SyncSessionTest, MoreToDownloadIfGotNoChangesRemaining) {
  status()->set_updates_request_types(ParamsMeaningAllEnabledTypes());

  status()->set_last_download_updates_result(SYNCER_OK);
  status()->mutable_updates_response()->mutable_get_updates()
      ->set_changes_remaining(0);
  EXPECT_TRUE(status()->ServerSaysNothingMoreToDownload());
  EXPECT_TRUE(status()->download_updates_succeeded());
}

TEST_F(SyncSessionTest, MakeTypeInvalidationMapFromBitSet) {
  ModelTypeSet types;
  std::string payload = "test";
  ModelTypeInvalidationMap invalidation_map =
      ModelTypeSetToInvalidationMap(types, payload);
  EXPECT_TRUE(invalidation_map.empty());

  types.Put(BOOKMARKS);
  types.Put(PASSWORDS);
  types.Put(AUTOFILL);
  payload = "test2";
  invalidation_map = ModelTypeSetToInvalidationMap(types, payload);

  ASSERT_EQ(3U, invalidation_map.size());
  EXPECT_EQ(invalidation_map[BOOKMARKS].payload, payload);
  EXPECT_EQ(invalidation_map[PASSWORDS].payload, payload);
  EXPECT_EQ(invalidation_map[AUTOFILL].payload, payload);
}

}  // namespace
}  // namespace sessions
}  // namespace syncer
