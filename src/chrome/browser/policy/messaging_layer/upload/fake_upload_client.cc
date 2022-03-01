// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/fake_upload_client.h"

#include <utility>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/policy/messaging_layer/upload/record_upload_request_builder.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {
namespace {

absl::optional<Priority> GetPriorityFromSequenceInformationValue(
    const base::Value& sequence_information) {
  const absl::optional<int> priority_result =
      sequence_information.FindIntKey("priority");
  if (!priority_result.has_value() ||
      !Priority_IsValid(priority_result.value())) {
    return absl::nullopt;
  }
  return Priority(priority_result.value());
}

StatusOr<SequenceInformation> SequenceInformationValueToProto(
    const base::Value& value) {
  const std::string* const sequencing_id = value.FindStringKey("sequencingId");
  const std::string* const generation_id = value.FindStringKey("generationId");
  const auto priority_result = GetPriorityFromSequenceInformationValue(value);

  // If any of the previous values don't exist, or are malformed, return error.
  if (!sequencing_id || generation_id->empty() || !generation_id ||
      generation_id->empty() || !priority_result.has_value() ||
      !Priority_IsValid(priority_result.value())) {
    return Status(error::INVALID_ARGUMENT,
                  base::StrCat({"Provided value lacks some fields required by "
                                "SequenceInformation proto: ",
                                value.DebugString()}));
  }

  int64_t seq_id;
  int64_t gen_id;
  if (!base::StringToInt64(*sequencing_id, &seq_id) ||
      !base::StringToInt64(*generation_id, &gen_id) || gen_id == 0) {
    // For backwards compatibility accept unsigned values if signed are not
    // parsed.
    // TODO(b/177677467): Remove this duplication once server is fully
    // transitioned.
    uint64_t unsigned_seq_id;
    uint64_t unsigned_gen_id;
    if (!base::StringToUint64(*sequencing_id, &unsigned_seq_id) ||
        !base::StringToUint64(*generation_id, &unsigned_gen_id) ||
        unsigned_gen_id == 0) {
      return Status(error::INVALID_ARGUMENT,
                    base::StrCat({"Provided value did not conform to a valid "
                                  "SequenceInformation proto: ",
                                  value.DebugString()}));
    }
    seq_id = static_cast<int64_t>(unsigned_seq_id);
    gen_id = static_cast<int64_t>(unsigned_gen_id);
  }

  SequenceInformation proto;
  proto.set_sequencing_id(seq_id);
  proto.set_generation_id(gen_id);
  proto.set_priority(Priority(priority_result.value()));
  return proto;
}

}  // namespace

FakeUploadClient::FakeUploadClient(
    policy::CloudPolicyClient* cloud_policy_client)
    : cloud_policy_client_(cloud_policy_client) {}

FakeUploadClient::~FakeUploadClient() = default;

void FakeUploadClient::Create(policy::CloudPolicyClient* cloud_policy_client,
                              CreatedCallback created_cb) {
  std::move(created_cb)
      .Run(base::WrapUnique<UploadClient>(
          new FakeUploadClient(cloud_policy_client)));
}

Status FakeUploadClient::EnqueueUpload(
    bool need_encryption_key,
    std::unique_ptr<std::vector<EncryptedRecord>> records,
    ReportSuccessfulUploadCallback report_upload_success_cb,
    EncryptionKeyAttachedCallback encryption_key_attached_cb) {
  UploadEncryptedReportingRequestBuilder builder;
  for (auto record : *records) {
    builder.AddRecord((std::move(record)));
  }
  auto request_result = builder.Build();
  if (!request_result.has_value()) {
    // While it might seem that this should return a bad status, the actual
    // UploadClient would return OK here as the records are processed at a later
    // time, and any failures are expected to be handled elsewhere.
    return Status::StatusOK();
  }

  auto response_cb = base::BindOnce(&FakeUploadClient::OnUploadComplete,
                                    base::Unretained(this),
                                    std::move(report_upload_success_cb),
                                    std::move(encryption_key_attached_cb));

  cloud_policy_client_->UploadEncryptedReport(
      std::move(request_result.value()),
      base::Value(base::Value::Type::DICTIONARY), std::move(response_cb));
  return Status::StatusOK();
}

void FakeUploadClient::OnUploadComplete(
    ReportSuccessfulUploadCallback report_upload_success_cb,
    EncryptionKeyAttachedCallback encryption_key_attached_cb,
    absl::optional<base::Value> response) {
  if (!response.has_value()) {
    return;
  }
  const base::Value* last_success =
      response->FindDictKey("lastSucceedUploadedRecord");
  if (last_success != nullptr) {
    const auto force_confirm_flag = last_success->FindBoolKey("forceConfirm");
    bool force_confirm =
        force_confirm_flag.has_value() && force_confirm_flag.value();
    auto seq_info_result = SequenceInformationValueToProto(*last_success);
    if (seq_info_result.ok()) {
      std::move(report_upload_success_cb)
          .Run(seq_info_result.ValueOrDie(), force_confirm);
    }
  }

  const base::Value* signed_encryption_key_record =
      response->FindDictKey("encryptionSettings");
  if (signed_encryption_key_record != nullptr) {
    const std::string* public_key_str =
        signed_encryption_key_record->FindStringKey("publicKey");
    const auto public_key_id_result =
        signed_encryption_key_record->FindIntKey("publicKeyId");
    const std::string* public_key_signature_str =
        signed_encryption_key_record->FindStringKey("publicKeySignature");
    std::string public_key;
    std::string public_key_signature;
    if (public_key_str != nullptr &&
        base::Base64Decode(*public_key_str, &public_key) &&
        public_key_signature_str != nullptr &&
        base::Base64Decode(*public_key_signature_str, &public_key_signature) &&
        public_key_id_result.has_value()) {
      SignedEncryptionInfo signed_encryption_key;
      signed_encryption_key.set_public_asymmetric_key(public_key);
      signed_encryption_key.set_public_key_id(public_key_id_result.value());
      signed_encryption_key.set_signature(public_key_signature);
      std::move(encryption_key_attached_cb).Run(signed_encryption_key);
    }
  }
}

}  // namespace reporting
