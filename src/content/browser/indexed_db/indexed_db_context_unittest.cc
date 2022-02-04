// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/barrier_closure.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/default_clock.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/mock_indexed_db_callbacks.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_callbacks.h"
#include "content/browser/indexed_db/mock_mojo_indexed_db_database_callbacks.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/quota_manager_proxy_sync.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace content {

class IndexedDBContextTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, temp_dir_.GetPath(),
        base::ThreadTaskRunnerHandle::Get().get(),
        /*special storage policy=*/nullptr);
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(), base::ThreadTaskRunnerHandle::Get());
    indexed_db_context_ = base::MakeRefCounted<IndexedDBContextImpl>(
        temp_dir_.GetPath(), quota_manager_proxy_.get(),
        base::DefaultClock::GetInstance(),
        /*blob_storage_context=*/mojo::NullRemote(),
        /*file_system_access_context=*/mojo::NullRemote(),
        base::SequencedTaskRunnerHandle::Get(),
        base::SequencedTaskRunnerHandle::Get());
  }

 protected:
  base::ScopedTempDir temp_dir_;

  // These tests need a full TaskEnvironment because IndexedDBContextImpl
  // uses the thread pool for querying QuotaDatabase.
  base::test::TaskEnvironment task_environment_;

  scoped_refptr<IndexedDBContextImpl> indexed_db_context_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;

  const blink::StorageKey example_storage_key_ =
      blink::StorageKey::CreateFromStringForTesting("https://example.com");
  const blink::StorageKey google_storage_key_ =
      blink::StorageKey::CreateFromStringForTesting("https://google.com");
};

TEST_F(IndexedDBContextTest, DefaultBucketCreatedOnBindIndexedDB) {
  mojo::Remote<blink::mojom::IDBFactory> example_remote;
  indexed_db_context_->BindIndexedDB(
      example_storage_key_, example_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<blink::mojom::IDBFactory> google_remote;
  indexed_db_context_->BindIndexedDB(
      google_storage_key_, google_remote.BindNewPipeAndPassReceiver());

  storage::QuotaManagerProxySync quota_manager_proxy_sync(
      quota_manager_proxy_.get());

  // Call a method on both IDBFactory remotes and wait for both replies
  // to ensure that BindIndexedDB has completed for both storage keys.
  base::RunLoop loop;
  auto callbacks = base::MakeRefCounted<MockIndexedDBCallbacks>(
      /*expect_connection=*/false);
  callbacks->CallOnInfoSuccess(base::BarrierClosure(2, loop.QuitClosure()));
  indexed_db_context_->GetIDBFactory()->GetDatabaseInfo(
      callbacks, example_storage_key_, indexed_db_context_->data_path());
  indexed_db_context_->GetIDBFactory()->GetDatabaseInfo(
      callbacks, google_storage_key_, indexed_db_context_->data_path());
  loop.Run();

  // Check default bucket exists for https://example.com.
  storage::QuotaErrorOr<storage::BucketInfo> result =
      quota_manager_proxy_sync.GetBucket(example_storage_key_,
                                         storage::kDefaultBucketName,
                                         blink::mojom::StorageType::kTemporary);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->name, storage::kDefaultBucketName);
  EXPECT_EQ(result->storage_key, example_storage_key_);
  EXPECT_GT(result->id.value(), 0);

  // Check default bucket exists for https://google.com.
  result = quota_manager_proxy_sync.GetBucket(
      google_storage_key_, storage::kDefaultBucketName,
      blink::mojom::StorageType::kTemporary);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->name, storage::kDefaultBucketName);
  EXPECT_EQ(result->storage_key, google_storage_key_);
  EXPECT_GT(result->id.value(), 0);
}

TEST_F(IndexedDBContextTest, GetDefaultBucketError) {
  // Disable database so it will return errors when getting the default bucket.
  quota_manager_->SetDisableDatabase(true);

  mojo::Remote<blink::mojom::IDBFactory> example_remote;
  indexed_db_context_->BindIndexedDB(
      example_storage_key_, example_remote.BindNewPipeAndPassReceiver());

  // IDBFactory::GetDatabaseInfo
  base::RunLoop loop_1;
  auto mock_callbacks =
      std::make_unique<testing::StrictMock<MockMojoIndexedDBCallbacks>>();
  EXPECT_CALL(*mock_callbacks,
              Error(blink::mojom::IDBException::kUnknownError,
                    std::u16string(
                        u"Internal error retrieving bucket data directory.")))
      .Times(1)
      .WillOnce(base::test::RunClosure(loop_1.QuitClosure()));

  example_remote->GetDatabaseInfo(mock_callbacks->CreateInterfacePtrAndBind());
  loop_1.Run();

  testing::Mock::VerifyAndClear(&mock_callbacks);

  // IDBFactory::Open
  base::RunLoop loop_2;
  mock_callbacks =
      std::make_unique<testing::StrictMock<MockMojoIndexedDBCallbacks>>();
  auto database_callbacks =
      std::make_unique<MockMojoIndexedDBDatabaseCallbacks>();
  auto transaction_remote =
      mojo::AssociatedRemote<blink::mojom::IDBTransaction>();
  EXPECT_CALL(*mock_callbacks,
              Error(blink::mojom::IDBException::kUnknownError,
                    std::u16string(
                        u"Internal error retrieving bucket data directory.")))
      .Times(1)
      .WillOnce(base::test::RunClosure(loop_2.QuitClosure()));

  example_remote->Open(mock_callbacks->CreateInterfacePtrAndBind(),
                       database_callbacks->CreateInterfacePtrAndBind(),
                       u"database_name", /*version=*/1,
                       transaction_remote.BindNewEndpointAndPassReceiver(),
                       /*transaction_id=*/0);
  loop_2.Run();

  // IDBFactory::DeleteDatabase
  base::RunLoop loop_3;
  mock_callbacks =
      std::make_unique<testing::StrictMock<MockMojoIndexedDBCallbacks>>();
  EXPECT_CALL(*mock_callbacks,
              Error(blink::mojom::IDBException::kUnknownError,
                    std::u16string(
                        u"Internal error retrieving bucket data directory.")))
      .Times(1)
      .WillOnce(base::test::RunClosure(loop_3.QuitClosure()));

  example_remote->DeleteDatabase(mock_callbacks->CreateInterfacePtrAndBind(),
                                 u"database_name", /*force_close=*/true);
  loop_3.Run();

  // IDBFactory::AbortTransactionsAndCompactDatabase
  base::RunLoop loop_4;
  example_remote->AbortTransactionsAndCompactDatabase(
      base::BindLambdaForTesting([&](blink::mojom::IDBStatus status) {
        EXPECT_EQ(status, blink::mojom::IDBStatus::NotFound);
        loop_4.Quit();
      }));
  loop_4.Run();

  // IDBFactory::AbortTransactionsForDatabase
  base::RunLoop loop_5;
  example_remote->AbortTransactionsForDatabase(
      base::BindLambdaForTesting([&](blink::mojom::IDBStatus status) {
        EXPECT_EQ(status, blink::mojom::IDBStatus::NotFound);
        loop_5.Quit();
      }));
  loop_5.Run();
}

}  // namespace content
