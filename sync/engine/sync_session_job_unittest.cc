// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/sync_session_job.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/time.h"
#include "sync/internal_api/public/base/model_type_invalidation_map.h"
#include "sync/sessions/sync_session.h"
#include "sync/sessions/sync_session_context.h"
#include "sync/sessions/test_util.h"
#include "sync/test/engine/fake_model_worker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeTicks;

namespace syncer {

using sessions::SyncSession;

class MockDelegate : public SyncSession::Delegate {
 public:
   MockDelegate() {}
   ~MockDelegate() {}

  MOCK_METHOD0(IsSyncingCurrentlySilenced, bool());
  MOCK_METHOD1(OnReceivedShortPollIntervalUpdate, void(const base::TimeDelta&));
  MOCK_METHOD1(OnReceivedLongPollIntervalUpdate ,void(const base::TimeDelta&));
  MOCK_METHOD1(OnReceivedSessionsCommitDelay, void(const base::TimeDelta&));
  MOCK_METHOD1(OnSyncProtocolError, void(const sessions::SyncSessionSnapshot&));
  MOCK_METHOD0(OnShouldStopSyncingPermanently, void());
  MOCK_METHOD1(OnSilencedUntil, void(const base::TimeTicks&));
};

class SyncSessionJobTest : public testing::Test {
 public:
  SyncSessionJobTest() : config_params_callback_invoked_(false) {}
  virtual void SetUp() {
    routes_.clear();
    workers_.clear();
    config_params_callback_invoked_ = false;
    routes_[BOOKMARKS] = GROUP_PASSIVE;
    scoped_refptr<ModelSafeWorker> passive_worker(
        new FakeModelWorker(GROUP_PASSIVE));
    workers_.push_back(passive_worker);
    std::vector<ModelSafeWorker*> workers;
    GetWorkers(&workers);
    context_.reset(new sessions::SyncSessionContext(
        NULL,  // |connection_manager|
        NULL,  // |directory|
        workers,
        NULL,  // |extensions_activity_monitor|
        NULL,  // |throttled_data_type_tracker|
        std::vector<SyncEngineEventListener*>(),
        NULL,  // |debug_info_getter|
        NULL,  // |traffic_recorder|
        true,  // |enable keystore encryption|
        "fake_invalidator_client_id"));
    context_->set_routing_info(routes_);
  }

  scoped_ptr<SyncSession> NewLocalSession() {
    sessions::SyncSourceInfo info(
        sync_pb::GetUpdatesCallerInfo::LOCAL, ModelTypeInvalidationMap());
    return scoped_ptr<SyncSession>(
        new SyncSession(context_.get(), &delegate_, info));
  }

  void GetWorkers(std::vector<ModelSafeWorker*>* out) const {
    out->clear();
    for (std::vector<scoped_refptr<ModelSafeWorker> >::const_iterator it =
             workers_.begin(); it != workers_.end(); ++it) {
      out->push_back(it->get());
    }
  }

  void ConfigurationParamsCallback() {
    config_params_callback_invoked_ = true;
  }

  bool config_params_callback_invoked() const {
    return config_params_callback_invoked_;
  }

  sessions::SyncSessionContext* context() { return context_.get(); }
  SyncSession::Delegate* delegate() { return &delegate_; }
  const ModelSafeRoutingInfo& routes() { return routes_; }

  // Checks that the two jobs are "clones" as defined by SyncSessionJob,
  // minus location and SyncSession checking, for reuse in different
  // scenarios.
  void ExpectClonesBase(SyncSessionJob* job, SyncSessionJob* clone) {
    EXPECT_EQ(job->purpose(), clone->purpose());
    EXPECT_EQ(job->scheduled_start(), clone->scheduled_start());
    EXPECT_EQ(job->start_step(), clone->start_step());
    EXPECT_EQ(job->end_step(), clone->end_step());
    EXPECT_FALSE(clone->is_canary());
  }

 private:
  scoped_ptr<sessions::SyncSessionContext> context_;
  std::vector<scoped_refptr<ModelSafeWorker> > workers_;
  MockDelegate delegate_;
  ModelSafeRoutingInfo routes_;
  bool config_params_callback_invoked_;
};

TEST_F(SyncSessionJobTest, Clone) {
  SyncSessionJob job1(SyncSessionJob::NUDGE, TimeTicks::Now(),
      NewLocalSession().Pass(), ConfigurationParams(), FROM_HERE);

  sessions::test_util::SimulateSuccess(job1.mutable_session(),
                                       job1.start_step(),
                                       job1.end_step());
  job1.Finish(false);
  ModelSafeRoutingInfo new_routes;
  new_routes[AUTOFILL] = GROUP_PASSIVE;
  context()->set_routing_info(new_routes);
  const tracked_objects::Location from_here1(FROM_HERE);
  scoped_ptr<SyncSessionJob> clone1 = job1.Clone();
  scoped_ptr<SyncSessionJob> clone1_loc = job1.CloneFromLocation(from_here1);

  ExpectClonesBase(&job1, clone1.get());
  ExpectClonesBase(&job1, clone1_loc.get());
  EXPECT_NE(job1.session(), clone1->session());
  EXPECT_EQ(job1.from_location().ToString(),
            clone1->from_location().ToString());
  EXPECT_NE(job1.session(), clone1_loc->session());
  EXPECT_EQ(from_here1.ToString(), clone1_loc->from_location().ToString());

  context()->set_routing_info(routes());
  clone1->GrantCanaryPrivilege();
  sessions::test_util::SimulateSuccess(clone1->mutable_session(),
                                       clone1->start_step(),
                                       clone1->end_step());
  clone1->Finish(false);
  const tracked_objects::Location from_here2(FROM_HERE);
  scoped_ptr<SyncSessionJob> clone2 = clone1->Clone();
  scoped_ptr<SyncSessionJob> clone2_loc(clone1->CloneFromLocation(from_here2));

  ExpectClonesBase(clone1.get(), clone2.get());
  ExpectClonesBase(clone1.get(), clone2_loc.get());
  EXPECT_NE(clone1->session(), clone2->session());
  EXPECT_EQ(clone1->from_location().ToString(),
            clone2->from_location().ToString());
  EXPECT_NE(clone1->session(), clone2->session());
  EXPECT_EQ(from_here2.ToString(), clone2_loc->from_location().ToString());

  clone1.reset();
  clone1_loc.reset();
  ExpectClonesBase(&job1, clone2.get());
  EXPECT_NE(job1.session(), clone2->session());
  EXPECT_EQ(job1.from_location().ToString(),
            clone2->from_location().ToString());
}

TEST_F(SyncSessionJobTest, CloneAfterEarlyExit) {
  SyncSessionJob job1(SyncSessionJob::NUDGE, TimeTicks::Now(),
      NewLocalSession().Pass(), ConfigurationParams(), FROM_HERE);
  job1.Finish(true);
  scoped_ptr<SyncSessionJob> job2 = job1.Clone();
  scoped_ptr<SyncSessionJob> job2_loc = job1.CloneFromLocation(FROM_HERE);
  ExpectClonesBase(&job1, job2.get());
  ExpectClonesBase(&job1, job2_loc.get());
}

TEST_F(SyncSessionJobTest, CloneAndAbandon) {
  scoped_ptr<SyncSession> session = NewLocalSession();
  SyncSession* session_ptr = session.get();

  SyncSessionJob job1(SyncSessionJob::NUDGE, TimeTicks::Now(),
      session.Pass(), ConfigurationParams(), FROM_HERE);
  ModelSafeRoutingInfo new_routes;
  new_routes[AUTOFILL] = GROUP_PASSIVE;
  context()->set_routing_info(new_routes);

  scoped_ptr<SyncSessionJob> clone1 = job1.CloneAndAbandon();
  ExpectClonesBase(&job1, clone1.get());
  EXPECT_FALSE(job1.session());
  EXPECT_EQ(session_ptr, clone1->session());
}

// Tests interaction between Finish and sync cycle success / failure.
TEST_F(SyncSessionJobTest, Finish) {
  SyncSessionJob job1(SyncSessionJob::NUDGE, TimeTicks::Now(),
      NewLocalSession().Pass(), ConfigurationParams(), FROM_HERE);

  sessions::test_util::SimulateSuccess(job1.mutable_session(),
                                       job1.start_step(),
                                       job1.end_step());
  EXPECT_TRUE(job1.Finish(false /* early_exit */));

  scoped_ptr<SyncSessionJob> job2 = job1.Clone();
  sessions::test_util::SimulateConnectionFailure(job2->mutable_session(),
                                                 job2->start_step(),
                                                 job2->end_step());
  EXPECT_FALSE(job2->Finish(false));

  scoped_ptr<SyncSessionJob> job3 = job2->Clone();
  EXPECT_FALSE(job3->Finish(true));
}

TEST_F(SyncSessionJobTest, FinishCallsReadyTask) {
  ConfigurationParams params;
  params.ready_task = base::Bind(
      &SyncSessionJobTest::ConfigurationParamsCallback,
      base::Unretained(this));

  sessions::SyncSourceInfo info(
      sync_pb::GetUpdatesCallerInfo::RECONFIGURATION,
      ModelTypeInvalidationMap());
  scoped_ptr<SyncSession> session(
      new SyncSession(context(), delegate(), info));

  SyncSessionJob job1(SyncSessionJob::CONFIGURATION, TimeTicks::Now(),
      session.Pass(), params, FROM_HERE);
  sessions::test_util::SimulateSuccess(job1.mutable_session(),
                                       job1.start_step(),
                                       job1.end_step());
  job1.Finish(false);
  EXPECT_TRUE(config_params_callback_invoked());
}

}  // namespace syncer
