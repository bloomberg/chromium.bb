// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/test/mock_quota_manager.h"

#include <memory>
#include <set>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::blink::StorageKey;
using ::blink::mojom::StorageType;

namespace storage {

namespace {

constexpr StorageType kTemporary = StorageType::kTemporary;
constexpr StorageType kPersistent = StorageType::kPersistent;

constexpr QuotaClientType kClientFile = QuotaClientType::kFileSystem;
constexpr QuotaClientType kClientDB = QuotaClientType::kIndexedDatabase;

}  // namespace

class MockQuotaManagerTest : public testing::Test {
 public:
  MockQuotaManagerTest() : deletion_callback_count_(0) {}

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    policy_ = base::MakeRefCounted<MockSpecialStoragePolicy>();
    manager_ = base::MakeRefCounted<MockQuotaManager>(
        false /* is_incognito */, data_dir_.GetPath(),
        base::ThreadTaskRunnerHandle::Get().get(), policy_.get());
  }

  void TearDown() override {
    // Make sure the quota manager cleans up correctly.
    manager_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

  QuotaErrorOr<BucketInfo> GetOrCreateBucket(
      const blink::StorageKey& storage_key,
      const std::string& bucket_name) {
    QuotaErrorOr<BucketInfo> result;
    base::RunLoop run_loop;
    manager_->GetOrCreateBucket(
        storage_key, bucket_name,
        base::BindLambdaForTesting([&](QuotaErrorOr<BucketInfo> bucket) {
          result = std::move(bucket);
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  QuotaErrorOr<BucketInfo> GetBucket(const blink::StorageKey& storage_key,
                                     const std::string& bucket_name,
                                     blink::mojom::StorageType type) {
    QuotaErrorOr<BucketInfo> result;
    base::RunLoop run_loop;
    manager_->GetBucket(
        storage_key, bucket_name, type,
        base::BindLambdaForTesting([&](QuotaErrorOr<BucketInfo> bucket) {
          result = std::move(bucket);
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  void GetModifiedBuckets(StorageType type, base::Time begin, base::Time end) {
    base::RunLoop run_loop;
    manager_->GetBucketsModifiedBetween(
        type, begin, end,
        base::BindOnce(&MockQuotaManagerTest::GotModifiedBuckets,
                       weak_factory_.GetWeakPtr(), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void GotModifiedBuckets(base::OnceClosure quit_closure,
                          const std::set<BucketInfo>& buckets,
                          StorageType type) {
    buckets_ = buckets;
    type_ = type;
    std::move(quit_closure).Run();
  }

  void DeleteBucketData(const BucketInfo& bucket,
                        QuotaClientTypes quota_client_types) {
    base::RunLoop run_loop;
    manager_->DeleteBucketData(
        bucket, std::move(quota_client_types),
        base::BindOnce(&MockQuotaManagerTest::DeletedBucketData,
                       weak_factory_.GetWeakPtr(), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void DeletedBucketData(base::OnceClosure quit_closure,
                         blink::mojom::QuotaStatusCode status) {
    ++deletion_callback_count_;
    EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk, status);
    std::move(quit_closure).Run();
  }

  int deletion_callback_count() const {
    return deletion_callback_count_;
  }

  MockQuotaManager* manager() const {
    return manager_.get();
  }

  const std::set<BucketInfo>& buckets() const { return buckets_; }

  const StorageType& type() const {
    return type_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir data_dir_;
  scoped_refptr<MockQuotaManager> manager_;
  scoped_refptr<MockSpecialStoragePolicy> policy_;

  int deletion_callback_count_;

  std::set<BucketInfo> buckets_;
  StorageType type_;

  base::WeakPtrFactory<MockQuotaManagerTest> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MockQuotaManagerTest);
};

TEST_F(MockQuotaManagerTest, GetOrCreateBucket) {
  const StorageKey kStorageKey1 =
      StorageKey::CreateFromStringForTesting("http://host1:1/");
  const StorageKey kStorageKey2 =
      StorageKey::CreateFromStringForTesting("http://host2:1/");
  const char kBucketName[] = "bucket_name";

  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 0);
  EXPECT_EQ(manager()->BucketDataCount(kClientDB), 0);

  QuotaErrorOr<BucketInfo> bucket1 =
      GetOrCreateBucket(kStorageKey1, kBucketName);
  EXPECT_TRUE(bucket1.ok());
  EXPECT_EQ(bucket1->storage_key, kStorageKey1);
  EXPECT_EQ(bucket1->name, kBucketName);
  EXPECT_EQ(bucket1->type, kTemporary);
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 1);
  EXPECT_TRUE(manager()->BucketHasData(bucket1.value(), kClientFile));

  QuotaErrorOr<BucketInfo> bucket2 =
      GetOrCreateBucket(kStorageKey2, kBucketName);
  EXPECT_TRUE(bucket2.ok());
  EXPECT_EQ(bucket2->storage_key, kStorageKey2);
  EXPECT_EQ(bucket2->name, kBucketName);
  EXPECT_EQ(bucket2->type, kTemporary);
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 2);
  EXPECT_TRUE(manager()->BucketHasData(bucket2.value(), kClientFile));

  QuotaErrorOr<BucketInfo> dupe_bucket =
      GetOrCreateBucket(kStorageKey1, kBucketName);
  EXPECT_TRUE(dupe_bucket.ok());
  EXPECT_EQ(dupe_bucket.value(), bucket1.value());
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 2);

  // GetOrCreateBucket actually creates buckets associated with all quota client
  // types, so check them all.
  for (auto client_type : AllQuotaClientTypes()) {
    EXPECT_EQ(manager()->BucketDataCount(client_type), 2);
    EXPECT_TRUE(manager()->BucketHasData(bucket1.value(), client_type));
    EXPECT_TRUE(manager()->BucketHasData(bucket2.value(), client_type));
  }
}

TEST_F(MockQuotaManagerTest, GetBucket) {
  const StorageKey kStorageKey1 =
      StorageKey::CreateFromStringForTesting("http://host1:1/");
  const StorageKey kStorageKey2 =
      StorageKey::CreateFromStringForTesting("http://host2:1/");
  const char kBucketName[] = "bucket_name";

  {
    QuotaErrorOr<BucketInfo> created =
        GetOrCreateBucket(kStorageKey1, kBucketName);
    EXPECT_TRUE(created.ok());
    QuotaErrorOr<BucketInfo> fetched =
        GetBucket(kStorageKey1, kBucketName, kTemporary);
    EXPECT_TRUE(fetched.ok());
    EXPECT_EQ(fetched.value(), created.value());
    EXPECT_EQ(fetched->storage_key, kStorageKey1);
    EXPECT_EQ(fetched->name, kBucketName);
    EXPECT_EQ(fetched->type, kTemporary);
  }

  {
    QuotaErrorOr<BucketInfo> created =
        GetOrCreateBucket(kStorageKey2, kBucketName);
    EXPECT_TRUE(created.ok());
    QuotaErrorOr<BucketInfo> fetched =
        GetBucket(kStorageKey2, kBucketName, kTemporary);
    EXPECT_TRUE(fetched.ok());
    EXPECT_EQ(fetched.value(), created.value());
    EXPECT_EQ(fetched->storage_key, kStorageKey2);
    EXPECT_EQ(fetched->name, kBucketName);
    EXPECT_EQ(fetched->type, kTemporary);
  }

  QuotaErrorOr<BucketInfo> not_found =
      GetBucket(kStorageKey1, kBucketName, kPersistent);
  EXPECT_FALSE(not_found.ok());
}

TEST_F(MockQuotaManagerTest, BasicBucketManipulation) {
  const StorageKey kStorageKey1 =
      StorageKey::CreateFromStringForTesting("http://host1:1/");
  const StorageKey kStorageKey2 =
      StorageKey::CreateFromStringForTesting("http://host2:1/");

  const BucketInfo temp_bucket1 =
      manager()->CreateBucket(kStorageKey1, "temp_host1", kTemporary);
  const BucketInfo perm_bucket1 =
      manager()->CreateBucket(kStorageKey1, "perm_host1", kPersistent);
  const BucketInfo temp_bucket2 =
      manager()->CreateBucket(kStorageKey2, "temp_host2", kTemporary);
  const BucketInfo perm_bucket2 =
      manager()->CreateBucket(kStorageKey2, "perm_host2", kPersistent);

  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 0);
  EXPECT_EQ(manager()->BucketDataCount(kClientDB), 0);

  manager()->AddBucket(temp_bucket1, {kClientFile}, base::Time::Now());
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 1);
  EXPECT_EQ(manager()->BucketDataCount(kClientDB), 0);
  EXPECT_TRUE(manager()->BucketHasData(temp_bucket1, kClientFile));

  manager()->AddBucket(perm_bucket1, {kClientFile}, base::Time::Now());
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 2);
  EXPECT_EQ(manager()->BucketDataCount(kClientDB), 0);
  EXPECT_TRUE(manager()->BucketHasData(temp_bucket1, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(perm_bucket1, kClientFile));

  manager()->AddBucket(temp_bucket2, {kClientFile, kClientDB},
                       base::Time::Now());
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 3);
  EXPECT_EQ(manager()->BucketDataCount(kClientDB), 1);
  EXPECT_TRUE(manager()->BucketHasData(temp_bucket1, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(perm_bucket1, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(temp_bucket2, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(temp_bucket2, kClientDB));

  manager()->AddBucket(perm_bucket2, {kClientDB}, base::Time::Now());
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 3);
  EXPECT_EQ(manager()->BucketDataCount(kClientDB), 2);
  EXPECT_TRUE(manager()->BucketHasData(temp_bucket1, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(perm_bucket1, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(temp_bucket2, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(temp_bucket2, kClientDB));
  EXPECT_TRUE(manager()->BucketHasData(perm_bucket2, kClientDB));
}

TEST_F(MockQuotaManagerTest, BucketDeletion) {
  const BucketInfo bucket1 = manager()->CreateBucket(
      StorageKey::CreateFromStringForTesting("http://host1:1/"),
      kDefaultBucketName, kTemporary);
  const BucketInfo bucket2 = manager()->CreateBucket(
      StorageKey::CreateFromStringForTesting("http://host2:1/"),
      kDefaultBucketName, kPersistent);
  const BucketInfo bucket3 = manager()->CreateBucket(
      StorageKey::CreateFromStringForTesting("http://host3:1/"),
      kDefaultBucketName, kTemporary);

  manager()->AddBucket(bucket1, {kClientFile}, base::Time::Now());
  manager()->AddBucket(bucket2, {kClientFile, kClientDB}, base::Time::Now());
  manager()->AddBucket(bucket3, {kClientFile, kClientDB}, base::Time::Now());

  DeleteBucketData(bucket2, {kClientFile});

  EXPECT_EQ(1, deletion_callback_count());
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 2);
  EXPECT_EQ(manager()->BucketDataCount(kClientDB), 2);
  EXPECT_TRUE(manager()->BucketHasData(bucket1, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(bucket2, kClientDB));
  EXPECT_TRUE(manager()->BucketHasData(bucket3, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(bucket3, kClientDB));

  DeleteBucketData(bucket3, {kClientFile, kClientDB});

  EXPECT_EQ(2, deletion_callback_count());
  EXPECT_EQ(manager()->BucketDataCount(kClientFile), 1);
  EXPECT_EQ(manager()->BucketDataCount(kClientDB), 1);
  EXPECT_TRUE(manager()->BucketHasData(bucket1, kClientFile));
  EXPECT_TRUE(manager()->BucketHasData(bucket2, kClientDB));
}

TEST_F(MockQuotaManagerTest, ModifiedBuckets) {
  const BucketInfo bucket1 = manager()->CreateBucket(
      StorageKey::CreateFromStringForTesting("http://host1:1/"),
      kDefaultBucketName, kTemporary);
  const BucketInfo bucket2 = manager()->CreateBucket(
      StorageKey::CreateFromStringForTesting("http://host2:1/"),
      kDefaultBucketName, kTemporary);

  base::Time now = base::Time::Now();
  base::Time then = base::Time();
  base::TimeDelta an_hour = base::Milliseconds(3600000);
  base::TimeDelta a_minute = base::Milliseconds(60000);

  GetModifiedBuckets(kTemporary, then, base::Time::Max());
  EXPECT_TRUE(buckets().empty());

  manager()->AddBucket(bucket1, {kClientFile}, now - an_hour);

  GetModifiedBuckets(kTemporary, then, base::Time::Max());

  EXPECT_EQ(kTemporary, type());
  EXPECT_EQ(1UL, buckets().size());
  EXPECT_EQ(1UL, buckets().count(bucket1));
  EXPECT_EQ(0UL, buckets().count(bucket2));

  manager()->AddBucket(bucket2, {kClientFile}, now);

  GetModifiedBuckets(kTemporary, then, base::Time::Max());

  EXPECT_EQ(kTemporary, type());
  EXPECT_EQ(2UL, buckets().size());
  EXPECT_EQ(1UL, buckets().count(bucket1));
  EXPECT_EQ(1UL, buckets().count(bucket2));

  GetModifiedBuckets(kTemporary, then, now);

  EXPECT_EQ(kTemporary, type());
  EXPECT_EQ(1UL, buckets().size());
  EXPECT_EQ(1UL, buckets().count(bucket1));
  EXPECT_EQ(0UL, buckets().count(bucket2));

  GetModifiedBuckets(kTemporary, now - a_minute, now + a_minute);

  EXPECT_EQ(kTemporary, type());
  EXPECT_EQ(1UL, buckets().size());
  EXPECT_EQ(0UL, buckets().count(bucket1));
  EXPECT_EQ(1UL, buckets().count(bucket2));
}
}  // namespace storage
