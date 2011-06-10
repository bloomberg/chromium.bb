// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_service.h"

#include "base/message_loop.h"
#include "net/base/completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/appcache/mock_appcache_storage.h"

namespace appcache {

class AppCacheServiceTest : public testing::Test {
 public:
  AppCacheServiceTest()
      : kOrigin("http://hello/"),
        service_(new AppCacheService(NULL)),
        delete_result_(net::OK), delete_completion_count_(0),
        ALLOW_THIS_IN_INITIALIZER_LIST(deletion_callback_(
            this, &AppCacheServiceTest::OnDeleteAppCachesComplete)) {
    // Setup to use mock storage.
    service_->storage_.reset(new MockAppCacheStorage(service_.get()));
  }

  void OnDeleteAppCachesComplete(int result) {
    delete_result_ = result;
    ++delete_completion_count_;
  }

  MockAppCacheStorage* mock_storage() {
    return static_cast<MockAppCacheStorage*>(service_->storage());
  }

  const GURL kOrigin;

  scoped_ptr<AppCacheService> service_;
  int delete_result_;
  int delete_completion_count_;
  net::CompletionCallbackImpl<AppCacheServiceTest> deletion_callback_;
};

TEST_F(AppCacheServiceTest, DeleteAppCachesForOrigin) {
  // Without giving mock storage simiulated info, should fail.
  service_->DeleteAppCachesForOrigin(kOrigin, &deletion_callback_);
  EXPECT_EQ(0, delete_completion_count_);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(1, delete_completion_count_);
  EXPECT_EQ(net::ERR_FAILED, delete_result_);
  delete_completion_count_ = 0;

  // Should succeed given an empty info collection.
  mock_storage()->SimulateGetAllInfo(new AppCacheInfoCollection);
  service_->DeleteAppCachesForOrigin(kOrigin, &deletion_callback_);
  EXPECT_EQ(0, delete_completion_count_);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(1, delete_completion_count_);
  EXPECT_EQ(net::OK, delete_result_);
  delete_completion_count_ = 0;

  scoped_refptr<AppCacheInfoCollection> info(new AppCacheInfoCollection);

  // Should succeed given a non-empty info collection.
  AppCacheInfo mock_manifest_1;
  AppCacheInfo mock_manifest_2;
  AppCacheInfo mock_manifest_3;
  mock_manifest_1.manifest_url = kOrigin.Resolve("manifest1");
  mock_manifest_2.manifest_url = kOrigin.Resolve("manifest2");
  mock_manifest_3.manifest_url = kOrigin.Resolve("manifest3");
  AppCacheInfoVector info_vector;
  info_vector.push_back(mock_manifest_1);
  info_vector.push_back(mock_manifest_2);
  info_vector.push_back(mock_manifest_3);
  info->infos_by_origin[kOrigin] = info_vector;
  mock_storage()->SimulateGetAllInfo(info);
  service_->DeleteAppCachesForOrigin(kOrigin, &deletion_callback_);
  EXPECT_EQ(0, delete_completion_count_);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(1, delete_completion_count_);
  EXPECT_EQ(net::OK, delete_result_);
  delete_completion_count_ = 0;

  // Should fail if storage fails to delete.
  info->infos_by_origin[kOrigin] = info_vector;
  mock_storage()->SimulateGetAllInfo(info);
  mock_storage()->SimulateMakeGroupObsoleteFailure();
  service_->DeleteAppCachesForOrigin(kOrigin, &deletion_callback_);
  EXPECT_EQ(0, delete_completion_count_);
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(1, delete_completion_count_);
  EXPECT_EQ(net::ERR_FAILED, delete_result_);
  delete_completion_count_ = 0;

  // Should complete with abort error if the service is deleted
  // prior to a delete completion.
  service_->DeleteAppCachesForOrigin(kOrigin, &deletion_callback_);
  EXPECT_EQ(0, delete_completion_count_);
  service_.reset();  // kill it
  EXPECT_EQ(1, delete_completion_count_);
  EXPECT_EQ(net::ERR_ABORTED, delete_result_);
  delete_completion_count_ = 0;

  // Let any tasks lingering from the sudden deletion run and verify
  // no other completion calls occur.
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(0, delete_completion_count_);
}

}  // namespace appcache
