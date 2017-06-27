// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_registry_impl.h"

#include "base/test/scoped_task_environment.h"
#include "mojo/edk/embedder/embedder.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

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

TEST_F(BlobRegistryImplTest, RegisterEmptyUUID) {
  mojom::BlobPtr blob;
  base::RunLoop loop;
  registry_.set_connection_error_handler(loop.QuitClosure());
  registry_->Register(MakeRequest(&blob), "", "", "",
                      std::vector<mojom::DataElementPtr>(), loop.QuitClosure());
  loop.Run();

  EXPECT_EQ(1u, bad_messages_.size());
  EXPECT_TRUE(registry_.encountered_error());

  blob.FlushForTesting();
  EXPECT_TRUE(blob.encountered_error());
}

TEST_F(BlobRegistryImplTest, RegisterExistingUUID) {
  const std::string kId = "id";
  std::unique_ptr<BlobDataHandle> handle =
      CreateBlobFromString(kId, "hello world");

  mojom::BlobPtr blob;
  base::RunLoop loop;
  registry_.set_connection_error_handler(loop.QuitClosure());
  registry_->Register(MakeRequest(&blob), kId, "", "",
                      std::vector<mojom::DataElementPtr>(), loop.QuitClosure());
  loop.Run();

  EXPECT_EQ(1u, bad_messages_.size());
  EXPECT_TRUE(registry_.encountered_error());

  blob.FlushForTesting();
  EXPECT_TRUE(blob.encountered_error());
}

TEST_F(BlobRegistryImplTest, RegisterEmptyBlob) {
  const std::string kId = "id";
  const std::string kContentType = "content/type";
  const std::string kContentDisposition = "disposition";

  mojom::BlobPtr blob;
  base::RunLoop loop;
  registry_->Register(MakeRequest(&blob), kId, kContentType,
                      kContentDisposition, std::vector<mojom::DataElementPtr>(),
                      loop.QuitClosure());
  loop.Run();

  EXPECT_TRUE(bad_messages_.empty());

  EXPECT_EQ(kId, UUIDFromBlob(blob.get()));
  EXPECT_TRUE(context_->registry().HasEntry(kId));
  std::unique_ptr<BlobDataHandle> handle = context_->GetBlobDataFromUUID(kId);
  EXPECT_EQ(kContentType, handle->content_type());
  EXPECT_EQ(kContentDisposition, handle->content_disposition());
  EXPECT_EQ(0u, handle->size());
}

}  // namespace storage
