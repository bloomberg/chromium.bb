// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/engine/test_directory_setter_upper.h"

#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/string_util.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/read_transaction.h"
#include "sync/test/null_transaction_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncable::NullTransactionObserver;
using syncable::ReadTransaction;

namespace browser_sync {

TestDirectorySetterUpper::TestDirectorySetterUpper() : name_("Test") {}

TestDirectorySetterUpper::~TestDirectorySetterUpper() {}

void TestDirectorySetterUpper::SetUp() {
  directory_.reset(new syncable::Directory(&encryptor_, &handler_, NULL));
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  ASSERT_EQ(syncable::OPENED, directory_->OpenInMemoryForTest(
          name_, &delegate_, NullTransactionObserver()));
}

void TestDirectorySetterUpper::TearDown() {
  if (!directory()->good())
    return;

  {
    RunInvariantCheck();
    directory()->SaveChanges();
    RunInvariantCheck();
    directory()->SaveChanges();
  }
  directory_.reset();

  ASSERT_TRUE(temp_dir_.Delete());
}

void TestDirectorySetterUpper::RunInvariantCheck() {
  {
    // Check invariants for in-memory items.
    ReadTransaction trans(FROM_HERE, directory());
    directory()->CheckTreeInvariants(&trans, false);
  }
  {
    // Check invariants for all items.
    ReadTransaction trans(FROM_HERE, directory());
    directory()->CheckTreeInvariants(&trans, true);
  }
}

}  // namespace browser_sync
