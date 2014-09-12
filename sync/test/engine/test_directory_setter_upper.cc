// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/engine/test_directory_setter_upper.h"

#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/in_memory_directory_backing_store.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/syncable_read_transaction.h"
#include "sync/syncable/syncable_write_transaction.h"
#include "sync/test/test_transaction_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

TestDirectorySetterUpper::TestDirectorySetterUpper() : name_("Test") {}

TestDirectorySetterUpper::~TestDirectorySetterUpper() {}

void TestDirectorySetterUpper::SetUp() {
  test_transaction_observer_.reset(new syncable::TestTransactionObserver());
  WeakHandle<syncable::TransactionObserver> transaction_observer =
      MakeWeakHandle(test_transaction_observer_->AsWeakPtr());

  directory_.reset(
      new syncable::Directory(
          new syncable::InMemoryDirectoryBackingStore(name_),
          &handler_,
          NULL,
          &encryption_handler_,
          encryption_handler_.cryptographer()));
  ASSERT_EQ(syncable::OPENED, directory_->Open(
          name_, &delegate_, transaction_observer));
}

void TestDirectorySetterUpper::SetUpWith(
    syncer::syncable::DirectoryBackingStore* directory_store) {
  CHECK(directory_store);
  test_transaction_observer_.reset(new syncable::TestTransactionObserver());
  WeakHandle<syncable::TransactionObserver> transaction_observer =
      MakeWeakHandle(test_transaction_observer_->AsWeakPtr());

  directory_.reset(
      new syncable::Directory(directory_store,
                              &handler_,
                              NULL,
                              &encryption_handler_,
                              encryption_handler_.cryptographer()));
    ASSERT_EQ(syncable::OPENED, directory_->Open(
          name_, &delegate_, transaction_observer));
}

void TestDirectorySetterUpper::TearDown() {
  if (!directory()->good())
    return;

  RunInvariantCheck();
  directory()->SaveChanges();
  RunInvariantCheck();
  directory()->SaveChanges();

  directory_.reset();
}

void TestDirectorySetterUpper::RunInvariantCheck() {
  // Check invariants for all items.
  syncable::ReadTransaction trans(FROM_HERE, directory());

  // The TestUnrecoverableErrorHandler that this directory was constructed with
  // will handle error reporting, so we can safely ignore the return value.
  directory()->FullyCheckTreeInvariants(&trans);
}

}  // namespace syncer
