// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/sessions/sync_session.h"

#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "sync/engine/syncer_types.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/sessions/status_controller.h"
#include "sync/syncable/syncable_id.h"
#include "sync/syncable/syncable_write_transaction.h"
#include "sync/test/engine/fake_model_worker.h"
#include "sync/test/engine/test_directory_setter_upper.h"
#include "sync/util/extensions_activity.h"
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
    return SyncSession::Build(context_.get(), this);
  }

  virtual void SetUp() {
    extensions_activity_ = new ExtensionsActivity();

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
            extensions_activity_.get(),
            std::vector<SyncEngineEventListener*>(),
            NULL,
            NULL,
            true,  // enable keystore encryption
            false,  // force enable pre-commit GU avoidance experiment
            "fake_invalidator_client_id"));
    context_->set_routing_info(routes_);

    session_.reset(MakeSession());
  }
  virtual void TearDown() {
    session_.reset();
    context_.reset();
  }

  virtual void OnThrottled(const base::TimeDelta& throttle_duration) OVERRIDE {
    FailControllerInvocationIfDisabled("OnThrottled");
  }
  virtual void OnTypesThrottled(
      ModelTypeSet types,
      const base::TimeDelta& throttle_duration) OVERRIDE {
    FailControllerInvocationIfDisabled("OnTypesThrottled");
  }
  virtual bool IsCurrentlyThrottled() OVERRIDE {
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
  virtual void OnReceivedClientInvalidationHintBufferSize(
      int size) OVERRIDE {
    FailControllerInvocationIfDisabled(
        "OnReceivedClientInvalidationHintBufferSize");
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

  base::MessageLoop message_loop_;
  bool controller_invocations_allowed_;
  scoped_ptr<SyncSession> session_;
  scoped_ptr<SyncSessionContext> context_;
  std::vector<scoped_refptr<ModelSafeWorker> > workers_;
  ModelSafeRoutingInfo routes_;
  scoped_refptr<ExtensionsActivity> extensions_activity_;
};

}  // namespace
}  // namespace sessions
}  // namespace syncer
