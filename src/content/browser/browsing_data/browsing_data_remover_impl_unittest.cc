﻿// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/browsing_data_remover_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/post_task.h"
#include "base/test/gmock_callback_support.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_storage_partition.h"
#include "content/public/test/test_utils.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_deletion_info.h"
#include "net/cookies/cookie_store.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/ssl/ssl_client_cert_type.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/cookie_manager.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/test/test_network_context.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_REPORTING)
#include "net/network_error_logging/mock_persistent_nel_store.h"
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/reporting/mock_persistent_reporting_store.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_endpoint.h"
#include "net/reporting/reporting_report.h"
#include "net/reporting/reporting_service.h"
#include "net/reporting/reporting_test_util.h"
#endif  // BUILDFLAG(ENABLE_REPORTING)

using base::test::RunOnceClosure;
using testing::_;
using testing::ByRef;
using testing::Eq;
using testing::Invoke;
using testing::IsEmpty;
using testing::MakeMatcher;
using testing::Matcher;
using testing::MatcherInterface;
using testing::MatchResultListener;
using testing::Not;
using testing::Return;
using testing::SizeIs;
using testing::StrictMock;
using testing::Truly;
using testing::UnorderedElementsAre;
using testing::WithArgs;
using CookieDeletionFilterPtr = network::mojom::CookieDeletionFilterPtr;

namespace content {

namespace {

struct StoragePartitionRemovalData {
  StoragePartitionRemovalData()
      : remove_mask(0),
        quota_storage_remove_mask(0),
        cookie_deletion_filter(network::mojom::CookieDeletionFilter::New()),
        remove_code_cache(false) {}

  StoragePartitionRemovalData(const StoragePartitionRemovalData& other)
      : remove_mask(other.remove_mask),
        quota_storage_remove_mask(other.quota_storage_remove_mask),
        remove_begin(other.remove_begin),
        remove_end(other.remove_end),
        origin_matcher(other.origin_matcher),
        cookie_deletion_filter(other.cookie_deletion_filter.Clone()),
        remove_code_cache(other.remove_code_cache),
        url_matcher(other.url_matcher) {}

  StoragePartitionRemovalData& operator=(
      const StoragePartitionRemovalData& rhs) {
    remove_mask = rhs.remove_mask;
    quota_storage_remove_mask = rhs.quota_storage_remove_mask;
    remove_begin = rhs.remove_begin;
    remove_end = rhs.remove_end;
    origin_matcher = rhs.origin_matcher;
    cookie_deletion_filter = rhs.cookie_deletion_filter.Clone();
    remove_code_cache = rhs.remove_code_cache;
    url_matcher = rhs.url_matcher;
    return *this;
  }

  uint32_t remove_mask;
  uint32_t quota_storage_remove_mask;
  base::Time remove_begin;
  base::Time remove_end;
  StoragePartition::OriginMatcherFunction origin_matcher;
  CookieDeletionFilterPtr cookie_deletion_filter;
  bool remove_code_cache;
  base::RepeatingCallback<bool(const GURL&)> url_matcher;
};

net::CanonicalCookie CreateCookieWithHost(const url::Origin& origin) {
  std::unique_ptr<net::CanonicalCookie> cookie(
      std::make_unique<net::CanonicalCookie>(
          "A", "1", origin.host(), "/", base::Time::Now(), base::Time::Now(),
          base::Time(), false, false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_MEDIUM));
  EXPECT_TRUE(cookie);
  return *cookie;
}

class StoragePartitionRemovalTestStoragePartition
    : public TestStoragePartition {
 public:
  StoragePartitionRemovalTestStoragePartition() {
    set_network_context(&network_context_);
  }
  ~StoragePartitionRemovalTestStoragePartition() override = default;

  void ClearDataForOrigin(uint32_t remove_mask,
                          uint32_t quota_storage_remove_mask,
                          const GURL& storage_origin) override {}

  void ClearData(uint32_t remove_mask,
                 uint32_t quota_storage_remove_mask,
                 const GURL& storage_origin,
                 const base::Time begin,
                 const base::Time end,
                 base::OnceClosure callback) override {
    // Store stuff to verify parameters' correctness later.
    storage_partition_removal_data_.remove_mask = remove_mask;
    storage_partition_removal_data_.quota_storage_remove_mask =
        quota_storage_remove_mask;
    storage_partition_removal_data_.remove_begin = begin;
    storage_partition_removal_data_.remove_end = end;

    base::PostTask(FROM_HERE, {BrowserThread::UI}, std::move(callback));
  }

  void ClearData(uint32_t remove_mask,
                 uint32_t quota_storage_remove_mask,
                 OriginMatcherFunction origin_matcher,
                 CookieDeletionFilterPtr cookie_deletion_filter,
                 bool perform_storage_cleanup,
                 const base::Time begin,
                 const base::Time end,
                 base::OnceClosure callback) override {
    // Store stuff to verify parameters' correctness later.
    storage_partition_removal_data_.remove_mask = remove_mask;
    storage_partition_removal_data_.quota_storage_remove_mask =
        quota_storage_remove_mask;
    storage_partition_removal_data_.remove_begin = begin;
    storage_partition_removal_data_.remove_end = end;
    storage_partition_removal_data_.origin_matcher = std::move(origin_matcher);
    storage_partition_removal_data_.cookie_deletion_filter =
        std::move(cookie_deletion_filter);

    base::PostTask(FROM_HERE, {BrowserThread::UI}, std::move(callback));
  }

  void ClearCodeCaches(
      base::Time begin,
      base::Time end,
      const base::RepeatingCallback<bool(const GURL&)>& url_matcher,
      base::OnceClosure callback) override {
    storage_partition_removal_data_.remove_code_cache = true;
    storage_partition_removal_data_.remove_begin = begin;
    storage_partition_removal_data_.remove_end = end;
    storage_partition_removal_data_.url_matcher = url_matcher;
  }

  const StoragePartitionRemovalData& GetStoragePartitionRemovalData() const {
    return storage_partition_removal_data_;
  }

 private:
  StoragePartitionRemovalData storage_partition_removal_data_;
  network::TestNetworkContext network_context_;

  DISALLOW_COPY_AND_ASSIGN(StoragePartitionRemovalTestStoragePartition);
};

// Custom matcher to test the equivalence of two URL filters. Since those are
// blackbox predicates, we can only approximate the equivalence by testing
// whether the filter give the same answer for several URLs. This is currently
// good enough for our testing purposes, to distinguish whitelists
// and blacklists, empty and non-empty filters and such.
// TODO(msramek): BrowsingDataRemover and some of its backends support URL
// filters, but its constructor currently only takes a single URL and constructs
// its own url filter. If an url filter was directly passed to
// BrowsingDataRemover (what should eventually be the case), we can use the same
// instance in the test as well, and thus simply test
// base::RepeatingCallback::Equals() in this matcher.
class ProbablySameFilterMatcher
    : public MatcherInterface<
          const base::RepeatingCallback<bool(const GURL&)>&> {
 public:
  explicit ProbablySameFilterMatcher(
      const base::RepeatingCallback<bool(const GURL&)>& filter)
      : to_match_(filter) {}

  bool MatchAndExplain(const base::RepeatingCallback<bool(const GURL&)>& filter,
                       MatchResultListener* listener) const override {
    if (!filter && !to_match_)
      return true;
    if (!filter || !to_match_)
      return false;

    const GURL urls_to_test_[] = {GURL("a.com"), GURL("b.com"), GURL("c.com"),
                                  GURL("invalid spec")};
    for (GURL url : urls_to_test_) {
      if (filter.Run(url) != to_match_.Run(url)) {
        if (listener)
          *listener << "The filters differ on the URL " << url;
        return false;
      }
    }
    return true;
  }

  void DescribeTo(::std::ostream* os) const override {
    *os << "is probably the same url filter as " << &to_match_;
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << "is definitely NOT the same url filter as " << &to_match_;
  }

 private:
  const base::RepeatingCallback<bool(const GURL&)>& to_match_;
};

inline Matcher<const base::RepeatingCallback<bool(const GURL&)>&>
ProbablySameFilter(const base::RepeatingCallback<bool(const GURL&)>& filter) {
  return MakeMatcher(new ProbablySameFilterMatcher(filter));
}

base::Time AnHourAgo() {
  return base::Time::Now() - base::TimeDelta::FromHours(1);
}

bool FilterMatchesCookie(const CookieDeletionFilterPtr& filter,
                         const net::CanonicalCookie& cookie) {
  return network::DeletionFilterToInfo(filter.Clone()).Matches(cookie);
}

}  // namespace

// Testers -------------------------------------------------------------------

class RemoveDownloadsTester {
 public:
  explicit RemoveDownloadsTester(BrowserContext* browser_context)
      : download_manager_(new MockDownloadManager()) {
    BrowserContext::SetDownloadManagerForTesting(
        browser_context, base::WrapUnique(download_manager_));
    EXPECT_EQ(download_manager_,
              BrowserContext::GetDownloadManager(browser_context));
    EXPECT_CALL(*download_manager_, Shutdown());
  }

  ~RemoveDownloadsTester() = default;

  MockDownloadManager* download_manager() { return download_manager_; }

 private:
  MockDownloadManager* download_manager_;  // Owned by browser context.

  DISALLOW_COPY_AND_ASSIGN(RemoveDownloadsTester);
};

// Test Class ----------------------------------------------------------------

class BrowsingDataRemoverImplTest : public testing::Test {
 public:
  BrowsingDataRemoverImplTest() : browser_context_(new TestBrowserContext()) {
    remover_ = static_cast<BrowsingDataRemoverImpl*>(
        BrowserContext::GetBrowsingDataRemover(browser_context_.get()));
  }

  ~BrowsingDataRemoverImplTest() override = default;

  void TearDown() override {
    mock_policy_ = nullptr;

    // BrowserContext contains a DOMStorageContext. BrowserContext's
    // destructor posts a message to the WEBKIT thread to delete some of its
    // member variables. We need to ensure that the browser context is
    // destroyed, and that the message loop is cleared out, before destroying
    // the threads and loop. Otherwise we leak memory.
    browser_context_.reset();
    RunAllTasksUntilIdle();
  }

  void BlockUntilBrowsingDataRemoved(const base::Time& delete_begin,
                                     const base::Time& delete_end,
                                     int remove_mask,
                                     bool include_protected_origins) {
    // TODO(msramek): Consider moving |storage_partition| to the test fixture.
    StoragePartitionRemovalTestStoragePartition storage_partition;

    if (network_context_override_) {
      storage_partition.set_network_context(network_context_override_);
    }

    remover_->OverrideStoragePartitionForTesting(&storage_partition);

    int origin_type_mask = BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB;
    if (include_protected_origins)
      origin_type_mask |= BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB;

    BrowsingDataRemoverCompletionObserver completion_observer(remover_);
    remover_->RemoveAndReply(delete_begin, delete_end, remove_mask,
                             origin_type_mask, &completion_observer);
    completion_observer.BlockUntilCompletion();

    // Save so we can verify later.
    storage_partition_removal_data_ =
        storage_partition.GetStoragePartitionRemovalData();
  }

  void BlockUntilOriginDataRemoved(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      int remove_mask,
      std::unique_ptr<BrowsingDataFilterBuilder> filter_builder) {
    StoragePartitionRemovalTestStoragePartition storage_partition;

    if (network_context_override_) {
      storage_partition.set_network_context(network_context_override_);
    }

    remover_->OverrideStoragePartitionForTesting(&storage_partition);

    BrowsingDataRemoverCompletionObserver completion_observer(remover_);
    remover_->RemoveWithFilterAndReply(
        delete_begin, delete_end, remove_mask,
        BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
        std::move(filter_builder), &completion_observer);
    completion_observer.BlockUntilCompletion();

    // Save so we can verify later.
    storage_partition_removal_data_ =
        storage_partition.GetStoragePartitionRemovalData();
  }

  BrowserContext* GetBrowserContext() { return browser_context_.get(); }

  void DestroyBrowserContext() { browser_context_.reset(); }

  const base::Time& GetBeginTime() {
    return remover_->GetLastUsedBeginTimeForTesting();
  }

  int GetRemovalMask() { return remover_->GetLastUsedRemovalMaskForTesting(); }

  int GetOriginTypeMask() {
    return remover_->GetLastUsedOriginTypeMaskForTesting();
  }

  const StoragePartitionRemovalData& GetStoragePartitionRemovalData() const {
    return storage_partition_removal_data_;
  }

  storage::MockSpecialStoragePolicy* CreateMockPolicy() {
    mock_policy_ = base::MakeRefCounted<storage::MockSpecialStoragePolicy>();
    return mock_policy_.get();
  }

  storage::MockSpecialStoragePolicy* mock_policy() {
    return mock_policy_.get();
  }

  void set_network_context_override(network::mojom::NetworkContext* context) {
    network_context_override_ = context;
  }

  bool Match(const GURL& origin,
             int mask,
             storage::SpecialStoragePolicy* policy) {
    return remover_->DoesOriginMatchMaskForTesting(
        mask, url::Origin::Create(origin), policy);
  }

 private:
  // Cached pointer to BrowsingDataRemoverImpl for access to testing methods.
  BrowsingDataRemoverImpl* remover_;

  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<BrowserContext> browser_context_;

  network::mojom::NetworkContext* network_context_override_ = nullptr;

  StoragePartitionRemovalData storage_partition_removal_data_;

  scoped_refptr<storage::MockSpecialStoragePolicy> mock_policy_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataRemoverImplTest);
};

// Tests ---------------------------------------------------------------------

TEST_F(BrowsingDataRemoverImplTest, RemoveCookieForever) {
  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                BrowsingDataRemover::DATA_TYPE_COOKIES, false);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_COOKIES, GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify that storage partition was instructed to remove the cookies.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_COOKIES);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
  EXPECT_EQ(removal_data.remove_begin, GetBeginTime());
}

TEST_F(BrowsingDataRemoverImplTest, RemoveCookieLastHour) {
  BlockUntilBrowsingDataRemoved(AnHourAgo(), base::Time::Max(),
                                BrowsingDataRemover::DATA_TYPE_COOKIES, false);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_COOKIES, GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify that storage partition was instructed to remove the cookies.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_COOKIES);
  // Removing with time period other than all time should not clear
  // persistent storage data.
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            ~StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT);
  EXPECT_EQ(removal_data.remove_begin, GetBeginTime());
}

TEST_F(BrowsingDataRemoverImplTest, RemoveCookiesDomainBlacklist) {
  std::unique_ptr<BrowsingDataFilterBuilder> filter(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::BLACKLIST));
  const GURL kTestUrl1("http://host1.com");
  const GURL kTestUrl3("http://host3.com");
  filter->AddRegisterableDomain(kTestUrl1.host());
  filter->AddRegisterableDomain(kTestUrl3.host());
  BlockUntilOriginDataRemoved(AnHourAgo(), base::Time::Max(),
                              BrowsingDataRemover::DATA_TYPE_COOKIES,
                              std::move(filter));

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_COOKIES, GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify that storage partition was instructed to remove the cookies.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_COOKIES);
  // Removing with time period other than all time should not clear
  // persistent storage data.
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            ~StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT);
  EXPECT_EQ(removal_data.remove_begin, GetBeginTime());
  const url::Origin kTestOrigin1 = url::Origin::Create(kTestUrl1);
  const url::Origin kTestOrigin2 =
      url::Origin::Create(GURL("http://host2.com"));
  const url::Origin kTestOrigin3 = url::Origin::Create(kTestUrl3);
  const url::Origin kTestOrigin3Secure =
      url::Origin::Create(GURL("https://host3.com"));
  EXPECT_FALSE(removal_data.origin_matcher.Run(kTestOrigin1, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(kTestOrigin2, mock_policy()));
  EXPECT_FALSE(removal_data.origin_matcher.Run(kTestOrigin3, mock_policy()));
  // Even though it's a different origin, it's the same domain.
  EXPECT_FALSE(
      removal_data.origin_matcher.Run(kTestOrigin3Secure, mock_policy()));

  EXPECT_FALSE(FilterMatchesCookie(removal_data.cookie_deletion_filter,
                                   CreateCookieWithHost(kTestOrigin1)));
  EXPECT_TRUE(FilterMatchesCookie(removal_data.cookie_deletion_filter,
                                  CreateCookieWithHost(kTestOrigin2)));
  EXPECT_FALSE(FilterMatchesCookie(removal_data.cookie_deletion_filter,
                                   CreateCookieWithHost(kTestOrigin3)));
  // This is false, because this is the same domain as 3, just with a different
  // scheme.
  EXPECT_FALSE(FilterMatchesCookie(removal_data.cookie_deletion_filter,
                                   CreateCookieWithHost(kTestOrigin3Secure)));
}

TEST_F(BrowsingDataRemoverImplTest, RemoveUnprotectedLocalStorageForever) {
  storage::MockSpecialStoragePolicy* policy = CreateMockPolicy();
  // Protect the test origin.
  const url::Origin kTestOrigin = url::Origin::Create(GURL("http://host1.com"));
  policy->AddProtected(kTestOrigin.GetURL());

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE,
                                false);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE, GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify that storage partition was instructed to remove the data correctly.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
  EXPECT_EQ(removal_data.remove_begin, GetBeginTime());

  // Check origin matcher.
  EXPECT_FALSE(removal_data.origin_matcher.Run(kTestOrigin, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("http://host2.com")), mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("http://host3.com")), mock_policy()));
  EXPECT_FALSE(removal_data.origin_matcher.Run(
      url::Origin::Create(
          GURL("chrome-extension://abcdefghijklmnopqrstuvwxyz/")),
      mock_policy()));
}

TEST_F(BrowsingDataRemoverImplTest, RemoveProtectedLocalStorageForever) {
  // Protect the test origin.
  storage::MockSpecialStoragePolicy* policy = CreateMockPolicy();
  const url::Origin kTestOrigin = url::Origin::Create(GURL("http://host1.com"));
  policy->AddProtected(kTestOrigin.GetURL());

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE,
                                true);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE, GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
                BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
            GetOriginTypeMask());

  // Verify that storage partition was instructed to remove the data correctly.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
  EXPECT_EQ(removal_data.remove_begin, GetBeginTime());

  // Check origin matcher all http origin will match since we specified
  // both protected and unprotected.
  EXPECT_TRUE(removal_data.origin_matcher.Run(kTestOrigin, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("http://host2.com")), mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("http://host3.com")), mock_policy()));
  EXPECT_FALSE(removal_data.origin_matcher.Run(
      url::Origin::Create(
          GURL("chrome-extension://abcdefghijklmnopqrstuvwxyz/")),
      mock_policy()));
}

TEST_F(BrowsingDataRemoverImplTest, RemoveLocalStorageForLastWeek) {
  CreateMockPolicy();

  BlockUntilBrowsingDataRemoved(
      base::Time::Now() - base::TimeDelta::FromDays(7), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE, false);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE, GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify that storage partition was instructed to remove the data correctly.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE);
  // Persistent storage won't be deleted.
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            ~StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT);
  EXPECT_EQ(removal_data.remove_begin, GetBeginTime());

  // Check origin matcher.
  EXPECT_TRUE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("http://host1.com")), mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("http://host2.com")), mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("http://host3.com")), mock_policy()));
  EXPECT_FALSE(removal_data.origin_matcher.Run(
      url::Origin::Create(
          GURL("chrome-extension://abcdefghijklmnopqrstuvwxyz/")),
      mock_policy()));
}

TEST_F(BrowsingDataRemoverImplTest, RemoveMultipleTypes) {
  // Downloads should be deleted through the DownloadManager, assure it would
  // be called.
  RemoveDownloadsTester downloads_tester(GetBrowserContext());
  EXPECT_CALL(*downloads_tester.download_manager(),
              RemoveDownloadsByURLAndTime(_, _, _));

  int removal_mask = BrowsingDataRemover::DATA_TYPE_DOWNLOADS |
                     BrowsingDataRemover::DATA_TYPE_COOKIES;

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(), removal_mask,
                                false);

  EXPECT_EQ(removal_mask, GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // The cookie would be deleted throught the StorageParition, check if the
  // partition was requested to remove cookie.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_COOKIES);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
}

TEST_F(BrowsingDataRemoverImplTest, RemoveQuotaManagedDataForeverBoth) {
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
          BrowsingDataRemover::DATA_TYPE_WEB_SQL |
          BrowsingDataRemover::DATA_TYPE_APP_CACHE |
          BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
          BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
          BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
      false);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
                BrowsingDataRemover::DATA_TYPE_WEB_SQL |
                BrowsingDataRemover::DATA_TYPE_APP_CACHE |
                BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
                BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
                BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
}

TEST_F(BrowsingDataRemoverImplTest,
       RemoveQuotaManagedDataForeverOnlyTemporary) {
  CreateMockPolicy();

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
          BrowsingDataRemover::DATA_TYPE_WEB_SQL |
          BrowsingDataRemover::DATA_TYPE_APP_CACHE |
          BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
          BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
          BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
      false);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
                BrowsingDataRemover::DATA_TYPE_WEB_SQL |
                BrowsingDataRemover::DATA_TYPE_APP_CACHE |
                BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
                BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
                BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);

  // Check that all related origin data would be removed, that is, origin
  // matcher would match these origin.
  EXPECT_TRUE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("http://host1.com")), mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("http://host2.com")), mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("http://host3.com")), mock_policy()));
}

TEST_F(BrowsingDataRemoverImplTest,
       RemoveQuotaManagedDataForeverOnlyPersistent) {
  CreateMockPolicy();

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
          BrowsingDataRemover::DATA_TYPE_WEB_SQL |
          BrowsingDataRemover::DATA_TYPE_APP_CACHE |
          BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
          BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
          BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
      false);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
                BrowsingDataRemover::DATA_TYPE_WEB_SQL |
                BrowsingDataRemover::DATA_TYPE_APP_CACHE |
                BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
                BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
                BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);

  // Check that all related origin data would be removed, that is, origin
  // matcher would match these origin.
  EXPECT_TRUE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("http://host1.com")), mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("http://host2.com")), mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("http://host3.com")), mock_policy()));
}

TEST_F(BrowsingDataRemoverImplTest, RemoveQuotaManagedDataForeverNeither) {
  CreateMockPolicy();

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
          BrowsingDataRemover::DATA_TYPE_WEB_SQL |
          BrowsingDataRemover::DATA_TYPE_APP_CACHE |
          BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
          BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
          BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
      false);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
                BrowsingDataRemover::DATA_TYPE_WEB_SQL |
                BrowsingDataRemover::DATA_TYPE_APP_CACHE |
                BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
                BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
                BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);

  // Check that all related origin data would be removed, that is, origin
  // matcher would match these origin.
  EXPECT_TRUE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("http://host1.com")), mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("http://host2.com")), mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("http://host3.com")), mock_policy()));
}

TEST_F(BrowsingDataRemoverImplTest,
       RemoveQuotaManagedDataForeverSpecificOrigin) {
  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST));
  const GURL kTestUrl("http://host1.com");
  builder->AddRegisterableDomain(kTestUrl.host());
  // Remove the test origin.
  BlockUntilOriginDataRemoved(
      base::Time(), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_APP_CACHE |
          BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
          BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
          BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
          BrowsingDataRemover::DATA_TYPE_INDEXED_DB |
          BrowsingDataRemover::DATA_TYPE_WEB_SQL,
      std::move(builder));

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_APP_CACHE |
                BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
                BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
                BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
                BrowsingDataRemover::DATA_TYPE_INDEXED_DB |
                BrowsingDataRemover::DATA_TYPE_WEB_SQL,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);
  EXPECT_TRUE(removal_data.origin_matcher.Run(url::Origin::Create(kTestUrl),
                                              mock_policy()));
  EXPECT_FALSE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("http://host2.com")), mock_policy()));
  EXPECT_FALSE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("http://host3.com")), mock_policy()));
  EXPECT_FALSE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("https://host3.com")), mock_policy()));
}

TEST_F(BrowsingDataRemoverImplTest, RemoveQuotaManagedDataForLastHour) {
  BlockUntilBrowsingDataRemoved(
      AnHourAgo(), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
          BrowsingDataRemover::DATA_TYPE_WEB_SQL |
          BrowsingDataRemover::DATA_TYPE_APP_CACHE |
          BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
          BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
          BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
      false);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
                BrowsingDataRemover::DATA_TYPE_WEB_SQL |
                BrowsingDataRemover::DATA_TYPE_APP_CACHE |
                BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
                BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
                BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);

  // Persistent data would be left out since we are not removing from
  // beginning of time.
  uint32_t expected_quota_mask =
      ~StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT;
  EXPECT_EQ(removal_data.quota_storage_remove_mask, expected_quota_mask);
  // Check removal begin time.
  EXPECT_EQ(removal_data.remove_begin, GetBeginTime());
}

TEST_F(BrowsingDataRemoverImplTest, RemoveQuotaManagedDataForLastWeek) {
  BlockUntilBrowsingDataRemoved(
      base::Time::Now() - base::TimeDelta::FromDays(7), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
          BrowsingDataRemover::DATA_TYPE_WEB_SQL |
          BrowsingDataRemover::DATA_TYPE_APP_CACHE |
          BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
          BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
          BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
      false);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
                BrowsingDataRemover::DATA_TYPE_WEB_SQL |
                BrowsingDataRemover::DATA_TYPE_APP_CACHE |
                BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
                BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
                BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);

  // Persistent data would be left out since we are not removing from
  // beginning of time.
  uint32_t expected_quota_mask =
      ~StoragePartition::QUOTA_MANAGED_STORAGE_MASK_PERSISTENT;
  EXPECT_EQ(removal_data.quota_storage_remove_mask, expected_quota_mask);
  // Check removal begin time.
  EXPECT_EQ(removal_data.remove_begin, GetBeginTime());
}

TEST_F(BrowsingDataRemoverImplTest, RemoveQuotaManagedUnprotectedOrigins) {
  storage::MockSpecialStoragePolicy* policy = CreateMockPolicy();
  // Protect the test origin.
  const url::Origin kTestOrigin = url::Origin::Create(GURL("http://host1.com"));
  policy->AddProtected(kTestOrigin.GetURL());

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
          BrowsingDataRemover::DATA_TYPE_WEB_SQL |
          BrowsingDataRemover::DATA_TYPE_APP_CACHE |
          BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
          BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
          BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
      false);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
                BrowsingDataRemover::DATA_TYPE_WEB_SQL |
                BrowsingDataRemover::DATA_TYPE_APP_CACHE |
                BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
                BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
                BrowsingDataRemover::DATA_TYPE_INDEXED_DB,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);

  // Check OriginMatcherFunction.
  EXPECT_FALSE(removal_data.origin_matcher.Run(kTestOrigin, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("http://host2.com")), mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("http://host3.com")), mock_policy()));
}

TEST_F(BrowsingDataRemoverImplTest, RemoveQuotaManagedProtectedSpecificOrigin) {
  storage::MockSpecialStoragePolicy* policy = CreateMockPolicy();
  // Protect the test origin.
  const GURL kTestUrl("http://host1.com");
  policy->AddProtected(kTestUrl);

  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST));
  builder->AddRegisterableDomain(kTestUrl.host());

  // Try to remove the test origin. Expect failure.
  BlockUntilOriginDataRemoved(
      base::Time(), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_APP_CACHE |
          BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
          BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
          BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
          BrowsingDataRemover::DATA_TYPE_INDEXED_DB |
          BrowsingDataRemover::DATA_TYPE_WEB_SQL,
      std::move(builder));

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_APP_CACHE |
                BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
                BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
                BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
                BrowsingDataRemover::DATA_TYPE_INDEXED_DB |
                BrowsingDataRemover::DATA_TYPE_WEB_SQL,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);

  // Check OriginMatcherFunction.
  EXPECT_FALSE(removal_data.origin_matcher.Run(url::Origin::Create(kTestUrl),
                                               mock_policy()));
  // Since we use the matcher function to validate origins now, this should
  // return false for the origins we're not trying to clear.
  EXPECT_FALSE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("http://host2.com")), mock_policy()));
  EXPECT_FALSE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("http://host3.com")), mock_policy()));
}

TEST_F(BrowsingDataRemoverImplTest, RemoveQuotaManagedProtectedOrigins) {
  storage::MockSpecialStoragePolicy* policy = CreateMockPolicy();
  // Protect the test origin.
  const url::Origin kTestOrigin = url::Origin::Create(GURL("http://host1.com"));
  policy->AddProtected(kTestOrigin.GetURL());

  // Try to remove the test origin. Expect success.
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_APP_CACHE |
          BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
          BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
          BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
          BrowsingDataRemover::DATA_TYPE_INDEXED_DB |
          BrowsingDataRemover::DATA_TYPE_WEB_SQL,
      true);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_APP_CACHE |
                BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
                BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
                BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
                BrowsingDataRemover::DATA_TYPE_INDEXED_DB |
                BrowsingDataRemover::DATA_TYPE_WEB_SQL,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB |
                BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);

  // Check OriginMatcherFunction, |kTestOrigin| would match mask since we
  // would have 'protected' specified in origin_type_mask.
  EXPECT_TRUE(removal_data.origin_matcher.Run(kTestOrigin, mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("http://host2.com")), mock_policy()));
  EXPECT_TRUE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("http://host3.com")), mock_policy()));
}

TEST_F(BrowsingDataRemoverImplTest,
       RemoveQuotaManagedIgnoreExtensionsAndDevTools) {
  CreateMockPolicy();

  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_APP_CACHE |
          BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
          BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
          BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
          BrowsingDataRemover::DATA_TYPE_INDEXED_DB |
          BrowsingDataRemover::DATA_TYPE_WEB_SQL,
      false);

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_APP_CACHE |
                BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS |
                BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE |
                BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS |
                BrowsingDataRemover::DATA_TYPE_INDEXED_DB |
                BrowsingDataRemover::DATA_TYPE_WEB_SQL,
            GetRemovalMask());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            GetOriginTypeMask());

  // Verify storage partition related stuffs.
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();

  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                StoragePartition::REMOVE_DATA_MASK_WEBSQL |
                StoragePartition::REMOVE_DATA_MASK_APPCACHE |
                StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS |
                StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE |
                StoragePartition::REMOVE_DATA_MASK_INDEXEDDB);
  EXPECT_EQ(removal_data.quota_storage_remove_mask,
            StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL);

  // Check that extension and devtools data wouldn't be removed, that is,
  // origin matcher would not match these origin.
  EXPECT_FALSE(removal_data.origin_matcher.Run(
      url::Origin::Create(
          GURL("chrome-extension://abcdefghijklmnopqrstuvwxyz/")),
      mock_policy()));
  EXPECT_FALSE(removal_data.origin_matcher.Run(
      url::Origin::Create(GURL("devtools://abcdefghijklmnopqrstuvw/")),
      mock_policy()));
}

class InspectableCompletionObserver
    : public BrowsingDataRemoverCompletionObserver {
 public:
  explicit InspectableCompletionObserver(BrowsingDataRemover* remover)
      : BrowsingDataRemoverCompletionObserver(remover) {}
  ~InspectableCompletionObserver() override = default;

  bool called() { return called_; }

 protected:
  void OnBrowsingDataRemoverDone() override {
    BrowsingDataRemoverCompletionObserver::OnBrowsingDataRemoverDone();
    called_ = true;
  }

 private:
  bool called_ = false;
};

TEST_F(BrowsingDataRemoverImplTest, CompletionInhibition) {
  BrowsingDataRemoverImpl* remover = static_cast<BrowsingDataRemoverImpl*>(
      BrowserContext::GetBrowsingDataRemover(GetBrowserContext()));

  // The |completion_inhibitor| on the stack should prevent removal sessions
  // from completing until after ContinueToCompletion() is called.
  BrowsingDataRemoverCompletionInhibitor completion_inhibitor(remover);
  InspectableCompletionObserver completion_observer(remover);
  remover->RemoveAndReply(
      base::Time(), base::Time::Max(), BrowsingDataRemover::DATA_TYPE_COOKIES,
      BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB, &completion_observer);

  // Process messages until the inhibitor is notified, and then some, to make
  // sure we do not complete asynchronously before ContinueToCompletion() is
  // called.
  completion_inhibitor.BlockUntilNearCompletion();
  RunAllTasksUntilIdle();

  // Verify that the removal has not yet been completed and the observer has
  // not been called.
  EXPECT_TRUE(remover->IsRemovingForTesting());
  EXPECT_FALSE(completion_observer.called());

  // Now run the removal process until completion, and verify that observers are
  // now notified, and the notifications is sent out.
  completion_inhibitor.ContinueToCompletion();
  completion_observer.BlockUntilCompletion();

  EXPECT_FALSE(remover->IsRemovingForTesting());
  EXPECT_TRUE(completion_observer.called());
}

TEST_F(BrowsingDataRemoverImplTest, EarlyShutdown) {
  BrowsingDataRemoverImpl* remover = static_cast<BrowsingDataRemoverImpl*>(
      BrowserContext::GetBrowsingDataRemover(GetBrowserContext()));
  InspectableCompletionObserver completion_observer(remover);
  BrowsingDataRemoverCompletionInhibitor completion_inhibitor(remover);
  remover->RemoveAndReply(
      base::Time(), base::Time::Max(), BrowsingDataRemover::DATA_TYPE_COOKIES,
      BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB, &completion_observer);

  completion_inhibitor.BlockUntilNearCompletion();
  completion_inhibitor.Reset();

  // Verify that the deletion has not yet been completed and the observer has
  // not been called.
  EXPECT_TRUE(remover->IsRemovingForTesting());
  EXPECT_FALSE(completion_observer.called());

  // Destroying the profile should trigger the notification.
  DestroyBrowserContext();

  EXPECT_TRUE(completion_observer.called());

  // Finishing after shutdown shouldn't break anything.
  completion_inhibitor.ContinueToCompletion();
  completion_observer.BlockUntilCompletion();
}

TEST_F(BrowsingDataRemoverImplTest, RemoveDownloadsByTimeOnly) {
  RemoveDownloadsTester tester(GetBrowserContext());
  base::RepeatingCallback<bool(const GURL&)> filter =
      BrowsingDataFilterBuilder::BuildNoopFilter();

  EXPECT_CALL(*tester.download_manager(),
              RemoveDownloadsByURLAndTime(ProbablySameFilter(filter), _, _));

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                BrowsingDataRemover::DATA_TYPE_DOWNLOADS,
                                false);
}

TEST_F(BrowsingDataRemoverImplTest, RemoveDownloadsByOrigin) {
  RemoveDownloadsTester tester(GetBrowserContext());
  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST));
  builder->AddRegisterableDomain("host1.com");
  base::RepeatingCallback<bool(const GURL&)> filter = builder->BuildUrlFilter();

  EXPECT_CALL(*tester.download_manager(),
              RemoveDownloadsByURLAndTime(ProbablySameFilter(filter), _, _));

  BlockUntilOriginDataRemoved(base::Time(), base::Time::Max(),
                              BrowsingDataRemover::DATA_TYPE_DOWNLOADS,
                              std::move(builder));
}

TEST_F(BrowsingDataRemoverImplTest, RemoveCodeCache) {
  RemoveDownloadsTester tester(GetBrowserContext());

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                BrowsingDataRemover::DATA_TYPE_CACHE, false);
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_TRUE(removal_data.remove_code_cache);
}

TEST_F(BrowsingDataRemoverImplTest, RemoveShaderCache) {
  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                BrowsingDataRemover::DATA_TYPE_CACHE, false);
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_SHADER_CACHE);
}

TEST_F(BrowsingDataRemoverImplTest, RemoveConversions) {
  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                BrowsingDataRemover::DATA_TYPE_CONVERSIONS,
                                false);
  StoragePartitionRemovalData removal_data = GetStoragePartitionRemovalData();
  EXPECT_EQ(removal_data.remove_mask,
            StoragePartition::REMOVE_DATA_MASK_CONVERSIONS);
}

class MultipleTasksObserver {
 public:
  // A simple implementation of BrowsingDataRemover::Observer.
  // MultipleTasksObserver will use several instances of Target to test
  // that completion callbacks are returned to the correct one.
  class Target : public BrowsingDataRemover::Observer {
   public:
    Target(MultipleTasksObserver* parent, BrowsingDataRemover* remover)
        : parent_(parent), observer_(this) {
      observer_.Add(remover);
    }
    ~Target() override = default;

    void OnBrowsingDataRemoverDone() override {
      parent_->last_called_targets_.push_back(this);
    }

   private:
    MultipleTasksObserver* parent_;
    ScopedObserver<BrowsingDataRemover, BrowsingDataRemover::Observer>
        observer_;
  };

  explicit MultipleTasksObserver(BrowsingDataRemover* remover)
      : target_a_(this, remover), target_b_(this, remover) {}
  ~MultipleTasksObserver() = default;

  void ClearLastCalledTarget() { last_called_targets_.clear(); }

  const std::vector<BrowsingDataRemover::Observer*> GetLastCalledTargets() {
    return last_called_targets_;
  }

  Target* target_a() { return &target_a_; }
  Target* target_b() { return &target_b_; }

 private:
  Target target_a_;
  Target target_b_;
  std::vector<BrowsingDataRemover::Observer*> last_called_targets_;
};

TEST_F(BrowsingDataRemoverImplTest, MultipleTasks) {
  BrowsingDataRemoverImpl* remover = static_cast<BrowsingDataRemoverImpl*>(
      BrowserContext::GetBrowsingDataRemover(GetBrowserContext()));
  EXPECT_FALSE(remover->IsRemovingForTesting());

  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder_1(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST));
  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder_2(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::BLACKLIST));
  filter_builder_2->AddRegisterableDomain("example.com");

  MultipleTasksObserver observer(remover);
  BrowsingDataRemoverCompletionInhibitor completion_inhibitor(remover);

  // Test several tasks with various configuration of masks, filters, and target
  // observers.
  std::list<BrowsingDataRemoverImpl::RemovalTask> tasks;
  tasks.emplace_back(
      base::Time(), base::Time::Max(), BrowsingDataRemover::DATA_TYPE_DOWNLOADS,
      BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::BLACKLIST),
      observer.target_a());
  tasks.emplace_back(
      base::Time(), base::Time::Max(), BrowsingDataRemover::DATA_TYPE_COOKIES,
      BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::BLACKLIST),
      nullptr);
  tasks.emplace_back(
      base::Time::Now(), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_DOWNLOADS,
      BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
          BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::BLACKLIST),
      observer.target_b());
  tasks.emplace_back(base::Time(), base::Time::UnixEpoch(),
                     BrowsingDataRemover::DATA_TYPE_WEB_SQL,
                     BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
                     std::move(filter_builder_1), observer.target_b());

  for (BrowsingDataRemoverImpl::RemovalTask& task : tasks) {
    // All tasks can be directly translated to a RemoveInternal() call. Since
    // that is a private method, we must call the four public versions of
    // Remove.* instead. This also serves as a test that those methods are all
    // correctly reduced to RemoveInternal().
    if (task.observers.empty() && task.filter_builder->IsEmptyBlacklist()) {
      remover->Remove(task.delete_begin, task.delete_end, task.remove_mask,
                      task.origin_type_mask);
    } else if (task.filter_builder->IsEmptyBlacklist()) {
      remover->RemoveAndReply(task.delete_begin, task.delete_end,
                              task.remove_mask, task.origin_type_mask,
                              task.observers[0]);
    } else {
      remover->RemoveWithFilterAndReply(task.delete_begin, task.delete_end,
                                        task.remove_mask, task.origin_type_mask,
                                        std::move(task.filter_builder),
                                        task.observers[0]);
    }
  }

  // Use the inhibitor to stop after every task and check the results.
  for (BrowsingDataRemoverImpl::RemovalTask& task : tasks) {
    EXPECT_TRUE(remover->IsRemovingForTesting());
    observer.ClearLastCalledTarget();

    // Finish the task execution synchronously.
    completion_inhibitor.BlockUntilNearCompletion();
    completion_inhibitor.ContinueToCompletion();

    // Observers, if any, should have been called by now (since we call
    // observers on the same thread).
    EXPECT_EQ(task.observers, observer.GetLastCalledTargets());

    // TODO(msramek): If BrowsingDataRemover took ownership of the last used
    // filter builder and exposed it, we could also test it here. Make it so.
    EXPECT_EQ(task.remove_mask, GetRemovalMask());
    EXPECT_EQ(task.origin_type_mask, GetOriginTypeMask());
    EXPECT_EQ(task.delete_begin, GetBeginTime());
  }

  EXPECT_FALSE(remover->IsRemovingForTesting());

  // Run clean up tasks.
  RunAllTasksUntilIdle();
}

// Scheduling multiple identical deletions should immediately execute the first
// deletion and merge all following deletions.
TEST_F(BrowsingDataRemoverImplTest, MultipleIdenticalTasks) {
  BrowsingDataRemoverImpl* remover = static_cast<BrowsingDataRemoverImpl*>(
      BrowserContext::GetBrowsingDataRemover(GetBrowserContext()));
  EXPECT_FALSE(remover->IsRemovingForTesting());

  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST));
  filter_builder->AddRegisterableDomain("example.com");

  MultipleTasksObserver observer(remover);
  BrowsingDataRemoverCompletionInhibitor completion_inhibitor(remover);

  std::list<BrowsingDataRemoverImpl::RemovalTask> tasks;
  for (int i = 0; i < 10; i++) {
    remover->RemoveWithFilterAndReply(
        base::Time(), base::Time::Max(), BrowsingDataRemover::DATA_TYPE_COOKIES,
        BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
        BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::BLACKLIST),
        observer.target_a());
  }

  EXPECT_TRUE(remover->IsRemovingForTesting());
  observer.ClearLastCalledTarget();

  // Finish the task execution synchronously.
  completion_inhibitor.BlockUntilNearCompletion();
  completion_inhibitor.ContinueToCompletion();

  // Expect the first observer to be called.
  EXPECT_EQ(1u, observer.GetLastCalledTargets().size());

  EXPECT_TRUE(remover->IsRemovingForTesting());
  observer.ClearLastCalledTarget();

  // Finish the task execution synchronously.
  completion_inhibitor.BlockUntilNearCompletion();
  completion_inhibitor.ContinueToCompletion();

  // Expect the remaining observer to be called by a batched deletion.
  EXPECT_EQ(9u, observer.GetLastCalledTargets().size());

  EXPECT_FALSE(remover->IsRemovingForTesting());

  // Run clean up tasks.
  RunAllTasksUntilIdle();
}

// BrowsingDataRemoverTest.MultipleTasks, tests that the tasks are not mixed up
// and they are executed in a correct order. However, the completion inhibitor
// kept synchronizing the execution in order to verify the parameters.
// This test demonstrates that even running the tasks without inhibition is
// executed correctly and doesn't crash.
TEST_F(BrowsingDataRemoverImplTest, MultipleTasksInQuickSuccession) {
  BrowsingDataRemoverImpl* remover = static_cast<BrowsingDataRemoverImpl*>(
      BrowserContext::GetBrowsingDataRemover(GetBrowserContext()));
  EXPECT_FALSE(remover->IsRemovingForTesting());

  int test_removal_masks[] = {
      BrowsingDataRemover::DATA_TYPE_COOKIES,
      BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE,
      BrowsingDataRemover::DATA_TYPE_COOKIES,
      BrowsingDataRemover::DATA_TYPE_COOKIES,
      BrowsingDataRemover::DATA_TYPE_COOKIES,
      BrowsingDataRemover::DATA_TYPE_DOWNLOADS,
      BrowsingDataRemover::DATA_TYPE_DOWNLOADS,
      BrowsingDataRemover::DATA_TYPE_DOWNLOADS,
      BrowsingDataRemover::DATA_TYPE_COOKIES |
          BrowsingDataRemover::DATA_TYPE_DOWNLOADS,
      BrowsingDataRemover::DATA_TYPE_COOKIES |
          BrowsingDataRemover::DATA_TYPE_DOWNLOADS,
      BrowsingDataRemover::DATA_TYPE_COOKIES |
          BrowsingDataRemover::DATA_TYPE_DOWNLOADS |
          BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE,
      BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE,
      BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE,
  };

  for (int removal_mask : test_removal_masks) {
    remover->Remove(base::Time(), base::Time::Max(), removal_mask,
                    BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB);
  }

  EXPECT_TRUE(remover->IsRemovingForTesting());

  // Add one more deletion and wait for it.
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(), BrowsingDataRemover::DATA_TYPE_COOKIES,
      BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB);

  EXPECT_FALSE(remover->IsRemovingForTesting());
}

namespace {
class MockNetworkContext : public network::TestNetworkContext {
 public:
  MOCK_METHOD2(
      ClearTrustTokenData,
      void(network::mojom::ClearDataFilterPtr,
           network::mojom::NetworkContext::ClearTrustTokenDataCallback));
};
}  // namespace

TEST_F(BrowsingDataRemoverImplTest, ClearsTrustTokens) {
  MockNetworkContext context;
  set_network_context_override(&context);

  EXPECT_CALL(context, ClearTrustTokenData(_, _)).WillOnce(RunOnceClosure<1>());

  BlockUntilBrowsingDataRemoved(base::Time(), base::Time::Max(),
                                BrowsingDataRemover::DATA_TYPE_TRUST_TOKENS,
                                /*include_protected_origins=*/false);
}

TEST_F(BrowsingDataRemoverImplTest, PreservesTrustTokens) {
  StrictMock<MockNetworkContext> context;
  set_network_context_override(&context);

  // When DATA_TYPE_TRUST_TOKENS isn't cleared, Trust Tokens state shouldn't be.
  BlockUntilBrowsingDataRemoved(
      base::Time(), base::Time::Max(),
      BrowsingDataRemover::DATA_TYPE_CACHE,  // arbitrary non-Trust Tokens type
      /*include_protected_origins=*/false);

  // (The strict mock will fail the test if its mocked method is called.)
}

TEST_F(BrowsingDataRemoverImplTest, ClearsTrustTokensForSite) {
  MockNetworkContext context;
  set_network_context_override(&context);

  auto expected = network::mojom::ClearDataFilter::New();
  expected->domains = {"host1.com"};

  EXPECT_CALL(
      context,
      ClearTrustTokenData(
          Truly([&expected](const network::mojom::ClearDataFilterPtr& filter) {
            return mojo::Equals(filter, expected);
          }),
          _))
      .WillOnce(RunOnceClosure<1>());

  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST));
  builder->AddRegisterableDomain("host1.com");

  BlockUntilOriginDataRemoved(base::Time(), base::Time::Max(),
                              BrowsingDataRemover::DATA_TYPE_TRUST_TOKENS,
                              std::move(builder));
}

TEST_F(BrowsingDataRemoverImplTest, ClearsTrustTokensForSiteDespiteTimeRange) {
  MockNetworkContext context;
  set_network_context_override(&context);

  auto expected = network::mojom::ClearDataFilter::New();
  expected->domains = {"host1.com"};

  EXPECT_CALL(
      context,
      ClearTrustTokenData(
          Truly([&expected](const network::mojom::ClearDataFilterPtr& filter) {
            return mojo::Equals(filter, expected);
          }),
          _))
      .WillOnce(RunOnceClosure<1>());

  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST));
  builder->AddRegisterableDomain("host1.com");

  // Since Trust Tokens data is not associated with particular timestamps, we
  // should observe the same clearing behavior with a non-default time range as
  // with the default time range.
  BlockUntilOriginDataRemoved(
      base::Time(), base::Time() + base::TimeDelta::FromSeconds(1),
      BrowsingDataRemover::DATA_TYPE_TRUST_TOKENS, std::move(builder));
}

}  // namespace content
