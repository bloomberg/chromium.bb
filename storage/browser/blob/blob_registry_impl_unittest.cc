// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_registry_impl.h"

#include <limits>
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/task_scheduler/post_task.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/test/mock_bytes_provider.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

namespace {

const size_t kTestBlobStorageIPCThresholdBytes = 5;
const size_t kTestBlobStorageMaxSharedMemoryBytes = 20;
const size_t kTestBlobStorageMaxBytesDataItemSize = 13;

const size_t kTestBlobStorageMaxBlobMemorySize = 400;
const uint64_t kTestBlobStorageMaxDiskSpace = 4000;
const uint64_t kTestBlobStorageMinFileSizeBytes = 10;
const uint64_t kTestBlobStorageMaxFileSizeBytes = 100;

class MockBlob : public mojom::Blob {
 public:
  explicit MockBlob(const std::string& uuid) : uuid_(uuid) {}

  void Clone(mojom::BlobRequest request) override {
    mojo::MakeStrongBinding(base::MakeUnique<MockBlob>(uuid_),
                            std::move(request));
  }

  void ReadRange(uint64_t offset,
                 uint64_t size,
                 mojo::ScopedDataPipeProducerHandle,
                 mojom::BlobReaderClientPtr) override {
    NOTREACHED();
  }

  void ReadAll(mojo::ScopedDataPipeProducerHandle,
               mojom::BlobReaderClientPtr) override {
    NOTREACHED();
  }

  void GetInternalUUID(GetInternalUUIDCallback callback) override {
    std::move(callback).Run(uuid_);
  }

 private:
  std::string uuid_;
};

class MockDelegate : public BlobRegistryImpl::Delegate {
 public:
  MockDelegate() {}
  ~MockDelegate() override {}

  bool CanReadFile(const base::FilePath& file) override {
    return can_read_file_result;
  }
  bool CanReadFileSystemFile(const FileSystemURL& url) override {
    return can_read_file_system_file_result;
  }
  bool CanCommitURL(const GURL& url) override { return can_commit_url_result; }

  bool can_read_file_result = true;
  bool can_read_file_system_file_result = true;
  bool can_commit_url_result = true;
};

void BindBytesProvider(std::unique_ptr<MockBytesProvider> impl,
                       mojom::BytesProviderRequest request) {
  mojo::MakeStrongBinding(std::move(impl), std::move(request));
}

}  // namespace

class BlobRegistryImplTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    context_ = base::MakeUnique<BlobStorageContext>(
        data_dir_.GetPath(),
        base::CreateTaskRunnerWithTraits({base::MayBlock()}));
    auto storage_policy =
        base::MakeRefCounted<content::MockSpecialStoragePolicy>();
    file_system_context_ = base::MakeRefCounted<storage::FileSystemContext>(
        base::ThreadTaskRunnerHandle::Get().get(),
        base::ThreadTaskRunnerHandle::Get().get(),
        nullptr /* external_mount_points */, storage_policy.get(),
        nullptr /* quota_manager_proxy */,
        std::vector<std::unique_ptr<FileSystemBackend>>(),
        std::vector<URLRequestAutoMountHandler>(), data_dir_.GetPath(),
        FileSystemOptions(FileSystemOptions::PROFILE_MODE_INCOGNITO,
                          std::vector<std::string>(), nullptr));
    registry_impl_ = base::MakeUnique<BlobRegistryImpl>(context_.get(),
                                                        file_system_context_);
    auto delegate = base::MakeUnique<MockDelegate>();
    delegate_ptr_ = delegate.get();
    registry_impl_->Bind(MakeRequest(&registry_), std::move(delegate));

    mojo::edk::SetDefaultProcessErrorCallback(base::Bind(
        &BlobRegistryImplTest::OnBadMessage, base::Unretained(this)));

    storage::BlobStorageLimits limits;
    limits.max_ipc_memory_size = kTestBlobStorageIPCThresholdBytes;
    limits.max_shared_memory_size = kTestBlobStorageMaxSharedMemoryBytes;
    limits.max_bytes_data_item_size = kTestBlobStorageMaxBytesDataItemSize;
    limits.max_blob_in_memory_space = kTestBlobStorageMaxBlobMemorySize;
    limits.desired_max_disk_space = kTestBlobStorageMaxDiskSpace;
    limits.effective_max_disk_space = kTestBlobStorageMaxDiskSpace;
    limits.min_page_file_size = kTestBlobStorageMinFileSizeBytes;
    limits.max_file_size = kTestBlobStorageMaxFileSizeBytes;
    context_->mutable_memory_controller()->set_limits_for_testing(limits);

    // Disallow IO on the main loop.
    base::ThreadRestrictions::SetIOAllowed(false);
  }

  void TearDown() override {
    base::ThreadRestrictions::SetIOAllowed(true);

    mojo::edk::SetDefaultProcessErrorCallback(
        mojo::edk::ProcessErrorCallback());
  }

  std::unique_ptr<BlobDataHandle> CreateBlobFromString(
      const std::string& uuid,
      const std::string& contents) {
    BlobDataBuilder builder(uuid);
    builder.set_content_type("text/plain");
    builder.AppendData(contents);
    return context_->AddFinishedBlob(builder);
  }

  std::string UUIDFromBlob(mojom::Blob* blob) {
    base::RunLoop loop;
    std::string received_uuid;
    blob->GetInternalUUID(base::Bind(
        [](base::Closure quit_closure, std::string* uuid_out,
           const std::string& uuid) {
          *uuid_out = uuid;
          quit_closure.Run();
        },
        loop.QuitClosure(), &received_uuid));
    loop.Run();
    return received_uuid;
  }

  void OnBadMessage(const std::string& error) {
    bad_messages_.push_back(error);
  }

  void WaitForBlobCompletion(BlobDataHandle* blob_handle) {
    base::RunLoop loop;
    blob_handle->RunOnConstructionComplete(base::Bind(
        [](const base::Closure& closure, BlobStatus status) { closure.Run(); },
        loop.QuitClosure()));
    loop.Run();
  }

  mojom::BytesProviderPtr CreateBytesProvider(const std::string& bytes) {
    if (!bytes_provider_runner_) {
      bytes_provider_runner_ =
          base::CreateSequencedTaskRunnerWithTraits({base::MayBlock()});
    }
    mojom::BytesProviderPtr result;
    auto provider = base::MakeUnique<MockBytesProvider>(
        std::vector<uint8_t>(bytes.begin(), bytes.end()), &reply_request_count_,
        &stream_request_count_, &file_request_count_);
    bytes_provider_runner_->PostTask(
        FROM_HERE, base::BindOnce(&BindBytesProvider, std::move(provider),
                                  MakeRequest(&result)));
    return result;
  }

  void CreateBytesProvider(const std::string& bytes,
                           mojom::BytesProviderRequest request) {
    if (!bytes_provider_runner_) {
      bytes_provider_runner_ =
          base::CreateSequencedTaskRunnerWithTraits({base::MayBlock()});
    }
    auto provider = base::MakeUnique<MockBytesProvider>(
        std::vector<uint8_t>(bytes.begin(), bytes.end()), &reply_request_count_,
        &stream_request_count_, &file_request_count_);
    bytes_provider_runner_->PostTask(
        FROM_HERE, base::BindOnce(&BindBytesProvider, std::move(provider),
                                  std::move(request)));
  }

  size_t BlobsUnderConstruction() {
    return registry_impl_->BlobsUnderConstructionForTesting();
  }

  void RegisterURL(mojom::BlobPtr blob,
                   const GURL& url,
                   mojom::BlobURLHandlePtr* url_handle_out) {
    base::RunLoop loop;
    registry_->RegisterURL(std::move(blob), url,
                           base::Bind(
                               [](base::Closure quit_closure,
                                  mojom::BlobURLHandlePtr* url_handle_out,
                                  mojom::BlobURLHandlePtr url_handle) {
                                 *url_handle_out = std::move(url_handle);
                                 quit_closure.Run();
                               },
                               loop.QuitClosure(), url_handle_out));
    loop.Run();
  }

 protected:
  base::ScopedTempDir data_dir_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<BlobStorageContext> context_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  std::unique_ptr<BlobRegistryImpl> registry_impl_;
  mojom::BlobRegistryPtr registry_;
  MockDelegate* delegate_ptr_;
  scoped_refptr<base::SequencedTaskRunner> bytes_provider_runner_;

  size_t reply_request_count_ = 0;
  size_t stream_request_count_ = 0;
  size_t file_request_count_ = 0;

  std::vector<std::string> bad_messages_;
};

TEST_F(BlobRegistryImplTest, GetBlobFromUUID) {
  const std::string kId = "id";
  std::unique_ptr<BlobDataHandle> handle =
      CreateBlobFromString(kId, "hello world");

  {
    mojom::BlobPtr blob;
    registry_->GetBlobFromUUID(MakeRequest(&blob), kId);
    EXPECT_EQ(kId, UUIDFromBlob(blob.get()));
    EXPECT_FALSE(blob.encountered_error());
  }

  {
    mojom::BlobPtr blob;
    registry_->GetBlobFromUUID(MakeRequest(&blob), "invalid id");
    blob.FlushForTesting();
    EXPECT_TRUE(blob.encountered_error());
  }
}

TEST_F(BlobRegistryImplTest, GetBlobFromEmptyUUID) {
  mojom::BlobPtr blob;
  registry_->GetBlobFromUUID(MakeRequest(&blob), "");
  blob.FlushForTesting();
  EXPECT_EQ(1u, bad_messages_.size());
  EXPECT_TRUE(blob.encountered_error());
}

TEST_F(BlobRegistryImplTest, Register_EmptyUUID) {
  mojom::BlobPtr blob;
  EXPECT_FALSE(registry_->Register(MakeRequest(&blob), "", "", "",
                                   std::vector<mojom::DataElementPtr>()));

  EXPECT_EQ(1u, bad_messages_.size());

  registry_.FlushForTesting();
  EXPECT_TRUE(registry_.encountered_error());

  blob.FlushForTesting();
  EXPECT_TRUE(blob.encountered_error());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_ExistingUUID) {
  const std::string kId = "id";
  std::unique_ptr<BlobDataHandle> handle =
      CreateBlobFromString(kId, "hello world");

  mojom::BlobPtr blob;
  EXPECT_FALSE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                   std::vector<mojom::DataElementPtr>()));

  EXPECT_EQ(1u, bad_messages_.size());

  registry_.FlushForTesting();
  EXPECT_TRUE(registry_.encountered_error());

  blob.FlushForTesting();
  EXPECT_TRUE(blob.encountered_error());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_EmptyBlob) {
  const std::string kId = "id";
  const std::string kContentType = "content/type";
  const std::string kContentDisposition = "disposition";

  mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, kContentType,
                                  kContentDisposition,
                                  std::vector<mojom::DataElementPtr>()));

  EXPECT_TRUE(bad_messages_.empty());

  EXPECT_EQ(kId, UUIDFromBlob(blob.get()));
  EXPECT_TRUE(context_->registry().HasEntry(kId));
  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  EXPECT_EQ(kContentType, handle->content_type());
  EXPECT_EQ(kContentDisposition, handle->content_disposition());
  EXPECT_EQ(0u, handle->size());

  WaitForBlobCompletion(handle.get());

  EXPECT_FALSE(handle->IsBroken());
  EXPECT_EQ(BlobStatus::DONE, handle->GetBlobStatus());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_ReferencedBlobClosedPipe) {
  const std::string kId = "id";

  std::vector<mojom::DataElementPtr> elements;
  mojom::BlobPtr referenced_blob;
  MakeRequest(&referenced_blob);
  elements.push_back(mojom::DataElement::NewBlob(
      mojom::DataElementBlob::New(std::move(referenced_blob), 0, 16)));

  mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_TRUE(handle->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_REFERENCED_BLOB_BROKEN, handle->GetBlobStatus());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_SelfReference) {
  const std::string kId = "id";

  mojom::BlobPtr blob;
  mojom::BlobRequest blob_request = MakeRequest(&blob);

  std::vector<mojom::DataElementPtr> elements;
  elements.push_back(mojom::DataElement::NewBlob(
      mojom::DataElementBlob::New(std::move(blob), 0, 16)));

  EXPECT_TRUE(registry_->Register(std::move(blob_request), kId, "", "",
                                  std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_TRUE(handle->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
            handle->GetBlobStatus());

  EXPECT_EQ(1u, bad_messages_.size());

  registry_.FlushForTesting();
  EXPECT_TRUE(registry_.encountered_error());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_CircularReference) {
  const std::string kId1 = "id1";
  const std::string kId2 = "id2";
  const std::string kId3 = "id3";

  mojom::BlobPtr blob1, blob2, blob3;
  mojom::BlobRequest blob_request1 = MakeRequest(&blob1);
  mojom::BlobRequest blob_request2 = MakeRequest(&blob2);
  mojom::BlobRequest blob_request3 = MakeRequest(&blob3);

  std::vector<mojom::DataElementPtr> elements1;
  elements1.push_back(mojom::DataElement::NewBlob(
      mojom::DataElementBlob::New(std::move(blob1), 0, 16)));

  std::vector<mojom::DataElementPtr> elements2;
  elements2.push_back(mojom::DataElement::NewBlob(
      mojom::DataElementBlob::New(std::move(blob2), 0, 16)));

  std::vector<mojom::DataElementPtr> elements3;
  elements3.push_back(mojom::DataElement::NewBlob(
      mojom::DataElementBlob::New(std::move(blob3), 0, 16)));

  EXPECT_TRUE(registry_->Register(std::move(blob_request1), kId1, "", "",
                                  std::move(elements2)));
  EXPECT_TRUE(registry_->Register(std::move(blob_request2), kId2, "", "",
                                  std::move(elements3)));
  EXPECT_TRUE(registry_->Register(std::move(blob_request3), kId3, "", "",
                                  std::move(elements1)));
  EXPECT_TRUE(bad_messages_.empty());

#if DCHECK_IS_ON()
  // Without DCHECKs on this will just hang forever.
  std::unique_ptr<BlobDataHandle> handle1 = context_->GetBlobDataFromUUID(kId1);
  std::unique_ptr<BlobDataHandle> handle2 = context_->GetBlobDataFromUUID(kId2);
  std::unique_ptr<BlobDataHandle> handle3 = context_->GetBlobDataFromUUID(kId3);
  WaitForBlobCompletion(handle1.get());
  WaitForBlobCompletion(handle2.get());
  WaitForBlobCompletion(handle3.get());

  EXPECT_TRUE(handle1->IsBroken());
  EXPECT_TRUE(handle2->IsBroken());
  EXPECT_TRUE(handle3->IsBroken());

  BlobStatus status1 = handle1->GetBlobStatus();
  BlobStatus status2 = handle2->GetBlobStatus();
  BlobStatus status3 = handle3->GetBlobStatus();
  EXPECT_TRUE(status1 == BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS ||
              status1 == BlobStatus::ERR_REFERENCED_BLOB_BROKEN);
  EXPECT_TRUE(status2 == BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS ||
              status2 == BlobStatus::ERR_REFERENCED_BLOB_BROKEN);
  EXPECT_TRUE(status3 == BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS ||
              status3 == BlobStatus::ERR_REFERENCED_BLOB_BROKEN);
  EXPECT_EQ((status1 == BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS) +
                (status2 == BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS) +
                (status3 == BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS),
            1);

  EXPECT_EQ(1u, bad_messages_.size());

  registry_.FlushForTesting();
  EXPECT_TRUE(registry_.encountered_error());
  EXPECT_EQ(0u, BlobsUnderConstruction());
#endif
}

TEST_F(BlobRegistryImplTest, Register_NonExistentBlob) {
  const std::string kId = "id";

  std::vector<mojom::DataElementPtr> elements;
  mojom::BlobPtr referenced_blob;
  mojo::MakeStrongBinding(base::MakeUnique<MockBlob>("mock blob"),
                          MakeRequest(&referenced_blob));
  elements.push_back(mojom::DataElement::NewBlob(
      mojom::DataElementBlob::New(std::move(referenced_blob), 0, 16)));

  mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_TRUE(handle->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
            handle->GetBlobStatus());

  EXPECT_EQ(1u, bad_messages_.size());

  registry_.FlushForTesting();
  EXPECT_TRUE(registry_.encountered_error());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_ValidBlobReferences) {
  const std::string kId1 = "id1";
  std::unique_ptr<BlobDataHandle> handle =
      CreateBlobFromString(kId1, "hello world");
  mojom::BlobPtr blob1;
  mojo::MakeStrongBinding(base::MakeUnique<MockBlob>(kId1),
                          MakeRequest(&blob1));

  const std::string kId2 = "id2";
  mojom::BlobPtr blob2;
  mojom::BlobRequest blob_request2 = MakeRequest(&blob2);

  std::vector<mojom::DataElementPtr> elements1;
  elements1.push_back(mojom::DataElement::NewBlob(
      mojom::DataElementBlob::New(std::move(blob1), 0, 8)));

  std::vector<mojom::DataElementPtr> elements2;
  elements2.push_back(mojom::DataElement::NewBlob(
      mojom::DataElementBlob::New(std::move(blob2), 0, 8)));

  mojom::BlobPtr final_blob;
  const std::string kId3 = "id3";
  EXPECT_TRUE(registry_->Register(MakeRequest(&final_blob), kId3, "", "",
                                  std::move(elements2)));
  EXPECT_TRUE(registry_->Register(std::move(blob_request2), kId2, "", "",
                                  std::move(elements1)));

  // kId3 references kId2, kId2 reference kId1, kId1 is a simple string.
  std::unique_ptr<BlobDataHandle> handle2 = context_->GetBlobDataFromUUID(kId2);
  std::unique_ptr<BlobDataHandle> handle3 = context_->GetBlobDataFromUUID(kId3);
  WaitForBlobCompletion(handle2.get());
  WaitForBlobCompletion(handle3.get());

  EXPECT_FALSE(handle2->IsBroken());
  ASSERT_EQ(BlobStatus::DONE, handle2->GetBlobStatus());

  EXPECT_FALSE(handle3->IsBroken());
  ASSERT_EQ(BlobStatus::DONE, handle3->GetBlobStatus());

  BlobDataBuilder expected_blob_data(kId2);
  expected_blob_data.AppendData("hello wo");

  EXPECT_EQ(expected_blob_data, *handle2->CreateSnapshot());
  EXPECT_EQ(expected_blob_data, *handle3->CreateSnapshot());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_UnreadableFile) {
  delegate_ptr_->can_read_file_result = false;

  const std::string kId = "id";

  std::vector<mojom::DataElementPtr> elements;
  elements.push_back(mojom::DataElement::NewFile(mojom::DataElementFile::New(
      base::FilePath(FILE_PATH_LITERAL("foobar")), 0, 16, base::nullopt)));

  mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_TRUE(handle->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_FILE_WRITE_FAILED, handle->GetBlobStatus());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_ValidFile) {
  delegate_ptr_->can_read_file_result = true;

  const std::string kId = "id";
  const base::FilePath path(FILE_PATH_LITERAL("foobar"));

  std::vector<mojom::DataElementPtr> elements;
  elements.push_back(mojom::DataElement::NewFile(
      mojom::DataElementFile::New(path, 0, 16, base::nullopt)));

  mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_FALSE(handle->IsBroken());
  ASSERT_EQ(BlobStatus::DONE, handle->GetBlobStatus());

  BlobDataBuilder expected_blob_data(kId);
  expected_blob_data.AppendFile(path, 0, 16, base::Time());

  EXPECT_EQ(expected_blob_data, *handle->CreateSnapshot());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_FileSystemFile_InvalidScheme) {
  const std::string kId = "id";

  std::vector<mojom::DataElementPtr> elements;
  elements.push_back(mojom::DataElement::NewFileFilesystem(
      mojom::DataElementFilesystemURL::New(GURL("http://foobar.com/"), 0, 16,
                                           base::nullopt)));

  mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_TRUE(handle->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_FILE_WRITE_FAILED, handle->GetBlobStatus());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_FileSystemFile_UnreadablFile) {
  delegate_ptr_->can_read_file_system_file_result = false;

  const std::string kId = "id";
  const GURL url("filesystem:http://example.com/temporary/myfile.png");

  std::vector<mojom::DataElementPtr> elements;
  elements.push_back(mojom::DataElement::NewFileFilesystem(
      mojom::DataElementFilesystemURL::New(url, 0, 16, base::nullopt)));

  mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_TRUE(handle->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_FILE_WRITE_FAILED, handle->GetBlobStatus());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_FileSystemFile_Valid) {
  delegate_ptr_->can_read_file_system_file_result = true;

  const std::string kId = "id";
  const GURL url("filesystem:http://example.com/temporary/myfile.png");

  std::vector<mojom::DataElementPtr> elements;
  elements.push_back(mojom::DataElement::NewFileFilesystem(
      mojom::DataElementFilesystemURL::New(url, 0, 16, base::nullopt)));

  mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_FALSE(handle->IsBroken());
  ASSERT_EQ(BlobStatus::DONE, handle->GetBlobStatus());

  BlobDataBuilder expected_blob_data(kId);
  expected_blob_data.AppendFileSystemFile(url, 0, 16, base::Time());

  EXPECT_EQ(expected_blob_data, *handle->CreateSnapshot());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_BytesInvalidEmbeddedData) {
  const std::string kId = "id";

  std::vector<mojom::DataElementPtr> elements;
  elements.push_back(mojom::DataElement::NewBytes(mojom::DataElementBytes::New(
      10, std::vector<uint8_t>(5), CreateBytesProvider(""))));

  mojom::BlobPtr blob;
  EXPECT_FALSE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                   std::move(elements)));
  EXPECT_EQ(1u, bad_messages_.size());

  registry_.FlushForTesting();
  EXPECT_TRUE(registry_.encountered_error());

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_TRUE(handle->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
            handle->GetBlobStatus());

  EXPECT_EQ(0u, reply_request_count_);
  EXPECT_EQ(0u, stream_request_count_);
  EXPECT_EQ(0u, file_request_count_);
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_BytesInvalidDataSize) {
  const std::string kId = "id";

  // Two elements that together are more than uint64_t::max bytes.
  std::vector<mojom::DataElementPtr> elements;
  elements.push_back(mojom::DataElement::NewBytes(
      mojom::DataElementBytes::New(8, base::nullopt, CreateBytesProvider(""))));
  elements.push_back(mojom::DataElement::NewBytes(
      mojom::DataElementBytes::New(std::numeric_limits<uint64_t>::max() - 4,
                                   base::nullopt, CreateBytesProvider(""))));

  mojom::BlobPtr blob;
  EXPECT_FALSE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                   std::move(elements)));
  EXPECT_EQ(1u, bad_messages_.size());

  registry_.FlushForTesting();
  EXPECT_TRUE(registry_.encountered_error());

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_TRUE(handle->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
            handle->GetBlobStatus());

  EXPECT_EQ(0u, reply_request_count_);
  EXPECT_EQ(0u, stream_request_count_);
  EXPECT_EQ(0u, file_request_count_);
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_BytesOutOfMemory) {
  const std::string kId = "id";

  // Two elements that together don't fit in the test quota.
  std::vector<mojom::DataElementPtr> elements;
  elements.push_back(mojom::DataElement::NewBytes(mojom::DataElementBytes::New(
      kTestBlobStorageMaxDiskSpace, base::nullopt, CreateBytesProvider(""))));
  elements.push_back(mojom::DataElement::NewBytes(mojom::DataElementBytes::New(
      kTestBlobStorageMaxDiskSpace, base::nullopt, CreateBytesProvider(""))));

  mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_TRUE(handle->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_OUT_OF_MEMORY, handle->GetBlobStatus());

  EXPECT_EQ(0u, reply_request_count_);
  EXPECT_EQ(0u, stream_request_count_);
  EXPECT_EQ(0u, file_request_count_);
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_ValidEmbeddedBytes) {
  const std::string kId = "id";
  const std::string kData = "hello world";

  std::vector<mojom::DataElementPtr> elements;
  elements.push_back(mojom::DataElement::NewBytes(mojom::DataElementBytes::New(
      kData.size(), std::vector<uint8_t>(kData.begin(), kData.end()),
      CreateBytesProvider(kData))));

  mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_FALSE(handle->IsBroken());
  ASSERT_EQ(BlobStatus::DONE, handle->GetBlobStatus());

  BlobDataBuilder expected_blob_data(kId);
  expected_blob_data.AppendData(kData);

  EXPECT_EQ(expected_blob_data, *handle->CreateSnapshot());

  EXPECT_EQ(0u, reply_request_count_);
  EXPECT_EQ(0u, stream_request_count_);
  EXPECT_EQ(0u, file_request_count_);
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_ValidBytesAsReply) {
  const std::string kId = "id";
  const std::string kData = "hello";

  std::vector<mojom::DataElementPtr> elements;
  elements.push_back(mojom::DataElement::NewBytes(mojom::DataElementBytes::New(
      kData.size(), base::nullopt, CreateBytesProvider(kData))));

  mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_FALSE(handle->IsBroken());
  ASSERT_EQ(BlobStatus::DONE, handle->GetBlobStatus());

  BlobDataBuilder expected_blob_data(kId);
  expected_blob_data.AppendData(kData);

  EXPECT_EQ(expected_blob_data, *handle->CreateSnapshot());

  EXPECT_EQ(1u, reply_request_count_);
  EXPECT_EQ(0u, stream_request_count_);
  EXPECT_EQ(0u, file_request_count_);
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_ValidBytesAsStream) {
  const std::string kId = "id";
  const std::string kData =
      base::RandBytesAsString(kTestBlobStorageMaxSharedMemoryBytes * 3 + 13);

  std::vector<mojom::DataElementPtr> elements;
  elements.push_back(mojom::DataElement::NewBytes(mojom::DataElementBytes::New(
      kData.size(), base::nullopt, CreateBytesProvider(kData))));

  mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_FALSE(handle->IsBroken());
  ASSERT_EQ(BlobStatus::DONE, handle->GetBlobStatus());

  size_t offset = 0;
  BlobDataBuilder expected_blob_data(kId);
  while (offset < kData.size()) {
    expected_blob_data.AppendData(
        kData.substr(offset, kTestBlobStorageMaxBytesDataItemSize));
    offset += kTestBlobStorageMaxBytesDataItemSize;
  }

  EXPECT_EQ(expected_blob_data, *handle->CreateSnapshot());

  EXPECT_EQ(0u, reply_request_count_);
  EXPECT_EQ(1u, stream_request_count_);
  EXPECT_EQ(0u, file_request_count_);
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_ValidBytesAsFile) {
  const std::string kId = "id";
  const std::string kData =
      base::RandBytesAsString(kTestBlobStorageMaxBlobMemorySize + 42);

  std::vector<mojom::DataElementPtr> elements;
  elements.push_back(mojom::DataElement::NewBytes(mojom::DataElementBytes::New(
      kData.size(), base::nullopt, CreateBytesProvider(kData))));

  mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_FALSE(handle->IsBroken());
  ASSERT_EQ(BlobStatus::DONE, handle->GetBlobStatus());

  BlobDataBuilder expected_blob_data(kId);
  expected_blob_data.AppendData(kData);

  size_t expected_file_count =
      1 + kData.size() / kTestBlobStorageMaxFileSizeBytes;
  EXPECT_EQ(0u, reply_request_count_);
  EXPECT_EQ(0u, stream_request_count_);
  EXPECT_EQ(expected_file_count, file_request_count_);

  auto snapshot = handle->CreateSnapshot();
  EXPECT_EQ(expected_file_count, snapshot->items().size());
  size_t remaining_size = kData.size();
  for (const auto& item : snapshot->items()) {
    EXPECT_EQ(DataElement::TYPE_FILE, item->type());
    EXPECT_EQ(0u, item->offset());
    if (remaining_size > kTestBlobStorageMaxFileSizeBytes)
      EXPECT_EQ(kTestBlobStorageMaxFileSizeBytes, item->length());
    else
      EXPECT_EQ(remaining_size, item->length());
    remaining_size -= item->length();
  }
  EXPECT_EQ(0u, remaining_size);
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, Register_BytesProviderClosedPipe) {
  const std::string kId = "id";

  mojom::BytesProviderPtr bytes_provider;
  MakeRequest(&bytes_provider);

  std::vector<mojom::DataElementPtr> elements;
  elements.push_back(mojom::DataElement::NewBytes(mojom::DataElementBytes::New(
      32, base::nullopt, std::move(bytes_provider))));

  mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  WaitForBlobCompletion(handle.get());

  EXPECT_TRUE(handle->IsBroken());
  EXPECT_EQ(BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT, handle->GetBlobStatus());
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest,
       Register_DefereferencedWhileBuildingBeforeBreaking) {
  const std::string kId = "id";

  mojom::BytesProviderPtr bytes_provider;
  mojom::BytesProviderRequest request = MakeRequest(&bytes_provider);

  std::vector<mojom::DataElementPtr> elements;
  elements.push_back(mojom::DataElement::NewBytes(mojom::DataElementBytes::New(
      32, base::nullopt, std::move(bytes_provider))));

  mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  EXPECT_TRUE(context_->registry().HasEntry(kId));
  EXPECT_TRUE(context_->GetBlobDataFromUUID(kId)->IsBeingBuilt());
  EXPECT_EQ(1u, BlobsUnderConstruction());

  // Now drop all references to the blob.
  blob.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(context_->registry().HasEntry(kId));

  // Now cause construction to fail, if it would still be going on.
  request = nullptr;
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest,
       Register_DefereferencedWhileBuildingBeforeTransporting) {
  const std::string kId = "id";
  const std::string kData = "hello world";

  mojom::BytesProviderPtr bytes_provider;
  mojom::BytesProviderRequest request = MakeRequest(&bytes_provider);

  std::vector<mojom::DataElementPtr> elements;
  elements.push_back(mojom::DataElement::NewBytes(mojom::DataElementBytes::New(
      kData.size(), base::nullopt, std::move(bytes_provider))));

  mojom::BlobPtr blob;
  EXPECT_TRUE(registry_->Register(MakeRequest(&blob), kId, "", "",
                                  std::move(elements)));
  EXPECT_TRUE(bad_messages_.empty());

  EXPECT_TRUE(context_->registry().HasEntry(kId));
  EXPECT_TRUE(context_->GetBlobDataFromUUID(kId)->IsBeingBuilt());
  EXPECT_EQ(1u, BlobsUnderConstruction());

  // Now drop all references to the blob.
  blob.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(context_->registry().HasEntry(kId));

  // Now cause construction to complete, if it would still be going on.
  CreateBytesProvider(kData, std::move(request));
  scoped_task_environment_.RunUntilIdle();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, BlobsUnderConstruction());
}

TEST_F(BlobRegistryImplTest, PublicBlobUrls) {
  const std::string kId = "id";
  std::unique_ptr<BlobDataHandle> handle =
      CreateBlobFromString(kId, "hello world");

  mojom::BlobPtr blob;
  registry_->GetBlobFromUUID(MakeRequest(&blob), kId);
  EXPECT_EQ(kId, UUIDFromBlob(blob.get()));
  EXPECT_FALSE(blob.encountered_error());

  // Now register a url for that blob.
  const GURL kUrl("blob:id");
  mojom::BlobURLHandlePtr url_handle;
  RegisterURL(std::move(blob), kUrl, &url_handle);

  std::unique_ptr<BlobDataHandle> blob_data_handle =
      context_->GetBlobDataFromPublicURL(kUrl);
  ASSERT_TRUE(blob_data_handle.get());
  EXPECT_EQ(kId, blob_data_handle->uuid());

  handle.reset();
  base::RunLoop().RunUntilIdle();

  // The url registration should keep the blob alive even after
  // explicit references are dropped.
  blob_data_handle = context_->GetBlobDataFromPublicURL(kUrl);
  EXPECT_TRUE(blob_data_handle);
  blob_data_handle.reset();

  // Finally drop the URL handle.
  url_handle.reset();
  base::RunLoop().RunUntilIdle();

  blob_data_handle = context_->GetBlobDataFromPublicURL(kUrl);
  EXPECT_FALSE(blob_data_handle.get());
  EXPECT_FALSE(context_->registry().HasEntry(kId));
}

}  // namespace storage
