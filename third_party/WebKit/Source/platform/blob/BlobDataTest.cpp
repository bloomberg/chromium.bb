// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/blob/BlobData.h"

#include <memory>
#include <utility>
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "platform/UUID.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/InterfaceProvider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using storage::mojom::blink::Blob;
using storage::mojom::blink::BlobRegistry;
using storage::mojom::blink::BlobRegistryRequest;
using storage::mojom::blink::BlobRequest;
using storage::mojom::blink::DataElement;
using storage::mojom::blink::DataElementBlob;
using storage::mojom::blink::DataElementBytes;
using storage::mojom::blink::DataElementFile;
using storage::mojom::blink::DataElementFilesystemURL;
using storage::mojom::blink::DataElementPtr;

namespace blink {

namespace {
const size_t kMaxConsolidatedItemSizeInBytes = 15 * 1024;
}

TEST(BlobDataTest, Consolidation) {
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

class MockBlob : public Blob {
 public:
  explicit MockBlob(const String& uuid) : uuid_(uuid) {}

  void Clone(BlobRequest request) override {
    mojo::MakeStrongBinding(WTF::MakeUnique<MockBlob>(uuid_),
                            std::move(request));
  }

  void GetInternalUUID(GetInternalUUIDCallback callback) override {
    std::move(callback).Run(uuid_);
  }

 private:
  String uuid_;
};

class MockBlobRegistry : public BlobRegistry {
 public:
  void Register(BlobRequest blob,
                const String& uuid,
                const String& content_type,
                const String& content_disposition,
                Vector<DataElementPtr> elements,
                RegisterCallback callback) override {
    registrations.push_back(Registration{
        uuid, content_type, content_disposition, std::move(elements)});
    mojo::MakeStrongBinding(WTF::MakeUnique<MockBlob>(uuid), std::move(blob));
    std::move(callback).Run();
  }

  void GetBlobFromUUID(BlobRequest blob, const String& uuid) override {
    binding_requests.push_back(BindingRequest{uuid});
    mojo::MakeStrongBinding(WTF::MakeUnique<MockBlob>(uuid), std::move(blob));
  }

  struct Registration {
    String uuid;
    String content_type;
    String content_disposition;
    Vector<DataElementPtr> elements;
  };
  Vector<Registration> registrations;

  struct BindingRequest {
    String uuid;
  };
  Vector<BindingRequest> binding_requests;
};

class MojoBlobInterfaceProvider : public InterfaceProvider {
 public:
  explicit MojoBlobInterfaceProvider(BlobRegistry* mock_registry)
      : mock_registry_(mock_registry) {}

  void GetInterface(const char* name,
                    mojo::ScopedMessagePipeHandle handle) override {
    if (std::string(name) == BlobRegistry::Name_) {
      registry_bindings_.AddBinding(mock_registry_,
                                    BlobRegistryRequest(std::move(handle)));
      return;
    }
  }

  void Flush() { registry_bindings_.FlushForTesting(); }

 private:
  BlobRegistry* mock_registry_;
  mojo::BindingSet<BlobRegistry> registry_bindings_;
};

class MojoBlobTestPlatform : public TestingPlatformSupport {
 public:
  explicit MojoBlobTestPlatform(BlobRegistry* mock_registry)
      : interface_provider_(
            WTF::MakeUnique<MojoBlobInterfaceProvider>(mock_registry)) {}

  InterfaceProvider* GetInterfaceProvider() override {
    return interface_provider_.get();
  }

  void Flush() { interface_provider_->Flush(); }

 private:
  std::unique_ptr<MojoBlobInterfaceProvider> interface_provider_;
};

struct ExpectedElement {
  DataElementPtr element;
  String blob_uuid;
  Vector<uint8_t> large_data;

  static ExpectedElement EmbeddedBytes(Vector<uint8_t> embedded_data) {
    uint64_t size = embedded_data.size();
    return ExpectedElement{DataElement::NewBytes(
        DataElementBytes::New(size, std::move(embedded_data), nullptr))};
  }

  static ExpectedElement LargeBytes(Vector<uint8_t> data) {
    uint64_t size = data.size();
    return ExpectedElement{DataElement::NewBytes(DataElementBytes::New(
                               size, WTF::nullopt, nullptr)),
                           String(), std::move(data)};
  }

  static ExpectedElement File(const String& path,
                              uint64_t offset,
                              uint64_t length,
                              WTF::Time time) {
    return ExpectedElement{
        DataElement::NewFile(DataElementFile::New(path, offset, length, time))};
  }

  static ExpectedElement FileFilesystem(const KURL& url,
                                        uint64_t offset,
                                        uint64_t length,
                                        WTF::Time time) {
    return ExpectedElement{DataElement::NewFileFilesystem(
        DataElementFilesystemURL::New(url, offset, length, time))};
  }

  static ExpectedElement Blob(const String& uuid,
                              uint64_t offset,
                              uint64_t length) {
    return ExpectedElement{
        DataElement::NewBlob(DataElementBlob::New(nullptr, offset, length)),
        uuid};
  }
};

}  // namespace

class BlobDataHandleTest : public testing::Test {
 public:
  BlobDataHandleTest()
      : enable_mojo_blobs_(true), testing_platform_(&mock_blob_registry_) {}

  void SetUp() override {
    small_test_data_.resize(1024);
    medium_test_data_.resize(1024 * 32);
    large_test_data_.resize(1024 * 512);
    for (size_t i = 0; i < small_test_data_.size(); ++i)
      small_test_data_[i] = i;
    for (size_t i = 0; i < medium_test_data_.size(); ++i)
      medium_test_data_[i] = i % 191;
    for (size_t i = 0; i < large_test_data_.size(); ++i)
      large_test_data_[i] = i % 251;

    ASSERT_LT(small_test_data_.size(), kMaxConsolidatedItemSizeInBytes);
    ASSERT_LT(medium_test_data_.size(),
              DataElementBytes::kMaximumEmbeddedDataSize);
    ASSERT_GT(medium_test_data_.size(), kMaxConsolidatedItemSizeInBytes);
    ASSERT_GT(large_test_data_.size(),
              DataElementBytes::kMaximumEmbeddedDataSize);

    empty_blob_ = BlobDataHandle::Create();

    std::unique_ptr<BlobData> test_data = BlobData::Create();
    test_data->AppendBytes(large_test_data_.data(), large_test_data_.size());
    test_blob_ =
        BlobDataHandle::Create(std::move(test_data), large_test_data_.size());

    testing_platform_->Flush();
    ASSERT_EQ(2u, mock_blob_registry_.registrations.size());
    empty_blob_uuid_ = mock_blob_registry_.registrations[0].uuid;
    test_blob_uuid_ = mock_blob_registry_.registrations[1].uuid;
    mock_blob_registry_.registrations.clear();
  }

  void TestCreateBlob(std::unique_ptr<BlobData> data,
                      Vector<ExpectedElement> expected_elements) {
    size_t blob_size = data->length();
    String type = data->ContentType();
    bool is_single_unknown_size_file = data->IsSingleUnknownSizeFile();

    RefPtr<BlobDataHandle> handle =
        BlobDataHandle::Create(std::move(data), blob_size);
    EXPECT_EQ(blob_size, handle->size());
    EXPECT_EQ(type, handle->GetType());
    EXPECT_EQ(is_single_unknown_size_file, handle->IsSingleUnknownSizeFile());

    testing_platform_->Flush();
    EXPECT_EQ(0u, mock_blob_registry_.binding_requests.size());
    ASSERT_EQ(1u, mock_blob_registry_.registrations.size());
    const auto& reg = mock_blob_registry_.registrations[0];
    EXPECT_EQ(handle->Uuid(), reg.uuid);
    EXPECT_EQ(type.IsNull() ? "" : type, reg.content_type);
    EXPECT_EQ("", reg.content_disposition);
    ASSERT_EQ(expected_elements.size(), reg.elements.size());
    for (size_t i = 0; i < expected_elements.size(); ++i) {
      const auto& expected = expected_elements[i].element;
      const auto& actual = reg.elements[i];
      if (expected->is_bytes()) {
        ASSERT_TRUE(actual->is_bytes());
        EXPECT_EQ(expected->get_bytes()->length, actual->get_bytes()->length);
        EXPECT_EQ(expected->get_bytes()->embedded_data,
                  actual->get_bytes()->embedded_data);

        base::RunLoop loop;
        Vector<uint8_t> received_bytes;
        actual->get_bytes()->data->RequestAsReply(base::Bind(
            [](base::Closure quit_closure, Vector<uint8_t>* bytes_out,
               const Vector<uint8_t>& bytes) {
              *bytes_out = bytes;
              quit_closure.Run();
            },
            loop.QuitClosure(), &received_bytes));
        loop.Run();
        if (expected->get_bytes()->embedded_data)
          EXPECT_EQ(expected->get_bytes()->embedded_data, received_bytes);
        else
          EXPECT_EQ(expected_elements[i].large_data, received_bytes);
      } else if (expected->is_file()) {
        ASSERT_TRUE(actual->is_file());
        EXPECT_EQ(expected->get_file()->path, actual->get_file()->path);
        EXPECT_EQ(expected->get_file()->length, actual->get_file()->length);
        EXPECT_EQ(expected->get_file()->offset, actual->get_file()->offset);
        EXPECT_EQ(expected->get_file()->expected_modification_time,
                  actual->get_file()->expected_modification_time);
      } else if (expected->is_file_filesystem()) {
        ASSERT_TRUE(actual->is_file_filesystem());
        EXPECT_EQ(expected->get_file_filesystem()->url,
                  actual->get_file_filesystem()->url);
        EXPECT_EQ(expected->get_file_filesystem()->length,
                  actual->get_file_filesystem()->length);
        EXPECT_EQ(expected->get_file_filesystem()->offset,
                  actual->get_file_filesystem()->offset);
        EXPECT_EQ(expected->get_file_filesystem()->expected_modification_time,
                  actual->get_file_filesystem()->expected_modification_time);
      } else if (expected->is_blob()) {
        ASSERT_TRUE(actual->is_blob());
        EXPECT_EQ(expected->get_blob()->length, actual->get_blob()->length);
        EXPECT_EQ(expected->get_blob()->offset, actual->get_blob()->offset);

        base::RunLoop loop;
        String received_uuid;
        actual->get_blob()->blob->GetInternalUUID(base::Bind(
            [](base::Closure quit_closure, String* uuid_out,
               const String& uuid) {
              *uuid_out = uuid;
              quit_closure.Run();
            },
            loop.QuitClosure(), &received_uuid));
        loop.Run();
        EXPECT_EQ(expected_elements[i].blob_uuid, received_uuid);
      }
    }
    mock_blob_registry_.registrations.clear();
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  ScopedMojoBlobsForTest enable_mojo_blobs_;
  ScopedTestingPlatformSupport<MojoBlobTestPlatform, BlobRegistry*>
      testing_platform_;
  MockBlobRegistry mock_blob_registry_;

  // Significantly less than BlobData's kMaxConsolidatedItemSizeInBytes.
  Vector<uint8_t> small_test_data_;
  // Larger than kMaxConsolidatedItemSizeInBytes, but smaller than
  // max_data_population.
  Vector<uint8_t> medium_test_data_;
  // Larger than max_data_population.
  Vector<uint8_t> large_test_data_;
  RefPtr<BlobDataHandle> empty_blob_;
  String empty_blob_uuid_;
  RefPtr<BlobDataHandle> test_blob_;
  String test_blob_uuid_;
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

  TestCreateBlob(std::move(data), {});
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

TEST_F(BlobDataHandleTest, CreateFromEmptyElements) {
  std::unique_ptr<BlobData> data = BlobData::Create();
  data->AppendBytes(small_test_data_.data(), 0);
  data->AppendBlob(empty_blob_, 0, 0);
  data->AppendFile("path", 0, 0, 0.0);
  data->AppendFileSystemURL(KURL(), 0, 0, 0.0);

  TestCreateBlob(std::move(data), {});
}

TEST_F(BlobDataHandleTest, CreateFromSmallBytes) {
  std::unique_ptr<BlobData> data = BlobData::Create();
  data->AppendBytes(small_test_data_.data(), small_test_data_.size());

  Vector<ExpectedElement> expected_elements;
  expected_elements.push_back(ExpectedElement::EmbeddedBytes(small_test_data_));

  TestCreateBlob(std::move(data), std::move(expected_elements));
}

TEST_F(BlobDataHandleTest, CreateFromLargeBytes) {
  std::unique_ptr<BlobData> data = BlobData::Create();
  data->AppendBytes(large_test_data_.data(), large_test_data_.size());

  Vector<ExpectedElement> expected_elements;
  expected_elements.push_back(ExpectedElement::LargeBytes(large_test_data_));

  TestCreateBlob(std::move(data), std::move(expected_elements));
}

TEST_F(BlobDataHandleTest, CreateFromMergedBytes) {
  std::unique_ptr<BlobData> data = BlobData::Create();
  data->AppendBytes(medium_test_data_.data(), medium_test_data_.size());
  data->AppendBytes(small_test_data_.data(), small_test_data_.size());
  EXPECT_EQ(2u, data->Items().size());

  Vector<uint8_t> expected_data = medium_test_data_;
  expected_data.AppendVector(small_test_data_);

  Vector<ExpectedElement> expected_elements;
  expected_elements.push_back(
      ExpectedElement::EmbeddedBytes(std::move(expected_data)));

  TestCreateBlob(std::move(data), std::move(expected_elements));
}

TEST_F(BlobDataHandleTest, CreateFromMergedLargeAndSmallBytes) {
  std::unique_ptr<BlobData> data = BlobData::Create();
  data->AppendBytes(large_test_data_.data(), large_test_data_.size());
  data->AppendBytes(small_test_data_.data(), small_test_data_.size());
  EXPECT_EQ(2u, data->Items().size());

  Vector<uint8_t> expected_data = large_test_data_;
  expected_data.AppendVector(small_test_data_);

  Vector<ExpectedElement> expected_elements;
  expected_elements.push_back(
      ExpectedElement::LargeBytes(std::move(expected_data)));

  TestCreateBlob(std::move(data), std::move(expected_elements));
}

TEST_F(BlobDataHandleTest, CreateFromMergedSmallAndLargeBytes) {
  std::unique_ptr<BlobData> data = BlobData::Create();
  data->AppendBytes(small_test_data_.data(), small_test_data_.size());
  data->AppendBytes(large_test_data_.data(), large_test_data_.size());
  EXPECT_EQ(2u, data->Items().size());

  Vector<uint8_t> expected_data = small_test_data_;
  expected_data.AppendVector(large_test_data_);

  Vector<ExpectedElement> expected_elements;
  expected_elements.push_back(
      ExpectedElement::LargeBytes(std::move(expected_data)));

  TestCreateBlob(std::move(data), std::move(expected_elements));
}

TEST_F(BlobDataHandleTest, CreateFromFileAndFileSystemURL) {
  double timestamp1 = CurrentTime();
  double timestamp2 = timestamp1 + 1;
  KURL url(KURL(), "http://example.com/");
  std::unique_ptr<BlobData> data = BlobData::Create();
  data->AppendFile("path", 4, 32, timestamp1);
  data->AppendFileSystemURL(url, 15, 876, timestamp2);

  Vector<ExpectedElement> expected_elements;
  expected_elements.push_back(
      ExpectedElement::File("path", 4, 32, WTF::Time::FromDoubleT(timestamp1)));
  expected_elements.push_back(ExpectedElement::FileFilesystem(
      url, 15, 876, WTF::Time::FromDoubleT(timestamp2)));

  TestCreateBlob(std::move(data), std::move(expected_elements));
}

TEST_F(BlobDataHandleTest, CreateFromFileWithUnknownSize) {
  Vector<ExpectedElement> expected_elements;
  expected_elements.push_back(
      ExpectedElement::File("path", 0, uint64_t(-1), WTF::Time()));

  TestCreateBlob(BlobData::CreateForFileWithUnknownSize("path"),
                 std::move(expected_elements));
}

TEST_F(BlobDataHandleTest, CreateFromFilesystemFileWithUnknownSize) {
  double timestamp = CurrentTime();
  KURL url(KURL(), "http://example.com/");
  Vector<ExpectedElement> expected_elements;
  expected_elements.push_back(ExpectedElement::FileFilesystem(
      url, 0, uint64_t(-1), WTF::Time::FromDoubleT(timestamp)));

  TestCreateBlob(
      BlobData::CreateForFileSystemURLWithUnknownSize(url, timestamp),
      std::move(expected_elements));
}

TEST_F(BlobDataHandleTest, CreateFromBlob) {
  std::unique_ptr<BlobData> data = BlobData::Create();
  data->AppendBlob(test_blob_, 13, 765);

  Vector<ExpectedElement> expected_elements;
  expected_elements.push_back(ExpectedElement::Blob(test_blob_uuid_, 13, 765));

  TestCreateBlob(std::move(data), std::move(expected_elements));
}

TEST_F(BlobDataHandleTest, CreateFromBlobsAndBytes) {
  std::unique_ptr<BlobData> data = BlobData::Create();
  data->AppendBlob(test_blob_, 10, 10);
  data->AppendBytes(medium_test_data_.data(), medium_test_data_.size());
  data->AppendBlob(test_blob_, 0, 0);
  data->AppendBytes(small_test_data_.data(), small_test_data_.size());
  data->AppendBlob(test_blob_, 0, 10);
  data->AppendBytes(large_test_data_.data(), large_test_data_.size());

  Vector<uint8_t> expected_data = medium_test_data_;
  expected_data.AppendVector(small_test_data_);

  Vector<ExpectedElement> expected_elements;
  expected_elements.push_back(ExpectedElement::Blob(test_blob_uuid_, 10, 10));
  expected_elements.push_back(
      ExpectedElement::EmbeddedBytes(std::move(expected_data)));
  expected_elements.push_back(ExpectedElement::Blob(test_blob_uuid_, 0, 10));
  expected_elements.push_back(ExpectedElement::LargeBytes(large_test_data_));

  TestCreateBlob(std::move(data), std::move(expected_elements));
}

TEST_F(BlobDataHandleTest, CreateFromSmallBytesAfterLargeBytes) {
  std::unique_ptr<BlobData> data = BlobData::Create();
  data->AppendBytes(large_test_data_.data(), large_test_data_.size());
  data->AppendBlob(test_blob_, 0, 10);
  data->AppendBytes(small_test_data_.data(), small_test_data_.size());

  Vector<ExpectedElement> expected_elements;
  expected_elements.push_back(ExpectedElement::LargeBytes(large_test_data_));
  expected_elements.push_back(ExpectedElement::Blob(test_blob_uuid_, 0, 10));
  expected_elements.push_back(ExpectedElement::EmbeddedBytes(small_test_data_));

  TestCreateBlob(std::move(data), std::move(expected_elements));
}

TEST_F(BlobDataHandleTest, CreateFromManyMergedBytes) {
  std::unique_ptr<BlobData> data = BlobData::Create();
  Vector<uint8_t> merged_data;
  while (merged_data.size() <= DataElementBytes::kMaximumEmbeddedDataSize) {
    data->AppendBytes(medium_test_data_.data(), medium_test_data_.size());
    merged_data.AppendVector(medium_test_data_);
  }
  data->AppendBlob(test_blob_, 0, 10);
  data->AppendBytes(medium_test_data_.data(), medium_test_data_.size());

  Vector<ExpectedElement> expected_elements;
  expected_elements.push_back(
      ExpectedElement::LargeBytes(std::move(merged_data)));
  expected_elements.push_back(ExpectedElement::Blob(test_blob_uuid_, 0, 10));
  expected_elements.push_back(
      ExpectedElement::EmbeddedBytes(medium_test_data_));

  TestCreateBlob(std::move(data), std::move(expected_elements));
}

}  // namespace blink
