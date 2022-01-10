// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/record_upload_request_builder.h"

#include <cstdint>
#include <string>
#include "base/logging.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Not;

namespace reporting {
namespace {

constexpr int64_t kGenerationId = 4321;
constexpr Priority kPriority = Priority::IMMEDIATE;

// Default values for EncryptionInfo
constexpr char kEncryptionKey[] = "abcdef";
constexpr int64_t kPublicKeyId = 9876;

// Default values for CompressionInformation.
constexpr CompressionInformation::CompressionAlgorithm kCompressionAlgorithm =
    CompressionInformation::COMPRESSION_SNAPPY;

class RecordUploadRequestBuilderTest : public ::testing::TestWithParam<bool> {
 public:
  RecordUploadRequestBuilderTest() = default;

 protected:
  static EncryptedRecord GenerateEncryptedRecord(
      const base::StringPiece encrypted_wrapped_record,
      const bool set_compression = false) {
    EncryptedRecord record;
    record.set_encrypted_wrapped_record(std::string(encrypted_wrapped_record));

    auto* const sequence_information = record.mutable_sequence_information();
    sequence_information->set_sequencing_id(GetNextSequencingId());
    sequence_information->set_generation_id(kGenerationId);
    sequence_information->set_priority(kPriority);

    auto* const encryption_info = record.mutable_encryption_info();
    encryption_info->set_encryption_key(kEncryptionKey);
    encryption_info->set_public_key_id(kPublicKeyId);

    if (set_compression) {
      auto* const compression_information =
          record.mutable_compression_information();
      compression_information->set_compression_algorithm(kCompressionAlgorithm);
    }

    return record;
  }

  static int64_t GetNextSequencingId() {
    static int64_t sequencing_id = 0;
    return sequencing_id++;
  }

  bool need_encryption_key() const { return GetParam(); }
};

TEST_P(RecordUploadRequestBuilderTest, AcceptEncryptedRecordsList) {
  static constexpr size_t kNumRecords = 10;

  std::vector<EncryptedRecord> records;
  records.reserve(kNumRecords);
  for (size_t counter = 0; counter < kNumRecords; ++counter) {
    records.push_back(GenerateEncryptedRecord(
        base::StrCat({"TEST_INFO_", base::NumberToString(counter)})));
  }

  UploadEncryptedReportingRequestBuilder builder(need_encryption_key());
  for (auto record : records) {
    builder.AddRecord(std::move(record));
  }
  auto request_payload = builder.Build();
  ASSERT_TRUE(request_payload.has_value());
  ASSERT_TRUE(request_payload.value().is_dict());
  const auto attach_encryption_settings = request_payload.value().FindBoolKey(
      UploadEncryptedReportingRequestBuilder::
          GetAttachEncryptionSettingsPath());
  EXPECT_EQ(need_encryption_key(), attach_encryption_settings.has_value());
  base::Value* const record_list = request_payload.value().FindListKey(
      UploadEncryptedReportingRequestBuilder::GetEncryptedRecordListPath());
  ASSERT_TRUE(record_list);
  ASSERT_TRUE(record_list->is_list());
  EXPECT_EQ(record_list->GetList().size(), records.size());

  size_t counter = 0;
  for (auto record : records) {
    auto record_value_result =
        EncryptedRecordDictionaryBuilder(std::move(record)).Build();
    ASSERT_TRUE(record_value_result.has_value());
    EXPECT_EQ(record_list->GetList()[counter++], record_value_result.value());
  }
}

TEST_P(RecordUploadRequestBuilderTest, BreakListOnSingleBadRecord) {
  static constexpr size_t kNumRecords = 10;

  std::vector<EncryptedRecord> records;
  records.reserve(kNumRecords);
  for (size_t counter = 0; counter < kNumRecords; ++counter) {
    records.push_back(GenerateEncryptedRecord(
        base::StrCat({"TEST_INFO_", base::NumberToString(counter)})));
  }
  // Corrupt one record.
  records[kNumRecords - 2]
      .mutable_sequence_information()
      ->clear_generation_id();

  UploadEncryptedReportingRequestBuilder builder(need_encryption_key());
  for (auto record : records) {
    builder.AddRecord((std::move(record)));
  }
  auto request_payload = builder.Build();
  ASSERT_FALSE(request_payload.has_value()) << request_payload.value();
}

TEST_P(RecordUploadRequestBuilderTest, DenyPoorlyFormedEncryptedRecords) {
  // Reject empty record.
  EncryptedRecord record;

  EXPECT_FALSE(EncryptedRecordDictionaryBuilder(record).Build().has_value());

  // Reject encrypted_wrapped_record without sequence information.
  record.set_encrypted_wrapped_record("Enterprise");

  EXPECT_FALSE(EncryptedRecordDictionaryBuilder(record).Build().has_value());

  // Reject incorrectly set sequence information by only setting sequencing id.
  auto* sequence_information = record.mutable_sequence_information();
  sequence_information->set_sequencing_id(1701);

  EXPECT_FALSE(EncryptedRecordDictionaryBuilder(record).Build().has_value());

  // Finish correctly setting sequence information but incorrectly set
  // encryption info.
  sequence_information->set_generation_id(12345678);
  sequence_information->set_priority(IMMEDIATE);

  auto* encryption_info = record.mutable_encryption_info();
  encryption_info->set_encryption_key("Key");

  EXPECT_FALSE(EncryptedRecordDictionaryBuilder(record).Build().has_value());

  // Finish correctly setting encryption info - expect complete call.
  encryption_info->set_public_key_id(1234);

  EXPECT_TRUE(EncryptedRecordDictionaryBuilder(record).Build().has_value());
}

TEST_P(RecordUploadRequestBuilderTest,
       DontBuildCompressionRequestIfNoInformation) {
  EncryptedRecord compressionless_record = GenerateEncryptedRecord("TEST_INFO");
  EXPECT_FALSE(compressionless_record.has_compression_information());

  absl::optional<base::Value> compressionless_payload =
      EncryptedRecordDictionaryBuilder(std::move(compressionless_record))
          .Build();
  DCHECK(compressionless_payload.has_value());

  EXPECT_FALSE(compressionless_payload.value().FindKey(
      EncryptedRecordDictionaryBuilder::GetCompressionInformationPath()));

  EncryptedRecord compressed_record =
      GenerateEncryptedRecord("TEST_INFO", true);
  EXPECT_TRUE(compressed_record.has_compression_information());

  absl::optional<base::Value> compressed_record_payload =
      EncryptedRecordDictionaryBuilder(std::move(compressed_record)).Build();
  DCHECK(compressed_record_payload.has_value());

  EXPECT_TRUE(compressed_record_payload.value().FindKey(
      EncryptedRecordDictionaryBuilder::GetCompressionInformationPath()));
}

INSTANTIATE_TEST_SUITE_P(NeedOrNoNeedKey,
                         RecordUploadRequestBuilderTest,
                         testing::Bool());

}  // namespace
}  // namespace reporting
