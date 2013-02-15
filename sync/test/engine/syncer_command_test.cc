// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/engine/syncer_command_test.h"

namespace syncer {

const unsigned int kMaxMessages = 10;
const unsigned int kMaxMessageSize = 5 * 1024;

SyncerCommandTestBase::SyncerCommandTestBase()
    : traffic_recorder_(kMaxMessages, kMaxMessageSize) {
}

SyncerCommandTestBase::~SyncerCommandTestBase() {
}

void SyncerCommandTestBase::SetUp() {
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

MockDebugInfoGetter::MockDebugInfoGetter() {
}

MockDebugInfoGetter::~MockDebugInfoGetter() {
}

MockDirectorySyncerCommandTest::MockDirectorySyncerCommandTest()
    :mock_directory_(&handler_) {
}

MockDirectorySyncerCommandTest::~MockDirectorySyncerCommandTest() {
}

void MockDirectorySyncerCommandTest::SetUp() {
  // The session always expects there to be a passive worker.
  workers()->push_back(
      make_scoped_refptr(new FakeModelWorker(GROUP_PASSIVE)));
  ResetContext();
}

syncable::Directory* MockDirectorySyncerCommandTest::directory() {
  return &mock_directory_;
}

}  // namespace syncer
