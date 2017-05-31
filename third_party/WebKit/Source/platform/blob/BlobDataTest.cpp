// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/blob/BlobData.h"

#include <memory>
#include <utility>
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "platform/UUID.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/InterfaceProvider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(BlobDataTest, Consolidation) {
  const size_t kMaxConsolidatedItemSizeInBytes = 15 * 1024;
  BlobData data(BlobData::FileCompositionStatus::NO_UNKNOWN_SIZE_FILES);
  const char* text1 = "abc";
  const char* text2 = "def";
  data.AppendBytes(text1, 3u);
  data.AppendBytes(text2, 3u);
  data.AppendText("ps1", false);
  data.AppendText("ps2", false);

  EXPECT_EQ(1u, data.items_.size());
  EXPECT_EQ(12u, data.items_[0].data->length());
  EXPECT_EQ(0, memcmp(data.items_[0].data->data(), "abcdefps1ps2", 12));

  std::unique_ptr<char[]> large_data =
      WrapArrayUnique(new char[kMaxConsolidatedItemSizeInBytes]);
  data.AppendBytes(large_data.get(), kMaxConsolidatedItemSizeInBytes);

  EXPECT_EQ(2u, data.items_.size());
  EXPECT_EQ(12u, data.items_[0].data->length());
  EXPECT_EQ(kMaxConsolidatedItemSizeInBytes, data.items_[1].data->length());
}

namespace {

class MockBlobRegistry : public storage::mojom::blink::BlobRegistry {
 public:
  void Register(storage::mojom::blink::BlobRequest blob,
                const String& uuid,
                const String& content_type,
                const String& content_disposition,
                Vector<storage::mojom::blink::DataElementPtr> elements,
                RegisterCallback callback) override {
    registrations.push_back(Registration{std::move(blob), uuid, content_type,
                                         content_disposition,
                                         std::move(elements)});
    std::move(callback).Run();
  }

  void GetBlobFromUUID(storage::mojom::blink::BlobRequest blob,
                       const String& uuid) override {
    binding_requests.push_back(BindingRequest{std::move(blob), uuid});
  }

  struct Registration {
    storage::mojom::blink::BlobRequest request;
    String uuid;
    String content_type;
    String content_disposition;
    Vector<storage::mojom::blink::DataElementPtr> elements;
  };
  Vector<Registration> registrations;

  struct BindingRequest {
    storage::mojom::blink::BlobRequest request;
    String uuid;
  };
  Vector<BindingRequest> binding_requests;
};

class MojoBlobInterfaceProvider : public InterfaceProvider {
 public:
  explicit MojoBlobInterfaceProvider(
      storage::mojom::blink::BlobRegistry* mock_registry)
      : mock_registry_(mock_registry) {}

  void GetInterface(const char* name,
                    mojo::ScopedMessagePipeHandle handle) override {
    if (std::string(name) == storage::mojom::blink::BlobRegistry::Name_) {
      registry_bindings_.AddBinding(
          mock_registry_,
          storage::mojom::blink::BlobRegistryRequest(std::move(handle)));
      return;
    }
  }

  void Flush() { registry_bindings_.FlushForTesting(); }

 private:
  storage::mojom::blink::BlobRegistry* mock_registry_;
  mojo::BindingSet<storage::mojom::blink::BlobRegistry> registry_bindings_;
};

class MojoBlobTestPlatform : public TestingPlatformSupport {
 public:
  explicit MojoBlobTestPlatform(
      storage::mojom::blink::BlobRegistry* mock_registry)
      : interface_provider_(
            WTF::MakeUnique<MojoBlobInterfaceProvider>(mock_registry)) {}

  InterfaceProvider* GetInterfaceProvider() override {
    return interface_provider_.get();
  }

  void Flush() { interface_provider_->Flush(); }

 private:
  std::unique_ptr<MojoBlobInterfaceProvider> interface_provider_;
};

}  // namespace

class BlobDataHandleTest : public testing::Test {
 public:
  BlobDataHandleTest()
      : enable_mojo_blobs_(true), testing_platform_(&mock_blob_registry_) {}

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  ScopedMojoBlobsForTest enable_mojo_blobs_;
  ScopedTestingPlatformSupport<MojoBlobTestPlatform,
                               storage::mojom::blink::BlobRegistry*>
      testing_platform_;
  MockBlobRegistry mock_blob_registry_;
};

TEST_F(BlobDataHandleTest, CreateEmpty) {
  RefPtr<BlobDataHandle> handle = BlobDataHandle::Create();
  EXPECT_TRUE(handle->GetType().IsNull());
  EXPECT_EQ(0u, handle->size());
  EXPECT_FALSE(handle->IsSingleUnknownSizeFile());

  testing_platform_->Flush();
  EXPECT_EQ(0u, mock_blob_registry_.binding_requests.size());
  ASSERT_EQ(1u, mock_blob_registry_.registrations.size());
  const auto& reg = mock_blob_registry_.registrations[0];
  EXPECT_EQ(handle->Uuid(), reg.uuid);
  EXPECT_EQ("", reg.content_type);
  EXPECT_EQ("", reg.content_disposition);
  EXPECT_EQ(0u, reg.elements.size());
}

TEST_F(BlobDataHandleTest, CreateFromEmptyData) {
  String kType = "content/type";

  std::unique_ptr<BlobData> data = BlobData::Create();
  data->SetContentType(kType);

  RefPtr<BlobDataHandle> handle = BlobDataHandle::Create(std::move(data), 0);
  EXPECT_EQ(kType, handle->GetType());
  EXPECT_EQ(0u, handle->size());
  EXPECT_FALSE(handle->IsSingleUnknownSizeFile());

  testing_platform_->Flush();
  EXPECT_EQ(0u, mock_blob_registry_.binding_requests.size());
  ASSERT_EQ(1u, mock_blob_registry_.registrations.size());
  const auto& reg = mock_blob_registry_.registrations[0];
  EXPECT_EQ(handle->Uuid(), reg.uuid);
  EXPECT_EQ(kType, reg.content_type);
  EXPECT_EQ("", reg.content_disposition);
  EXPECT_EQ(0u, reg.elements.size());
}

TEST_F(BlobDataHandleTest, CreateFromUUID) {
  String kUuid = CreateCanonicalUUIDString();
  String kType = "content/type";
  uint64_t kSize = 1234;

  RefPtr<BlobDataHandle> handle = BlobDataHandle::Create(kUuid, kType, kSize);
  EXPECT_EQ(kUuid, handle->Uuid());
  EXPECT_EQ(kType, handle->GetType());
  EXPECT_EQ(kSize, handle->size());
  EXPECT_FALSE(handle->IsSingleUnknownSizeFile());

  testing_platform_->Flush();
  EXPECT_EQ(0u, mock_blob_registry_.registrations.size());
  ASSERT_EQ(1u, mock_blob_registry_.binding_requests.size());
  EXPECT_EQ(kUuid, mock_blob_registry_.binding_requests[0].uuid);
}

}  // namespace blink
