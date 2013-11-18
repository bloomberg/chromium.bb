// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/engine/syncer_command_test.h"

namespace syncer {

const unsigned int kMaxMessages = 10;
const unsigned int kMaxMessageSize = 5 * 1024;

void SyncerCommandTestBase::OnThrottled(
    const base::TimeDelta& throttle_duration) {
  FAIL() << "Should not get silenced.";
}

void SyncerCommandTestBase::OnTypesThrottled(
    ModelTypeSet types,
    const base::TimeDelta& throttle_duration) {
  FAIL() << "Should not get silenced.";
}

bool SyncerCommandTestBase::IsCurrentlyThrottled() {
  return false;
}

void SyncerCommandTestBase::OnReceivedLongPollIntervalUpdate(
    const base::TimeDelta& new_interval) {
  FAIL() << "Should not get poll interval update.";
}

void SyncerCommandTestBase::OnReceivedShortPollIntervalUpdate(
    const base::TimeDelta& new_interval) {
  FAIL() << "Should not get poll interval update.";
}

void SyncerCommandTestBase::OnReceivedSessionsCommitDelay(
    const base::TimeDelta& new_delay) {
  FAIL() << "Should not get sessions commit delay.";
}

void SyncerCommandTestBase::OnReceivedClientInvalidationHintBufferSize(
    int size) {
  FAIL() << "Should not get hint buffer size.";
}

void SyncerCommandTestBase::OnSyncProtocolError(
    const sessions::SyncSessionSnapshot& session) {
  return;
}
SyncerCommandTestBase::SyncerCommandTestBase()
    : traffic_recorder_(kMaxMessages, kMaxMessageSize) {
}

SyncerCommandTestBase::~SyncerCommandTestBase() {
}

void SyncerCommandTestBase::SetUp() {
  extensions_activity_ = new ExtensionsActivity();

  // The session always expects there to be a passive worker.
  workers()->push_back(
      make_scoped_refptr(new FakeModelWorker(GROUP_PASSIVE)));
  ResetContext();
}

void SyncerCommandTestBase::TearDown() {
}

syncable::Directory* SyncerCommandTest::directory() {
  return dir_maker_.directory();
}

void SyncerCommandTest::SetUp() {
  dir_maker_.SetUp();
  SyncerCommandTestBase::SetUp();
}

void SyncerCommandTest::TearDown() {
  dir_maker_.TearDown();
}

}  // namespace syncer
