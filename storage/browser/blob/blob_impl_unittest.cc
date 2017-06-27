// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_impl.h"

#include "base/test/scoped_task_environment.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

class BlobImplTest : public testing::Test {
 public:
  void SetUp() override { context_ = base::MakeUnique<BlobStorageContext>(); }

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

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<BlobStorageContext> context_;
};

TEST_F(BlobImplTest, GetInternalUUID) {
  const std::string kId = "id";
  auto handle = CreateBlobFromString(kId, "hello world");

  mojom::BlobPtr ptr;
  auto blob = BlobImpl::Create(std::move(handle), MakeRequest(&ptr));
  EXPECT_EQ(kId, UUIDFromBlob(blob.get()));
  EXPECT_EQ(kId, UUIDFromBlob(ptr.get()));
}

TEST_F(BlobImplTest, CloneAndLifetime) {
  const std::string kId = "id";
  auto handle = CreateBlobFromString(kId, "hello world");

  mojom::BlobPtr ptr;
  auto blob = BlobImpl::Create(std::move(handle), MakeRequest(&ptr));
  EXPECT_EQ(kId, UUIDFromBlob(ptr.get()));

  // Blob should exist in registry as long as connection is alive.
  EXPECT_TRUE(context_->registry().HasEntry(kId));
  EXPECT_TRUE(blob);

  mojom::BlobPtr clone;
  blob->Clone(MakeRequest(&clone));
  EXPECT_EQ(kId, UUIDFromBlob(clone.get()));
  clone.FlushForTesting();

  ptr.reset();
  blob->FlushForTesting();
  EXPECT_TRUE(context_->registry().HasEntry(kId));
  EXPECT_TRUE(blob);

  clone.reset();
  blob->FlushForTesting();
  EXPECT_FALSE(context_->registry().HasEntry(kId));
  EXPECT_FALSE(blob);
}

}  // namespace storage
