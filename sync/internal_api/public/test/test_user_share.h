// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A handy class that takes care of setting up and destroying a
// UserShare instance for unit tests that require one.
//
// The expected usage is to make this a component of your test fixture:
//
//   class AwesomenessTest : public testing::Test {
//    public:
//     virtual void SetUp() {
//       test_user_share_.SetUp();
//     }
//     virtual void TearDown() {
//       test_user_share_.TearDown();
//     }
//    protected:
//     TestUserShare test_user_share_;
//   };
//
// Then, in your tests:
//
//   TEST_F(AwesomenessTest, IsMaximal) {
//     ReadTransaction trans(test_user_share_.user_share());
//     ...
//   }
//

#ifndef SYNC_INTERNAL_API_PUBLIC_TEST_TEST_USER_SHARE_H_
#define SYNC_INTERNAL_API_PUBLIC_TEST_TEST_USER_SHARE_H_

#include "base/basictypes.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/user_share.h"

namespace syncer {

class SyncEncryptionHandler;
class TestDirectorySetterUpper;

namespace syncable {
  class TestTransactionObserver;
}

class TestUserShare {
 public:
  TestUserShare();
  ~TestUserShare();

  // Sets up the UserShare instance.  Clears any existing database
  // backing files that might exist on disk.
  void SetUp();

  // Undo everything done by SetUp(): closes the UserShare and deletes
  // the backing files. Before closing the directory, this will run
  // the directory invariant checks and perform the SaveChanges action
  // on the user share's directory.
  void TearDown();

  // Save and reload Directory to clear out temporary data in memory.
  bool Reload();

  // Non-NULL iff called between a call to SetUp() and TearDown().
  UserShare* user_share();

  // Sync's encryption handler. Used by tests to invoke the sync encryption
  // methods normally handled via the SyncBackendHost
  SyncEncryptionHandler* encryption_handler();

  // Returns the directory's transaction observer.  This transaction observer
  // has methods which can be helpful when writing test assertions.
  syncable::TestTransactionObserver* transaction_observer();

  // A helper function to pretend to download this type's root node.
  static bool CreateRoot(syncer::ModelType model_type,
                         syncer::UserShare* service);

  size_t GetDeleteJournalSize() const;

 private:
  scoped_ptr<TestDirectorySetterUpper> dir_maker_;
  scoped_ptr<UserShare> user_share_;

  DISALLOW_COPY_AND_ASSIGN(TestUserShare);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_TEST_TEST_USER_SHARE_H_
