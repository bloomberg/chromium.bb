// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/model_type_store_impl.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer_v2 {

class ModelTypeStoreImplTest : public testing::Test {
 public:
  void TearDown() override {
    if (store_) {
      store_.reset();
      PumpLoop();
    }
  }

  ModelTypeStore* store() { return store_.get(); }

  void OnInitDone(ModelTypeStore::Result result,
                  scoped_ptr<ModelTypeStore> store) {
    ASSERT_EQ(ModelTypeStore::Result::SUCCESS, result);
    store_ = store.Pass();
  }

  void PumpLoop() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

 private:
  base::MessageLoop message_loop_;
  scoped_ptr<ModelTypeStore> store_;
};

// Test that CreateInMemoryStoreForTest triggers store initialization that
// results with callback being calleds with valid store pointer.
TEST_F(ModelTypeStoreImplTest, CreateInMemoryStore) {
  ModelTypeStore::CreateInMemoryStoreForTest(
      base::Bind(&ModelTypeStoreImplTest::OnInitDone, base::Unretained(this)));
  ASSERT_EQ(nullptr, store());
  PumpLoop();
  ASSERT_NE(nullptr, store());
}

}  // namespace syncer_v2
