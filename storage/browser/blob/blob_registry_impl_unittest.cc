// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_registry_impl.h"

#include "base/test/scoped_task_environment.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

namespace {

class MockBlob : public mojom::Blob {
 public:
  explicit MockBlob(const std::string& uuid) : uuid_(uuid) {}

  void Clone(mojom::BlobRequest request) override {
    mojo::MakeStrongBinding(base::MakeUnique<MockBlob>(uuid_),
                            std::move(request));
  }

  void GetInternalUUID(GetInternalUUIDCallback callback) override {
    std::move(callback).Run(uuid_);
  }

 private:
  std::string uuid_;
};

}  // namespace

class BlobRegistryImplTest : public testing::Test {
 public:
  void SetUp() override {
    context_ = base::MakeUnique<BlobStorageContext>();
    registry_impl_ = base::MakeUnique<BlobRegistryImpl>(context_.get());
    registry_impl_->Bind(MakeRequest(&registry_));
    mojo::edk::SetDefaultProcessErrorCallback(base::Bind(
        &BlobRegistryImplTest::OnBadMessage, base::Unretained(this)));
  }

  void TearDown() override {
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

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<BlobStorageContext> context_;
  std::unique_ptr<BlobRegistryImpl> registry_impl_;
  mojom::BlobRegistryPtr registry_;

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
  EXPECT_EQ(BlobStatus::DONE, handle2->GetBlobStatus());

  EXPECT_FALSE(handle3->IsBroken());
  EXPECT_EQ(BlobStatus::DONE, handle3->GetBlobStatus());

  BlobDataBuilder expected_blob_data(kId2);
  expected_blob_data.AppendData("hello wo");

  EXPECT_EQ(expected_blob_data, *handle2->CreateSnapshot());
  EXPECT_EQ(expected_blob_data, *handle3->CreateSnapshot());
}

}  // namespace storage
