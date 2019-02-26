// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/fileapi/quota/quota_backend_impl.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "storage/browser/fileapi/file_system_usage_cache.h"
#include "storage/browser/fileapi/obfuscated_file_util.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/leveldb_chrome.h"

using storage::FileSystemUsageCache;
using storage::ObfuscatedFileUtil;
using storage::QuotaBackendImpl;
using storage::SandboxFileSystemBackendDelegate;

namespace content {

namespace {

const url::Origin kOrigin(url::Origin::Create(GURL("http://example.com")));

bool DidReserveQuota(bool accepted,
                     base::File::Error* error_out,
                     int64_t* delta_out,
                     base::File::Error error,
                     int64_t delta) {
  DCHECK(error_out);
  DCHECK(delta_out);
  *error_out = error;
  *delta_out = delta;
  return accepted;
}

class MockQuotaManagerProxy : public storage::QuotaManagerProxy {
 public:
  MockQuotaManagerProxy()
      : QuotaManagerProxy(nullptr, nullptr),
        storage_modified_count_(0),
        usage_(0),
        quota_(0) {}

  // We don't mock them.
  void NotifyOriginInUse(const url::Origin& origin) override {}
  void NotifyOriginNoLongerInUse(const url::Origin& origin) override {}
  void SetUsageCacheEnabled(storage::QuotaClient::ID client_id,
                            const url::Origin& origin,
                            blink::mojom::StorageType type,
                            bool enabled) override {}

  void NotifyStorageModified(storage::QuotaClient::ID client_id,
                             const url::Origin& origin,
                             blink::mojom::StorageType type,
                             int64_t delta) override {
    ++storage_modified_count_;
    usage_ += delta;
    ASSERT_LE(usage_, quota_);
  }

  void GetUsageAndQuota(base::SequencedTaskRunner* original_task_runner,
                        const url::Origin& origin,
                        blink::mojom::StorageType type,
                        UsageAndQuotaCallback callback) override {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk, usage_, quota_);
  }

  int storage_modified_count() { return storage_modified_count_; }
  int64_t usage() { return usage_; }
  void set_usage(int64_t usage) { usage_ = usage; }
  void set_quota(int64_t quota) { quota_ = quota; }

 protected:
  ~MockQuotaManagerProxy() override = default;

 private:
  int storage_modified_count_;
  int64_t usage_;
  int64_t quota_;

  DISALLOW_COPY_AND_ASSIGN(MockQuotaManagerProxy);
};

}  // namespace

class QuotaBackendImplTest : public testing::Test {
 public:
  QuotaBackendImplTest() : quota_manager_proxy_(new MockQuotaManagerProxy) {}

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    in_memory_env_ = leveldb_chrome::NewMemEnv("quota");
    file_util_.reset(ObfuscatedFileUtil::CreateForTesting(
        nullptr, data_dir_.GetPath(), in_memory_env_.get()));
    backend_ = std::make_unique<QuotaBackendImpl>(
        file_task_runner(), file_util_.get(), &file_system_usage_cache_,
        quota_manager_proxy_.get());
  }

  void TearDown() override {
    backend_.reset();
    quota_manager_proxy_ = nullptr;
    file_util_.reset();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  void InitializeForOriginAndType(const url::Origin& origin,
                                  storage::FileSystemType type) {
    ASSERT_TRUE(
        file_util_->InitOriginDatabase(origin.GetURL(), true /* create */));
    ASSERT_TRUE(file_util_->origin_database_ != nullptr);

    std::string type_string =
        SandboxFileSystemBackendDelegate::GetTypeString(type);
    base::File::Error error = base::File::FILE_ERROR_FAILED;
    base::FilePath path = file_util_->GetDirectoryForOriginAndType(
        origin.GetURL(), type_string, true /* create */, &error);
    ASSERT_EQ(base::File::FILE_OK, error);

    ASSERT_TRUE(file_system_usage_cache_.UpdateUsage(
        GetUsageCachePath(origin, type), 0));
  }

  base::SequencedTaskRunner* file_task_runner() {
    return base::ThreadTaskRunnerHandle::Get().get();
  }

  base::FilePath GetUsageCachePath(const url::Origin& origin,
                                   storage::FileSystemType type) {
    base::FilePath path;
    base::File::Error error = backend_->GetUsageCachePath(origin, type, &path);
    EXPECT_EQ(base::File::FILE_OK, error);
    EXPECT_FALSE(path.empty());
    return path;
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  base::ScopedTempDir data_dir_;
  std::unique_ptr<leveldb::Env> in_memory_env_;
  std::unique_ptr<ObfuscatedFileUtil> file_util_;
  FileSystemUsageCache file_system_usage_cache_;
  scoped_refptr<MockQuotaManagerProxy> quota_manager_proxy_;
  std::unique_ptr<QuotaBackendImpl> backend_;

 private:
  DISALLOW_COPY_AND_ASSIGN(QuotaBackendImplTest);
};

TEST_F(QuotaBackendImplTest, ReserveQuota_Basic) {
  storage::FileSystemType type = storage::kFileSystemTypeTemporary;
  InitializeForOriginAndType(kOrigin, type);
  quota_manager_proxy_->set_quota(10000);

  int64_t delta = 0;

  const int64_t kDelta1 = 1000;
  base::File::Error error = base::File::FILE_ERROR_FAILED;
  backend_->ReserveQuota(
      kOrigin, type, kDelta1,
      base::BindOnce(&DidReserveQuota, true, &error, &delta));
  EXPECT_EQ(base::File::FILE_OK, error);
  EXPECT_EQ(kDelta1, delta);
  EXPECT_EQ(kDelta1, quota_manager_proxy_->usage());

  const int64_t kDelta2 = -300;
  error = base::File::FILE_ERROR_FAILED;
  backend_->ReserveQuota(
      kOrigin, type, kDelta2,
      base::BindOnce(&DidReserveQuota, true, &error, &delta));
  EXPECT_EQ(base::File::FILE_OK, error);
  EXPECT_EQ(kDelta2, delta);
  EXPECT_EQ(kDelta1 + kDelta2, quota_manager_proxy_->usage());

  EXPECT_EQ(2, quota_manager_proxy_->storage_modified_count());
}

TEST_F(QuotaBackendImplTest, ReserveQuota_NoSpace) {
  storage::FileSystemType type = storage::kFileSystemTypeTemporary;
  InitializeForOriginAndType(kOrigin, type);
  quota_manager_proxy_->set_quota(100);

  int64_t delta = 0;

  const int64_t kDelta = 1000;
  base::File::Error error = base::File::FILE_ERROR_FAILED;
  backend_->ReserveQuota(
      kOrigin, type, kDelta,
      base::BindOnce(&DidReserveQuota, true, &error, &delta));
  EXPECT_EQ(base::File::FILE_OK, error);
  EXPECT_EQ(100, delta);
  EXPECT_EQ(100, quota_manager_proxy_->usage());

  EXPECT_EQ(1, quota_manager_proxy_->storage_modified_count());
}

TEST_F(QuotaBackendImplTest, ReserveQuota_Revert) {
  storage::FileSystemType type = storage::kFileSystemTypeTemporary;
  InitializeForOriginAndType(kOrigin, type);
  quota_manager_proxy_->set_quota(10000);

  int64_t delta = 0;

  const int64_t kDelta = 1000;
  base::File::Error error = base::File::FILE_ERROR_FAILED;
  backend_->ReserveQuota(
      kOrigin, type, kDelta,
      base::BindOnce(&DidReserveQuota, false, &error, &delta));
  EXPECT_EQ(base::File::FILE_OK, error);
  EXPECT_EQ(kDelta, delta);
  EXPECT_EQ(0, quota_manager_proxy_->usage());

  EXPECT_EQ(2, quota_manager_proxy_->storage_modified_count());
}

TEST_F(QuotaBackendImplTest, ReleaseReservedQuota) {
  storage::FileSystemType type = storage::kFileSystemTypeTemporary;
  InitializeForOriginAndType(kOrigin, type);
  const int64_t kInitialUsage = 2000;
  quota_manager_proxy_->set_usage(kInitialUsage);
  quota_manager_proxy_->set_quota(10000);

  const int64_t kSize = 1000;
  backend_->ReleaseReservedQuota(kOrigin, type, kSize);
  EXPECT_EQ(kInitialUsage - kSize, quota_manager_proxy_->usage());

  EXPECT_EQ(1, quota_manager_proxy_->storage_modified_count());
}

TEST_F(QuotaBackendImplTest, CommitQuotaUsage) {
  storage::FileSystemType type = storage::kFileSystemTypeTemporary;
  InitializeForOriginAndType(kOrigin, type);
  quota_manager_proxy_->set_quota(10000);
  base::FilePath path = GetUsageCachePath(kOrigin, type);

  const int64_t kDelta1 = 1000;
  backend_->CommitQuotaUsage(kOrigin, type, kDelta1);
  EXPECT_EQ(kDelta1, quota_manager_proxy_->usage());
  int64_t usage = 0;
  EXPECT_TRUE(file_system_usage_cache_.GetUsage(path, &usage));
  EXPECT_EQ(kDelta1, usage);

  const int64_t kDelta2 = -300;
  backend_->CommitQuotaUsage(kOrigin, type, kDelta2);
  EXPECT_EQ(kDelta1 + kDelta2, quota_manager_proxy_->usage());
  usage = 0;
  EXPECT_TRUE(file_system_usage_cache_.GetUsage(path, &usage));
  EXPECT_EQ(kDelta1 + kDelta2, usage);

  EXPECT_EQ(2, quota_manager_proxy_->storage_modified_count());
}

TEST_F(QuotaBackendImplTest, DirtyCount) {
  storage::FileSystemType type = storage::kFileSystemTypeTemporary;
  InitializeForOriginAndType(kOrigin, type);
  base::FilePath path = GetUsageCachePath(kOrigin, type);

  backend_->IncrementDirtyCount(kOrigin, type);
  uint32_t dirty = 0;
  ASSERT_TRUE(file_system_usage_cache_.GetDirty(path, &dirty));
  EXPECT_EQ(1u, dirty);

  backend_->DecrementDirtyCount(kOrigin, type);
  ASSERT_TRUE(file_system_usage_cache_.GetDirty(path, &dirty));
  EXPECT_EQ(0u, dirty);
}

}  // namespace content
