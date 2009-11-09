// Copyright (c) 2009 The Chromium Authos. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_response.h"
#include "webkit/appcache/appcache_storage.h"
#include "webkit/appcache/mock_appcache_service.h"

namespace appcache {

class AppCacheStorageTest : public testing::Test {
 public:
  class MockStorageDelegate : public AppCacheStorage::Delegate {
   public:
  };
};

TEST_F(AppCacheStorageTest, AddRemoveCache) {
  MockAppCacheService service;
  scoped_refptr<AppCache> cache = new AppCache(&service, 111);

  EXPECT_EQ(cache.get(),
            service.storage()->working_set()->GetCache(111));

  service.storage()->working_set()->RemoveCache(cache);

  EXPECT_TRUE(!service.storage()->working_set()->GetCache(111));

  // Removing non-existing cache from service should not fail.
  MockAppCacheService dummy;
  dummy.storage()->working_set()->RemoveCache(cache);
}

TEST_F(AppCacheStorageTest, AddRemoveGroup) {
  MockAppCacheService service;
  scoped_refptr<AppCacheGroup> group =
      new AppCacheGroup(&service, GURL::EmptyGURL());

  EXPECT_EQ(group.get(),
            service.storage()->working_set()->GetGroup(GURL::EmptyGURL()));

  service.storage()->working_set()->RemoveGroup(group);

  EXPECT_TRUE(!service.storage()->working_set()->GetGroup(GURL::EmptyGURL()));

  // Removing non-existing group from service should not fail.
  MockAppCacheService dummy;
  dummy.storage()->working_set()->RemoveGroup(group);
}

TEST_F(AppCacheStorageTest, AddRemoveResponseInfo) {
  MockAppCacheService service;
  scoped_refptr<AppCacheResponseInfo> info =
      new AppCacheResponseInfo(&service, GURL(),
                               111, new net::HttpResponseInfo);

  EXPECT_EQ(info.get(),
            service.storage()->working_set()->GetResponseInfo(111));

  service.storage()->working_set()->RemoveResponseInfo(info);

  EXPECT_TRUE(!service.storage()->working_set()->GetResponseInfo(111));

  // Removing non-existing info from service should not fail.
  MockAppCacheService dummy;
  dummy.storage()->working_set()->RemoveResponseInfo(info);
}

TEST_F(AppCacheStorageTest, DelegateReferences) {
  typedef scoped_refptr<AppCacheStorage::DelegateReference>
      ScopedDelegateReference;
  MockAppCacheService service;
  MockStorageDelegate delegate;
  ScopedDelegateReference delegate_reference1;
  ScopedDelegateReference delegate_reference2;

  EXPECT_FALSE(service.storage()->GetDelegateReference(&delegate));

  delegate_reference1 =
      service.storage()->GetOrCreateDelegateReference(&delegate);
  EXPECT_TRUE(delegate_reference1.get());
  EXPECT_TRUE(delegate_reference1->HasOneRef());
  EXPECT_TRUE(service.storage()->GetDelegateReference(&delegate));
  EXPECT_EQ(&delegate,
            service.storage()->GetDelegateReference(&delegate)->delegate);
  EXPECT_EQ(service.storage()->GetDelegateReference(&delegate),
            service.storage()->GetOrCreateDelegateReference(&delegate));
  delegate_reference1 = NULL;
  EXPECT_FALSE(service.storage()->GetDelegateReference(&delegate));

  delegate_reference1 =
      service.storage()->GetOrCreateDelegateReference(&delegate);
  service.storage()->CancelDelegateCallbacks(&delegate);
  EXPECT_TRUE(delegate_reference1.get());
  EXPECT_TRUE(delegate_reference1->HasOneRef());
  EXPECT_FALSE(delegate_reference1->delegate);
  EXPECT_FALSE(service.storage()->GetDelegateReference(&delegate));

  delegate_reference2 =
      service.storage()->GetOrCreateDelegateReference(&delegate);
  EXPECT_TRUE(delegate_reference2.get());
  EXPECT_TRUE(delegate_reference2->HasOneRef());
  EXPECT_EQ(&delegate, delegate_reference2->delegate);
  EXPECT_NE(delegate_reference1.get(), delegate_reference2.get());
}

}  // namespace appcache
