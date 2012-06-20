// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/test/test_user_share.h"

#include "base/compiler_specific.h"
#include "sync/syncable/directory.h"
#include "sync/test/engine/test_directory_setter_upper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

TestUserShare::TestUserShare() : dir_maker_(new TestDirectorySetterUpper()) {}

TestUserShare::~TestUserShare() {
  if (user_share_.get())
    ADD_FAILURE() << "Should have called TestUserShare::TearDown()";
}

void TestUserShare::SetUp() {
  user_share_.reset(new sync_api::UserShare());
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

sync_api::UserShare* TestUserShare::user_share() {
  return user_share_.get();
}

}  // namespace browser_sync
