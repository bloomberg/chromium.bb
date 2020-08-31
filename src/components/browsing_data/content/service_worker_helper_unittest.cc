// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/service_worker_helper.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browsing_data {
namespace {

class CannedServiceWorkerHelperTest : public testing::Test {
 public:
  content::ServiceWorkerContext* ServiceWorkerContext() {
    return content::BrowserContext::GetDefaultStoragePartition(
               &browser_context_)
        ->GetServiceWorkerContext();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
};

TEST_F(CannedServiceWorkerHelperTest, Empty) {
  const GURL origin("https://host1:1/");
  std::vector<GURL> scopes;
  scopes.push_back(GURL("https://host1:1/*"));

  scoped_refptr<CannedServiceWorkerHelper> helper(
      new CannedServiceWorkerHelper(ServiceWorkerContext()));

  ASSERT_TRUE(helper->empty());
  helper->Add(url::Origin::Create(origin));
  ASSERT_FALSE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());
}

TEST_F(CannedServiceWorkerHelperTest, Delete) {
  const GURL origin1("http://host1:9000");
  std::vector<GURL> scopes1;
  scopes1.push_back(GURL("http://host1:9000/*"));

  const GURL origin2("https://example.com");
  std::vector<GURL> scopes2;
  scopes2.push_back(GURL("https://example.com/app1/*"));
  scopes2.push_back(GURL("https://example.com/app2/*"));

  scoped_refptr<CannedServiceWorkerHelper> helper(
      new CannedServiceWorkerHelper(ServiceWorkerContext()));

  EXPECT_TRUE(helper->empty());
  helper->Add(url::Origin::Create(origin1));
  helper->Add(url::Origin::Create(origin2));
  EXPECT_EQ(2u, helper->GetCount());
  helper->DeleteServiceWorkers(origin2);
  EXPECT_EQ(1u, helper->GetCount());
}

TEST_F(CannedServiceWorkerHelperTest, IgnoreExtensionsAndDevTools) {
  const GURL origin1("chrome-extension://abcdefghijklmnopqrstuvwxyz/");
  const GURL origin2("devtools://abcdefghijklmnopqrstuvwxyz/");
  const std::vector<GURL> scopes;

  scoped_refptr<CannedServiceWorkerHelper> helper(
      new CannedServiceWorkerHelper(ServiceWorkerContext()));

  ASSERT_TRUE(helper->empty());
  helper->Add(url::Origin::Create(origin1));
  ASSERT_TRUE(helper->empty());
  helper->Add(url::Origin::Create(origin2));
  ASSERT_TRUE(helper->empty());
}

}  // namespace
}  // namespace browsing_data
