// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/barrier_closure.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "components/services/storage/indexed_db/locks/leveled_lock_manager.h"
#include "components/services/storage/indexed_db/transactional_leveldb/transactional_leveldb_database.h"
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom-test-utils.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/indexed_db/indexed_db_bucket_state.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_leveldb_env.h"
#include "content/browser/indexed_db/mock_indexed_db_callbacks.h"
#include "content/browser/indexed_db/mock_indexed_db_database_callbacks.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

using blink::IndexedDBDatabaseMetadata;

namespace content {
namespace {

base::FilePath CreateAndReturnTempDir(base::ScopedTempDir* temp_dir) {
  CHECK(temp_dir->CreateUniqueTempDir());
  return temp_dir->GetPath();
}

void CreateAndBindTransactionPlaceholder(
    base::WeakPtr<IndexedDBTransaction> transaction) {}

class LevelDBLock {
 public:
  LevelDBLock() = default;
  LevelDBLock(leveldb::Env* env, leveldb::FileLock* lock)
      : env_(env), lock_(lock) {}

  LevelDBLock(const LevelDBLock&) = delete;
  LevelDBLock& operator=(const LevelDBLock&) = delete;

  ~LevelDBLock() {
    if (env_)
      env_->UnlockFile(lock_);
  }

 private:
  raw_ptr<leveldb::Env> env_ = nullptr;
  raw_ptr<leveldb::FileLock> lock_ = nullptr;
};

std::unique_ptr<LevelDBLock> LockForTesting(const base::FilePath& file_name) {
  leveldb::Env* env = IndexedDBLevelDBEnv::Get();
  base::FilePath lock_path = file_name.AppendASCII("LOCK");
  leveldb::FileLock* lock = nullptr;
  leveldb::Status status = env->LockFile(lock_path.AsUTF8Unsafe(), &lock);
  if (!status.ok())
    return nullptr;
  DCHECK(lock);
  return std::make_unique<LevelDBLock>(env, lock);
}

}  // namespace

class IndexedDBTest : public testing::Test,
                      public testing::WithParamInterface<bool> {
 public:
  blink::StorageKey kNormalFirstPartyStorageKey;
  storage::BucketLocator kNormalFirstPartyBucketLocator;
  blink::StorageKey kSessionOnlyFirstPartyStorageKey;
  storage::BucketLocator kSessionOnlyFirstPartyBucketLocator;
  blink::StorageKey kNormalThirdPartyStorageKey;
  storage::BucketLocator kNormalThirdPartyBucketLocator;
  blink::StorageKey kSessionOnlyThirdPartyStorageKey;
  storage::BucketLocator kSessionOnlyThirdPartyBucketLocator;
  blink::StorageKey kInvertedNormalThirdPartyStorageKey;
  storage::BucketLocator kInvertedNormalThirdPartyBucketLocator;
  blink::StorageKey kInvertedSessionOnlyThirdPartyStorageKey;
  storage::BucketLocator kInvertedSessionOnlyThirdPartyBucketLocator;

  IndexedDBTest()
      : quota_manager_proxy_(
            base::MakeRefCounted<storage::MockQuotaManagerProxy>(
                nullptr,
                base::SequencedTaskRunnerHandle::Get())),
        context_(base::MakeRefCounted<IndexedDBContextImpl>(
            CreateAndReturnTempDir(&temp_dir_),
            quota_manager_proxy_.get(),
            base::DefaultClock::GetInstance(),
            /*blob_storage_context=*/mojo::NullRemote(),
            /*file_system_access_context=*/mojo::NullRemote(),
            base::SequencedTaskRunnerHandle::Get(),
            base::SequencedTaskRunnerHandle::Get())) {
    scoped_feature_list_.InitWithFeatureState(
        blink::features::kThirdPartyStoragePartitioning,
        IsThirdPartyStoragePartitioningEnabled());
    kNormalFirstPartyStorageKey =
        blink::StorageKey::CreateFromStringForTesting("http://normal/");
    kNormalFirstPartyBucketLocator = storage::BucketLocator();
    kNormalFirstPartyBucketLocator.id = storage::BucketId::FromUnsafeValue(1);
    kNormalFirstPartyBucketLocator.storage_key = kNormalFirstPartyStorageKey;
    context()->RegisterBucketLocatorToSkipQuotaLookupForTesting(
        kNormalFirstPartyBucketLocator);
    kSessionOnlyFirstPartyStorageKey =
        blink::StorageKey::CreateFromStringForTesting("http://session-only/");
    kSessionOnlyFirstPartyBucketLocator = storage::BucketLocator();
    kSessionOnlyFirstPartyBucketLocator.id =
        storage::BucketId::FromUnsafeValue(2);
    kSessionOnlyFirstPartyBucketLocator.storage_key =
        kSessionOnlyFirstPartyStorageKey;
    context()->RegisterBucketLocatorToSkipQuotaLookupForTesting(
        kSessionOnlyFirstPartyBucketLocator);
    kNormalThirdPartyStorageKey =
        blink::StorageKey(url::Origin::Create(GURL("http://normal/")),
                          url::Origin::Create(GURL("http://rando/")));
    kNormalThirdPartyBucketLocator = storage::BucketLocator();
    kNormalThirdPartyBucketLocator.id = storage::BucketId::FromUnsafeValue(3);
    kNormalThirdPartyBucketLocator.storage_key = kNormalThirdPartyStorageKey;
    context()->RegisterBucketLocatorToSkipQuotaLookupForTesting(
        kNormalThirdPartyBucketLocator);
    kSessionOnlyThirdPartyStorageKey =
        blink::StorageKey(url::Origin::Create(GURL("http://session-only/")),
                          url::Origin::Create(GURL("http://rando/")));
    kSessionOnlyThirdPartyBucketLocator = storage::BucketLocator();
    kSessionOnlyThirdPartyBucketLocator.id =
        storage::BucketId::FromUnsafeValue(4);
    kSessionOnlyThirdPartyBucketLocator.storage_key =
        kSessionOnlyThirdPartyStorageKey;
    context()->RegisterBucketLocatorToSkipQuotaLookupForTesting(
        kSessionOnlyThirdPartyBucketLocator);
    kInvertedNormalThirdPartyStorageKey =
        blink::StorageKey(url::Origin::Create(GURL("http://rando/")),
                          url::Origin::Create(GURL("http://normal/")));
    kInvertedNormalThirdPartyBucketLocator = storage::BucketLocator();
    kInvertedNormalThirdPartyBucketLocator.id =
        storage::BucketId::FromUnsafeValue(3);
    kInvertedNormalThirdPartyBucketLocator.storage_key =
        kInvertedNormalThirdPartyStorageKey;
    context()->RegisterBucketLocatorToSkipQuotaLookupForTesting(
        kInvertedNormalThirdPartyBucketLocator);
    kInvertedSessionOnlyThirdPartyStorageKey =
        blink::StorageKey(url::Origin::Create(GURL("http://rando/")),
                          url::Origin::Create(GURL("http://session-only/")));
    kInvertedSessionOnlyThirdPartyBucketLocator = storage::BucketLocator();
    kInvertedSessionOnlyThirdPartyBucketLocator.id =
        storage::BucketId::FromUnsafeValue(4);
    kInvertedSessionOnlyThirdPartyBucketLocator.storage_key =
        kInvertedSessionOnlyThirdPartyStorageKey;
    context()->RegisterBucketLocatorToSkipQuotaLookupForTesting(
        kInvertedSessionOnlyThirdPartyBucketLocator);
    std::vector<storage::mojom::StoragePolicyUpdatePtr> policy_updates;
    policy_updates.emplace_back(storage::mojom::StoragePolicyUpdate::New(
        kSessionOnlyFirstPartyStorageKey.origin(),
        /*should_purge_on_shutdown=*/true));
    context_->ApplyPolicyUpdates(std::move(policy_updates));
  }

  IndexedDBTest(const IndexedDBTest&) = delete;
  IndexedDBTest& operator=(const IndexedDBTest&) = delete;

  ~IndexedDBTest() override = default;

  void RunPostedTasks() {
    base::RunLoop loop;
    context_->IDBTaskRunner()->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }

  void TearDown() override {
    if (context_ && !context_->IsInMemoryContext()) {
      IndexedDBFactoryImpl* factory = context_->GetIDBFactory();

      // Loop through all open buckets, and force close them, and request
      // the deletion of the leveldb state. Once the states are no longer
      // around, delete all of the databases on disk.
      auto open_factory_buckets = factory->GetOpenBuckets();
      for (const auto& bucket_locator : open_factory_buckets) {
        context_->ForceClose(
            bucket_locator,
            storage::mojom::ForceCloseReason::FORCE_CLOSE_DELETE_ORIGIN,
            base::DoNothing());
      }
      // All leveldb databases are closed, and they can be deleted.
      for (auto bucket_locator : context_->GetAllBuckets()) {
        bool success = false;
        storage::mojom::IndexedDBControlAsyncWaiter waiter(context_.get());
        waiter.DeleteForBucket(bucket_locator.storage_key, &success);
        EXPECT_TRUE(success);
      }
    }

    if (temp_dir_.IsValid())
      ASSERT_TRUE(temp_dir_.Delete());
  }

  base::FilePath GetFilePathForTesting(
      const storage::BucketLocator& bucket_locator) {
    base::FilePath path;
    base::RunLoop run_loop;
    context()->GetFilePathForTesting(
        bucket_locator,
        base::BindLambdaForTesting([&](const base::FilePath& async_path) {
          path = async_path;
          run_loop.Quit();
        }));
    run_loop.Run();
    return path;
  }

  bool IsThirdPartyStoragePartitioningEnabled() { return GetParam(); }

 protected:
  IndexedDBContextImpl* context() const { return context_.get(); }
  base::test::ScopedFeatureList scoped_feature_list_;

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<IndexedDBContextImpl> context_;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IndexedDBTest,
    testing::Bool());

TEST_P(IndexedDBTest, ClearSessionOnlyDatabases) {
  base::FilePath normal_path_first_party;
  base::FilePath session_only_path_first_party;
  base::FilePath normal_path_third_party;
  base::FilePath session_only_path_third_party;
  base::FilePath inverted_normal_path_third_party;
  base::FilePath inverted_session_only_path_third_party;

  normal_path_first_party =
      GetFilePathForTesting(kNormalFirstPartyBucketLocator);
  session_only_path_first_party =
      GetFilePathForTesting(kSessionOnlyFirstPartyBucketLocator);
  normal_path_third_party =
      GetFilePathForTesting(kNormalThirdPartyBucketLocator);
  session_only_path_third_party =
      GetFilePathForTesting(kSessionOnlyThirdPartyBucketLocator);
  inverted_normal_path_third_party =
      GetFilePathForTesting(kInvertedNormalThirdPartyBucketLocator);
  inverted_session_only_path_third_party =
      GetFilePathForTesting(kInvertedSessionOnlyThirdPartyBucketLocator);
  if (IsThirdPartyStoragePartitioningEnabled()) {
    EXPECT_NE(normal_path_first_party, normal_path_third_party);
    EXPECT_NE(session_only_path_first_party, session_only_path_third_party);
    EXPECT_NE(inverted_normal_path_third_party,
              inverted_session_only_path_third_party);
  } else {
    EXPECT_EQ(normal_path_first_party, normal_path_third_party);
    EXPECT_EQ(session_only_path_first_party, session_only_path_third_party);
    EXPECT_EQ(inverted_normal_path_third_party,
              inverted_session_only_path_third_party);
  }

  ASSERT_TRUE(base::CreateDirectory(normal_path_first_party));
  ASSERT_TRUE(base::CreateDirectory(session_only_path_first_party));
  ASSERT_TRUE(base::CreateDirectory(normal_path_third_party));
  ASSERT_TRUE(base::CreateDirectory(session_only_path_third_party));
  ASSERT_TRUE(base::CreateDirectory(inverted_normal_path_third_party));
  ASSERT_TRUE(base::CreateDirectory(inverted_session_only_path_third_party));

  context()->ForceInitializeFromFilesForTesting(base::DoNothing());
  base::RunLoop().RunUntilIdle();

  context()->Shutdown();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(base::DirectoryExists(normal_path_first_party));
  EXPECT_FALSE(base::DirectoryExists(session_only_path_first_party));
  EXPECT_TRUE(base::DirectoryExists(normal_path_third_party));
  EXPECT_FALSE(base::DirectoryExists(session_only_path_third_party));
  EXPECT_TRUE(base::DirectoryExists(inverted_normal_path_third_party));
  if (IsThirdPartyStoragePartitioningEnabled()) {
    EXPECT_FALSE(base::DirectoryExists(inverted_session_only_path_third_party));
  } else {
    EXPECT_TRUE(base::DirectoryExists(inverted_session_only_path_third_party));
  }
}

TEST_P(IndexedDBTest, SetForceKeepSessionState) {
  base::FilePath normal_path_first_party;
  base::FilePath session_only_path_first_party;
  base::FilePath normal_path_third_party;
  base::FilePath session_only_path_third_party;

  // Save session state. This should bypass the destruction-time deletion.
  context()->SetForceKeepSessionState();
  base::RunLoop().RunUntilIdle();

  normal_path_first_party =
      GetFilePathForTesting(kNormalFirstPartyBucketLocator);
  session_only_path_first_party =
      GetFilePathForTesting(kSessionOnlyFirstPartyBucketLocator);
  normal_path_third_party =
      GetFilePathForTesting(kNormalThirdPartyBucketLocator);
  session_only_path_third_party =
      GetFilePathForTesting(kSessionOnlyThirdPartyBucketLocator);
  if (IsThirdPartyStoragePartitioningEnabled()) {
    EXPECT_NE(normal_path_first_party, normal_path_third_party);
    EXPECT_NE(session_only_path_first_party, session_only_path_third_party);
  } else {
    EXPECT_EQ(normal_path_first_party, normal_path_third_party);
    EXPECT_EQ(session_only_path_first_party, session_only_path_third_party);
  }

  ASSERT_TRUE(base::CreateDirectory(normal_path_first_party));
  ASSERT_TRUE(base::CreateDirectory(session_only_path_first_party));
  ASSERT_TRUE(base::CreateDirectory(normal_path_third_party));
  ASSERT_TRUE(base::CreateDirectory(session_only_path_third_party));

  context()->ForceInitializeFromFilesForTesting(base::DoNothing());
  base::RunLoop().RunUntilIdle();

  context()->Shutdown();
  base::RunLoop().RunUntilIdle();

  // No data was cleared because of SetForceKeepSessionState.
  EXPECT_TRUE(base::DirectoryExists(normal_path_first_party));
  EXPECT_TRUE(base::DirectoryExists(session_only_path_first_party));
  EXPECT_TRUE(base::DirectoryExists(normal_path_third_party));
  EXPECT_TRUE(base::DirectoryExists(session_only_path_third_party));
}

class ForceCloseDBCallbacks : public IndexedDBCallbacks {
 public:
  ForceCloseDBCallbacks(scoped_refptr<IndexedDBContextImpl> idb_context,
                        const storage::BucketLocator& bucket_locator)
      : IndexedDBCallbacks(nullptr,
                           bucket_locator,
                           mojo::NullAssociatedRemote(),
                           idb_context->IDBTaskRunner()),
        idb_context_(idb_context),
        bucket_locator_(bucket_locator) {}

  ForceCloseDBCallbacks(const ForceCloseDBCallbacks&) = delete;
  ForceCloseDBCallbacks& operator=(const ForceCloseDBCallbacks&) = delete;

  void OnSuccess() override {}
  void OnSuccess(std::unique_ptr<IndexedDBConnection> connection,
                 const IndexedDBDatabaseMetadata& metadata) override {
    connection_ = std::move(connection);
    idb_context_->ConnectionOpened(bucket_locator_, connection_.get());
  }

  IndexedDBConnection* connection() { return connection_.get(); }

 protected:
  ~ForceCloseDBCallbacks() override = default;

 private:
  scoped_refptr<IndexedDBContextImpl> idb_context_;
  storage::BucketLocator bucket_locator_;
  std::unique_ptr<IndexedDBConnection> connection_;
};

TEST_P(IndexedDBTest, ForceCloseOpenDatabasesOnDeleteFirstParty) {
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("http://test/");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.id = storage::BucketId::FromUnsafeValue(5);
  bucket_locator.storage_key = kTestStorageKey;

  auto open_db_callbacks =
      base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  auto closed_db_callbacks =
      base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  auto open_callbacks =
      base::MakeRefCounted<ForceCloseDBCallbacks>(context(), bucket_locator);
  auto closed_callbacks =
      base::MakeRefCounted<ForceCloseDBCallbacks>(context(), bucket_locator);
  base::FilePath test_path = GetFilePathForTesting(bucket_locator);

  const int64_t host_transaction_id = 0;
  const int64_t version = 0;

  IndexedDBFactory* factory = context()->GetIDBFactory();

  auto create_transaction_callback1 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  factory->Open(u"opendb",
                std::make_unique<IndexedDBPendingConnection>(
                    open_callbacks, open_db_callbacks, host_transaction_id,
                    version, std::move(create_transaction_callback1)),
                bucket_locator, context()->GetDataPath(bucket_locator));
  EXPECT_TRUE(base::DirectoryExists(test_path));

  auto create_transaction_callback2 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  factory->Open(u"closeddb",
                std::make_unique<IndexedDBPendingConnection>(
                    closed_callbacks, closed_db_callbacks, host_transaction_id,
                    version, std::move(create_transaction_callback2)),
                bucket_locator, context()->GetDataPath(bucket_locator));
  RunPostedTasks();
  ASSERT_TRUE(closed_callbacks->connection());
  closed_callbacks->connection()->AbortTransactionsAndClose(
      IndexedDBConnection::CloseErrorHandling::kAbortAllReturnLastError);
  RunPostedTasks();

  context()->ForceClose(
      bucket_locator,
      storage::mojom::ForceCloseReason::FORCE_CLOSE_DELETE_ORIGIN,
      base::DoNothing());
  EXPECT_TRUE(open_db_callbacks->forced_close_called());
  EXPECT_FALSE(closed_db_callbacks->forced_close_called());

  RunPostedTasks();

  bool success = false;
  storage::mojom::IndexedDBControlAsyncWaiter waiter(context());
  waiter.DeleteForBucket(kTestStorageKey, &success);
  EXPECT_TRUE(success);

  EXPECT_FALSE(base::DirectoryExists(test_path));
}

TEST_P(IndexedDBTest, ForceCloseOpenDatabasesOnDeleteThirdParty) {
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey(url::Origin::Create(GURL("http://test/")),
                        url::Origin::Create(GURL("http://rando/")));
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.id = storage::BucketId::FromUnsafeValue(5);
  bucket_locator.storage_key = kTestStorageKey;

  auto open_db_callbacks =
      base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  auto closed_db_callbacks =
      base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  auto open_callbacks =
      base::MakeRefCounted<ForceCloseDBCallbacks>(context(), bucket_locator);
  auto closed_callbacks =
      base::MakeRefCounted<ForceCloseDBCallbacks>(context(), bucket_locator);
  base::FilePath test_path = GetFilePathForTesting(bucket_locator);

  const int64_t host_transaction_id = 0;
  const int64_t version = 0;

  IndexedDBFactory* factory = context()->GetIDBFactory();

  auto create_transaction_callback1 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  factory->Open(u"opendb",
                std::make_unique<IndexedDBPendingConnection>(
                    open_callbacks, open_db_callbacks, host_transaction_id,
                    version, std::move(create_transaction_callback1)),
                bucket_locator, context()->GetDataPath(bucket_locator));
  EXPECT_TRUE(base::DirectoryExists(test_path));

  auto create_transaction_callback2 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  factory->Open(u"closeddb",
                std::make_unique<IndexedDBPendingConnection>(
                    closed_callbacks, closed_db_callbacks, host_transaction_id,
                    version, std::move(create_transaction_callback2)),
                bucket_locator, context()->GetDataPath(bucket_locator));
  RunPostedTasks();
  ASSERT_TRUE(closed_callbacks->connection());
  closed_callbacks->connection()->AbortTransactionsAndClose(
      IndexedDBConnection::CloseErrorHandling::kAbortAllReturnLastError);
  RunPostedTasks();

  context()->ForceClose(
      bucket_locator,
      storage::mojom::ForceCloseReason::FORCE_CLOSE_DELETE_ORIGIN,
      base::DoNothing());
  EXPECT_TRUE(open_db_callbacks->forced_close_called());
  EXPECT_FALSE(closed_db_callbacks->forced_close_called());

  RunPostedTasks();

  bool success = false;
  storage::mojom::IndexedDBControlAsyncWaiter waiter(context());
  waiter.DeleteForBucket(kTestStorageKey, &success);
  EXPECT_TRUE(success);

  EXPECT_FALSE(base::DirectoryExists(test_path));
}

TEST_P(IndexedDBTest, DeleteFailsIfDirectoryLockedFirstParty) {
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("http://test/");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.id = storage::BucketId::FromUnsafeValue(5);
  bucket_locator.storage_key = kTestStorageKey;
  context()->RegisterBucketLocatorToSkipQuotaLookupForTesting(bucket_locator);

  base::FilePath test_path = GetFilePathForTesting(bucket_locator);
  ASSERT_TRUE(base::CreateDirectory(test_path));

  auto lock = LockForTesting(test_path);
  ASSERT_TRUE(lock);

  bool success = false;
  base::RunLoop loop;
  context()->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        storage::mojom::IndexedDBControlAsyncWaiter waiter(context());
        waiter.DeleteForBucket(kTestStorageKey, &success);
        loop.Quit();
      }));
  loop.Run();
  EXPECT_FALSE(success);

  EXPECT_TRUE(base::DirectoryExists(test_path));
}

TEST_P(IndexedDBTest, DeleteFailsIfDirectoryLockedThirdParty) {
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey(url::Origin::Create(GURL("http://test/")),
                        url::Origin::Create(GURL("http://rando/")));
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.id = storage::BucketId::FromUnsafeValue(5);
  bucket_locator.storage_key = kTestStorageKey;
  context()->RegisterBucketLocatorToSkipQuotaLookupForTesting(bucket_locator);

  base::FilePath test_path = GetFilePathForTesting(bucket_locator);
  ASSERT_TRUE(base::CreateDirectory(test_path));

  auto lock = LockForTesting(test_path);
  ASSERT_TRUE(lock);

  bool success = false;
  base::RunLoop loop;
  context()->IDBTaskRunner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        storage::mojom::IndexedDBControlAsyncWaiter waiter(context());
        waiter.DeleteForBucket(kTestStorageKey, &success);
        loop.Quit();
      }));
  loop.Run();
  EXPECT_FALSE(success);

  EXPECT_TRUE(base::DirectoryExists(test_path));
}

TEST_P(IndexedDBTest, ForceCloseOpenDatabasesOnCommitFailureFirstParty) {
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("http://test/");
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.id = storage::BucketId::FromUnsafeValue(5);
  bucket_locator.storage_key = kTestStorageKey;

  auto* factory =
      static_cast<IndexedDBFactoryImpl*>(context()->GetIDBFactory());

  const int64_t transaction_id = 1;

  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>();
  auto db_callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  auto create_transaction_callback1 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection = std::make_unique<IndexedDBPendingConnection>(
      callbacks, db_callbacks, transaction_id,
      IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      std::move(create_transaction_callback1));
  factory->Open(u"db", std::move(connection), bucket_locator,
                context()->GetDataPath(bucket_locator));
  RunPostedTasks();

  ASSERT_TRUE(callbacks->connection());

  // ConnectionOpened() is usually called by the dispatcher.
  context()->ConnectionOpened(bucket_locator, callbacks->connection());

  EXPECT_TRUE(factory->IsBackingStoreOpen(bucket_locator));

  // Simulate the write failure.
  leveldb::Status status = leveldb::Status::IOError("Simulated failure");
  factory->HandleBackingStoreFailure(bucket_locator);

  EXPECT_TRUE(db_callbacks->forced_close_called());
  EXPECT_FALSE(factory->IsBackingStoreOpen(bucket_locator));
}

TEST_P(IndexedDBTest, ForceCloseOpenDatabasesOnCommitFailureThirdParty) {
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey(url::Origin::Create(GURL("http://test/")),
                        url::Origin::Create(GURL("http://rando/")));
  auto bucket_locator = storage::BucketLocator();
  bucket_locator.id = storage::BucketId::FromUnsafeValue(5);
  bucket_locator.storage_key = kTestStorageKey;

  auto* factory =
      static_cast<IndexedDBFactoryImpl*>(context()->GetIDBFactory());

  const int64_t transaction_id = 1;

  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>();
  auto db_callbacks = base::MakeRefCounted<MockIndexedDBDatabaseCallbacks>();
  auto create_transaction_callback1 =
      base::BindOnce(&CreateAndBindTransactionPlaceholder);
  auto connection = std::make_unique<IndexedDBPendingConnection>(
      callbacks, db_callbacks,
      transaction_id, IndexedDBDatabaseMetadata::DEFAULT_VERSION,
      std::move(create_transaction_callback1));
  factory->Open(u"db", std::move(connection), bucket_locator,
                context()->GetDataPath(bucket_locator));
  RunPostedTasks();

  ASSERT_TRUE(callbacks->connection());

  // ConnectionOpened() is usually called by the dispatcher.
  context()->ConnectionOpened(bucket_locator, callbacks->connection());

  EXPECT_TRUE(factory->IsBackingStoreOpen(bucket_locator));

  // Simulate the write failure.
  leveldb::Status status = leveldb::Status::IOError("Simulated failure");
  factory->HandleBackingStoreFailure(bucket_locator);

  EXPECT_TRUE(db_callbacks->forced_close_called());
  EXPECT_FALSE(factory->IsBackingStoreOpen(bucket_locator));
}

TEST(LeveledLockManager, TestRangeDifferences) {
  LeveledLockRange range_db1;
  LeveledLockRange range_db2;
  LeveledLockRange range_db1_os1;
  LeveledLockRange range_db1_os2;
  for (int64_t i = 0; i < 512; ++i) {
    range_db1 = GetDatabaseLockRange(i);
    range_db2 = GetDatabaseLockRange(i + 1);
    range_db1_os1 = GetObjectStoreLockRange(i, i);
    range_db1_os2 = GetObjectStoreLockRange(i, i + 1);
    EXPECT_TRUE(range_db1.IsValid() && range_db2.IsValid() &&
                range_db1_os1.IsValid() && range_db1_os2.IsValid());
    EXPECT_LT(range_db1, range_db2);
    EXPECT_LT(range_db1, range_db1_os1);
    EXPECT_LT(range_db1, range_db1_os2);
    EXPECT_LT(range_db1_os1, range_db1_os2);
    EXPECT_LT(range_db1_os1, range_db2);
    EXPECT_LT(range_db1_os2, range_db2);
  }
}

}  // namespace content
