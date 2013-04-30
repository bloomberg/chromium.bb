// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/test/test_user_share.h"

#include "base/compiler_specific.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/syncable_read_transaction.h"
#include "sync/syncable/syncable_write_transaction.h"
#include "sync/test/engine/test_directory_setter_upper.h"
#include "sync/test/engine/test_id_factory.h"
#include "sync/test/engine/test_syncable_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

TestUserShare::TestUserShare() : dir_maker_(new TestDirectorySetterUpper()) {}

TestUserShare::~TestUserShare() {
  if (user_share_)
    ADD_FAILURE() << "Should have called TestUserShare::TearDown()";
}

void TestUserShare::SetUp() {
  user_share_.reset(new UserShare());
  dir_maker_->SetUp();

  // The pointer is owned by dir_maker_, we should not be storing it in a
  // scoped_ptr.  We must be careful to ensure the scoped_ptr never deletes it.
  user_share_->directory.reset(dir_maker_->directory());
}

void TestUserShare::TearDown() {
  // Ensure the scoped_ptr doesn't delete the memory we don't own.
  ignore_result(user_share_->directory.release());

  user_share_.reset();
  dir_maker_->TearDown();
}

bool TestUserShare::Reload() {
  if (!user_share_->directory->SaveChanges())
    return false;

  syncer::syncable::DirectoryBackingStore* saved_store =
      user_share_->directory->store_.release();

  // Ensure the scoped_ptr doesn't delete the memory we don't own.
  ignore_result(user_share_->directory.release());
  user_share_.reset(new UserShare());
  dir_maker_->SetUpWith(saved_store);
  user_share_->directory.reset(dir_maker_->directory());
  return true;
}

UserShare* TestUserShare::user_share() {
  return user_share_.get();
}

SyncEncryptionHandler* TestUserShare::encryption_handler() {
  return dir_maker_->encryption_handler();
}

syncable::TestTransactionObserver* TestUserShare::transaction_observer() {
  return dir_maker_->transaction_observer();
}

/* static */
bool TestUserShare::CreateRoot(ModelType model_type, UserShare* user_share) {
  syncer::syncable::Directory* directory = user_share->directory.get();
  syncable::WriteTransaction wtrans(FROM_HERE, syncable::UNITTEST, directory);
  CreateTypeRoot(&wtrans, directory, model_type);
  return true;
}

size_t TestUserShare::GetDeleteJournalSize() const {
  syncable::ReadTransaction trans(FROM_HERE, user_share_->directory.get());
  return user_share_->directory->delete_journal()->GetDeleteJournalSize(&trans);
}

}  // namespace syncer
