// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/test/test_user_share.h"

#include "base/compiler_specific.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/write_transaction.h"
#include "sync/test/engine/test_directory_setter_upper.h"
#include "sync/test/engine/test_id_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

TestUserShare::TestUserShare() : dir_maker_(new TestDirectorySetterUpper()) {}

TestUserShare::~TestUserShare() {
  if (user_share_.get())
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

UserShare* TestUserShare::user_share() {
  return user_share_.get();
}

SyncEncryptionHandler* TestUserShare::encryption_handler() {
  return dir_maker_->encryption_handler();
}

/* static */
bool TestUserShare::CreateRoot(ModelType model_type, UserShare* user_share) {
  syncer::syncable::Directory* directory = user_share->directory.get();

  std::string tag_name = syncer::ModelTypeToRootTag(model_type);

  syncable::WriteTransaction wtrans(FROM_HERE, syncable::UNITTEST, directory);
  syncable::MutableEntry node(&wtrans,
                              syncable::CREATE,
                              wtrans.root_id(),
                              tag_name);
  node.Put(syncable::UNIQUE_SERVER_TAG, tag_name);
  node.Put(syncable::IS_DIR, true);
  node.Put(syncable::SERVER_IS_DIR, false);
  node.Put(syncable::IS_UNSYNCED, false);
  node.Put(syncable::IS_UNAPPLIED_UPDATE, false);
  node.Put(syncable::SERVER_VERSION, 20);
  node.Put(syncable::BASE_VERSION, 20);
  node.Put(syncable::IS_DEL, false);
  node.Put(syncable::ID, syncer::TestIdFactory::MakeServer(tag_name));
  sync_pb::EntitySpecifics specifics;
  syncer::AddDefaultFieldValue(model_type, &specifics);
  node.Put(syncable::SPECIFICS, specifics);

  return true;
}

}  // namespace syncer
