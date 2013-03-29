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

  scoped_ptr<SyncSession> MakeSession() {
    sessions::SyncSourceInfo info(MakeSourceInfo());
    std::vector<sessions::SyncSourceInfo> sources_list;
    return scoped_ptr<SyncSession>(
        new SyncSession(context_.get(), &delegate_, info));
  }

  sessions::SyncSourceInfo MakeSourceInfo() {
    sessions::SyncSourceInfo info(sync_pb::GetUpdatesCallerInfo::LOCAL,
                                  ModelTypeInvalidationMap());
    return info;
  }

  ModelTypeSet ParamsMeaningAllEnabledTypes() {
    ModelTypeSet request_params(BOOKMARKS, AUTOFILL);
    return request_params;
  }

  ModelTypeSet ParamsMeaningJustOneEnabledType() {
    return ModelTypeSet(AUTOFILL);
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

  // Checks that the two jobs are "clones" as defined by SyncSessionJob.
  void ExpectClones(SyncSessionJob* job, SyncSessionJob* clone) {
    EXPECT_EQ(job->purpose(), clone->purpose());
    EXPECT_EQ(job->scheduled_start(), clone->scheduled_start());
    EXPECT_EQ(job->start_step(), clone->start_step());
    EXPECT_EQ(job->end_step(), clone->end_step());
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
      MakeSourceInfo(), ConfigurationParams());

  scoped_ptr<SyncSession> session1 = MakeSession().Pass();
  sessions::test_util::SimulateSuccess(session1.get(),
                                       job1.start_step(),
                                       job1.end_step());
  job1.Finish(false, session1.get());
  ModelSafeRoutingInfo new_routes;
  new_routes[AUTOFILL] = GROUP_PASSIVE;
  context()->set_routing_info(new_routes);
  scoped_ptr<SyncSessionJob> clone1 = job1.Clone();

  ExpectClones(&job1, clone1.get());

  context()->set_routing_info(routes());
  scoped_ptr<SyncSession> session2 = MakeSession().Pass();
  sessions::test_util::SimulateSuccess(session2.get(),
                                       clone1->start_step(),
                                       clone1->end_step());
  clone1->Finish(false, session2.get());
  scoped_ptr<SyncSessionJob> clone2 = clone1->Clone();

  ExpectClones(clone1.get(), clone2.get());

  clone1.reset();
  ExpectClones(&job1, clone2.get());
}

TEST_F(SyncSessionJobTest, CloneAfterEarlyExit) {
  scoped_ptr<SyncSession> session = MakeSession().Pass();
  SyncSessionJob job1(SyncSessionJob::NUDGE, TimeTicks::Now(),
      MakeSourceInfo(), ConfigurationParams());
  job1.Finish(true, session.get());
  scoped_ptr<SyncSessionJob> job2 = job1.Clone();
  ExpectClones(&job1, job2.get());
}

// Tests interaction between Finish and sync cycle success / failure.
TEST_F(SyncSessionJobTest, Finish) {
  SyncSessionJob job1(SyncSessionJob::NUDGE, TimeTicks::Now(),
      MakeSourceInfo(), ConfigurationParams());

  scoped_ptr<SyncSession> session1 = MakeSession().Pass();
  sessions::test_util::SimulateSuccess(session1.get(),
                                       job1.start_step(),
                                       job1.end_step());
  EXPECT_TRUE(job1.Finish(false /* early_exit */, session1.get()));

  scoped_ptr<SyncSessionJob> job2 = job1.Clone();
  scoped_ptr<SyncSession> session2 = MakeSession().Pass();
  sessions::test_util::SimulateConnectionFailure(session2.get(),
                                                 job2->start_step(),
                                                 job2->end_step());
  EXPECT_FALSE(job2->Finish(false, session2.get()));
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

  SyncSessionJob job1(
      SyncSessionJob::CONFIGURATION, TimeTicks::Now(), info, params);
  sessions::test_util::SimulateSuccess(session.get(),
                                       job1.start_step(),
                                       job1.end_step());
  job1.Finish(false, session.get());
  EXPECT_TRUE(config_params_callback_invoked());
}

TEST_F(SyncSessionJobTest, CoalesceSources) {
  ModelTypeInvalidationMap one_type =
      ModelTypeSetToInvalidationMap(
          ParamsMeaningJustOneEnabledType(),
          std::string());
  ModelTypeInvalidationMap all_types =
      ModelTypeSetToInvalidationMap(
          ParamsMeaningAllEnabledTypes(),
          std::string());
  sessions::SyncSourceInfo source_one(
      sync_pb::GetUpdatesCallerInfo::PERIODIC, one_type);
  sessions::SyncSourceInfo source_two(
      sync_pb::GetUpdatesCallerInfo::LOCAL, all_types);

  SyncSessionJob job(SyncSessionJob::NUDGE, TimeTicks::Now(),
                     source_one, ConfigurationParams());

  job.CoalesceSources(source_two);

  EXPECT_EQ(source_two.updates_source, job.source_info().updates_source);
}

}  // namespace syncer
