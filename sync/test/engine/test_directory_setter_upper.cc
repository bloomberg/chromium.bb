// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/engine/test_directory_setter_upper.h"

#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/string_util.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/in_memory_directory_backing_store.h"
#include "sync/syncable/read_transaction.h"
#include "sync/test/null_transaction_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

using syncable::NullTransactionObserver;

TestDirectorySetterUpper::TestDirectorySetterUpper() : name_("Test") {}

TestDirectorySetterUpper::~TestDirectorySetterUpper() {}

void TestDirectorySetterUpper::SetUp() {
  directory_.reset(new syncable::Directory(&encryptor_, &handler_, NULL,
      new syncable::InMemoryDirectoryBackingStore(name_)));
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  ASSERT_EQ(syncable::OPENED, directory_->Open(
          name_, &delegate_, NullTransactionObserver()));
}

void TestDirectorySetterUpper::TearDown() {
  if (!directory()->good())
    return;

  RunInvariantCheck();
  directory()->SaveChanges();
  RunInvariantCheck();
  directory()->SaveChanges();

  directory_.reset();

  ASSERT_TRUE(temp_dir_.Delete());
}

void TestDirectorySetterUpper::RunInvariantCheck() {
  // Check invariants for all items.
  syncable::ReadTransaction trans(FROM_HERE, directory());

  // The TestUnrecoverableErrorHandler that this directory was constructed with
  // will handle error reporting, so we can safely ignore the return value.
  directory()->FullyCheckTreeInvariants(&trans);
}

}  // namespace syncer
