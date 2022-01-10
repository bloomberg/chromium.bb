// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/record_handler_impl.h"

#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/upload/dm_server_upload_service.h"
#include "chrome/browser/policy/messaging_layer/upload/record_upload_request_builder.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/task_runner_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {
namespace {

// Priority could come back as an int or as a std::string, this function handles
// both situations.
absl::optional<Priority> GetPriorityProtoFromSequenceInformationValue(
    const base::Value& sequence_information) {
  const absl::optional<int> int_priority_result =
      sequence_information.FindIntKey("priority");
  if (int_priority_result.has_value()) {
    return Priority(int_priority_result.value());
  }

  const std::string* str_priority_result =
      sequence_information.FindStringKey("priority");
  if (!str_priority_result) {
    LOG(ERROR) << "Field priority is missing from SequenceInformation: "
               << sequence_information;
    return absl::nullopt;
  }

  Priority priority;
  if (!Priority_Parse(*str_priority_result, &priority)) {
    LOG(ERROR) << "Unable to parse field priority in SequenceInformation: "
               << sequence_information;
    return absl::nullopt;
  }
  return priority;
}
}  // namespace

// ReportUploader handles enqueuing events on the |report_queue_|,
// and uploading those events with the |client_|.
class RecordHandlerImpl::ReportUploader
    : public TaskRunnerContext<DmServerUploadService::CompletionResponse> {
 public:
  ReportUploader(
      bool need_encryption_key,
      std::unique_ptr<std::vector<EncryptedRecord>> records,
      policy::CloudPolicyClient* client,
      DmServerUploadService::CompletionCallback upload_complete_cb,
      DmServerUploadService::EncryptionKeyAttachedCallback
          encryption_key_attached_cb,
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);

 private:
  ~ReportUploader() override;

  void OnStart() override;

  void StartUpload();
  void OnUploadComplete(absl::optional<base::Value> response);
  void HandleFailedUpload();
  void HandleSuccessfulUpload();
  void Complete(DmServerUploadService::CompletionResponse result);

  // Returns a gap record if it is necessary. Expects the contents of the
  // failedUploadedRecord field in the response:
  // {
  //   "sequencingId": 1234
  //   "generationId": 4321
  //   "priority": 3
  // }
  absl::optional<EncryptedRecord> HandleFailedUploadedSequenceInformation(
      const base::Value& sequence_information);

  // Helper function for converting a base::Value representation of
  // SequenceInformation into a proto. Will return an INVALID_ARGUMENT error
  // if the base::Value is not convertible.
  StatusOr<SequenceInformation> SequenceInformationValueToProto(
      const base::Value& value);

  bool need_encryption_key_;
  std::unique_ptr<std::vector<EncryptedRecord>> records_;
  raw_ptr<policy::CloudPolicyClient> client_;

  // Encryption key delivery callback.
  DmServerUploadService::EncryptionKeyAttachedCallback
      encryption_key_attached_cb_;

  // Last successful response to be processed.
  // Note: I could not find a way to pass it as a parameter,
  // so it is a class member variable. |last_response_| must be processed before
  // any attempt to retry calling the client, otherwise it will be overwritten.
  base::Value last_response_;

  // When a record fails to be processed on the server, |ReportUploader| creates
  // a gap record to upload in its place.
  EncryptedRecord gap_record_;

  // Set for the highest record being uploaded.
  absl::optional<SequenceInformation> highest_sequence_information_;

  // Set to |true| if force_confirm flag is present. |false| by default.
  bool force_confirm_{false};
};

RecordHandlerImpl::ReportUploader::ReportUploader(
    bool need_encryption_key,
    std::unique_ptr<std::vector<EncryptedRecord>> records,
    policy::CloudPolicyClient* client,
    DmServerUploadService::CompletionCallback client_cb,
    DmServerUploadService::EncryptionKeyAttachedCallback
        encryption_key_attached_cb,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
    : TaskRunnerContext<DmServerUploadService::CompletionResponse>(
          std::move(client_cb),
          sequenced_task_runner),
      need_encryption_key_(need_encryption_key),
      records_(std::move(records)),
      client_(client),
      encryption_key_attached_cb_(std::move(encryption_key_attached_cb)) {}

RecordHandlerImpl::ReportUploader::~ReportUploader() = default;

void RecordHandlerImpl::ReportUploader::OnStart() {
  if (client_ == nullptr) {
    Status null_client = Status(error::INVALID_ARGUMENT, "Client was null");
    LOG(ERROR) << null_client;
    Complete(null_client);
    return;
  }

  if (records_ == nullptr) {
    Status null_records = Status(error::INVALID_ARGUMENT, "records_ was null");
    LOG(ERROR) << null_records;
    Complete(null_records);
    return;
  }

  if (records_->empty() && !need_encryption_key_) {
    Status empty_records =
        Status(error::INVALID_ARGUMENT, "records_ was empty");
    LOG(ERROR) << empty_records;
    Complete(empty_records);
    return;
  }

  StartUpload();
}

void RecordHandlerImpl::ReportUploader::StartUpload() {
  auto response_cb =
      base::BindOnce(&RecordHandlerImpl::ReportUploader::OnUploadComplete,
                     base::Unretained(this));

  UploadEncryptedReportingRequestBuilder request_builder{need_encryption_key_};
  for (auto record : *records_) {
    request_builder.AddRecord((std::move(record)));
  }
  auto request_result = request_builder.Build();
  if (!request_result.has_value()) {
    std::move(response_cb).Run(absl::nullopt);
    return;
  }

  // Records have been captured in the request, safe to clear the vector.
  records_->clear();

  base::Value request = std::move(request_result.value());

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](policy::CloudPolicyClient* client, base::Value request,
             base::OnceCallback<void(absl::optional<base::Value>)>
                 response_cb) {
            client->UploadEncryptedReport(
                std::move(request),
                reporting::GetContext(ProfileManager::GetPrimaryUserProfile()),
                std::move(response_cb));
          },
          client_, std::move(request), std::move(response_cb)));
}

void RecordHandlerImpl::ReportUploader::OnUploadComplete(
    absl::optional<base::Value> response) {
  if (!response.has_value() || !response.value().is_dict()) {
    Schedule(&RecordHandlerImpl::ReportUploader::HandleFailedUpload,
             base::Unretained(this));
    return;
  }
  last_response_ = std::move(response.value());
  Schedule(&RecordHandlerImpl::ReportUploader::HandleSuccessfulUpload,
           base::Unretained(this));
}

void RecordHandlerImpl::ReportUploader::HandleFailedUpload() {
  if (highest_sequence_information_.has_value()) {
    Complete(DmServerUploadService::SuccessfulUploadResponse{
        .sequence_information =
            std::move(highest_sequence_information_.value()),
        .force_confirm = force_confirm_});
    return;
  }

  Complete(Status(error::INTERNAL, "Unable to upload any records"));
}

void RecordHandlerImpl::ReportUploader::HandleSuccessfulUpload() {
  DCHECK(last_response_.is_dict());
  //  {
  //    "lastSucceedUploadedRecord": ... // SequenceInformation proto
  //    "firstFailedUploadedRecord": {
  //      "failedUploadedRecord": ... // SequenceInformation proto
  //      "failureStatus": ... // Status proto
  //    }
  //    "forceConfirm": true  // if present, flag that lastSucceedUploadedRecord
  //                          // is to be accepted unconditionally by client
  //    "encryptionSettings": ... // EncryptionSettings proto
  //  }
  const base::Value* last_succeed_uploaded_record =
      last_response_.FindDictKey("lastSucceedUploadedRecord");
  if (last_succeed_uploaded_record != nullptr) {
    auto seq_info_result =
        SequenceInformationValueToProto(*last_succeed_uploaded_record);
    if (seq_info_result.ok()) {
      highest_sequence_information_ = std::move(seq_info_result.ValueOrDie());
    } else {
      LOG(ERROR) << "Server responded with an invalid SequenceInformation "
                    "for lastSucceedUploadedRecord:"
                 << *last_succeed_uploaded_record;
    }
  }

  // Handle forceConfirm flag, if present.
  const auto force_confirm_flag = last_response_.FindBoolKey("forceConfirm");
  if (force_confirm_flag.has_value() && force_confirm_flag.value()) {
    force_confirm_ = true;
  }

  // Handle the encryption settings.
  // Note: server can attach it to response regardless of whether
  // the response indicates success or failure, and whether the client
  // set attach_encryption_settings to true in request.
  const base::Value* signed_encryption_key_record =
      last_response_.FindDictKey("encryptionSettings");
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
      std::move(encryption_key_attached_cb_).Run(signed_encryption_key);
      need_encryption_key_ = false;
    }
  }

  // Check if a record was unprocessable on the server.
  const base::Value* failed_uploaded_record = last_response_.FindDictPath(
      "firstFailedUploadedRecord.failedUploadedRecord");
  if (!force_confirm_ && failed_uploaded_record != nullptr) {
    // The record we uploaded previously was unprocessable by the server, if the
    // record was after the current |highest_sequence_information_| we should
    // return a gap record. A gap record consists of an EncryptedRecord with
    // just SequenceInformation. The server will report success for the gap
    // record and |highest_sequence_information_| will be updated in the next
    // response. In the future there may be recoverable |failureStatus|, but
    // for now all the device can do is delete the record.
    auto gap_record_result =
        HandleFailedUploadedSequenceInformation(*failed_uploaded_record);
    if (gap_record_result.has_value()) {
      LOG(ERROR) << "Data Loss. Record was unprocessable by the server: "
                 << *failed_uploaded_record;
      records_->push_back(std::move(gap_record_result.value()));
    }
  }

  if (!records_->empty()) {
    // Upload the next record but do not request encryption key again.
    StartUpload();
    return;
  }

  // No more records to process. Return the highest_sequence_information_ if
  // available.
  if (highest_sequence_information_.has_value()) {
    Complete(DmServerUploadService::SuccessfulUploadResponse{
        .sequence_information =
            std::move(highest_sequence_information_.value()),
        .force_confirm = force_confirm_});
    return;
  }

  Complete(Status(error::INTERNAL, "Unable to upload any records"));
}

absl::optional<EncryptedRecord>
RecordHandlerImpl::ReportUploader::HandleFailedUploadedSequenceInformation(
    const base::Value& sequence_information) {
  if (!highest_sequence_information_.has_value()) {
    LOG(ERROR) << "highest_sequence_information_ has no value.";
    return absl::nullopt;
  }

  auto seq_info_result = SequenceInformationValueToProto(sequence_information);
  if (!seq_info_result.ok()) {
    LOG(ERROR) << "Server responded with an invalid SequenceInformation for "
                  "firstFailedUploadedRecord.failedUploadedRecord:"
               << sequence_information;
    return absl::nullopt;
  }

  SequenceInformation& seq_info = seq_info_result.ValueOrDie();

  // |seq_info| should be of the same generation and priority as
  // highest_sequence_information_, and have the next sequencing_id.
  if (seq_info.generation_id() !=
          highest_sequence_information_->generation_id() ||
      seq_info.priority() != highest_sequence_information_->priority() ||
      seq_info.sequencing_id() !=
          highest_sequence_information_->sequencing_id() + 1) {
    return absl::nullopt;
  }

  // Build a gap record and return it.
  EncryptedRecord encrypted_record;
  *encrypted_record.mutable_sequence_information() = std::move(seq_info);
  return encrypted_record;
}

void RecordHandlerImpl::ReportUploader::Complete(
    DmServerUploadService::CompletionResponse completion_result) {
  Schedule(&RecordHandlerImpl::ReportUploader::Response, base::Unretained(this),
           completion_result);
}

StatusOr<SequenceInformation>
RecordHandlerImpl::ReportUploader::SequenceInformationValueToProto(
    const base::Value& value) {
  const std::string* sequencing_id = value.FindStringKey("sequencingId");
  const std::string* generation_id = value.FindStringKey("generationId");
  const auto priority_result =
      GetPriorityProtoFromSequenceInformationValue(value);

  // If any of the previous values don't exist, or are malformed, return error.
  int64_t seq_id;
  int64_t gen_id;
  if (!sequencing_id || !generation_id || generation_id->empty() ||
      !priority_result.has_value() ||
      !Priority_IsValid(priority_result.value())) {
    return Status(error::INVALID_ARGUMENT,
                  base::StrCat({"Provided value lacks some fields required by "
                                "SequenceInformation proto: ",
                                value.DebugString()}));
  }

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

RecordHandlerImpl::RecordHandlerImpl(policy::CloudPolicyClient* client)
    : RecordHandler(client),
      sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})) {}

RecordHandlerImpl::~RecordHandlerImpl() = default;

void RecordHandlerImpl::HandleRecords(
    bool need_encryption_key,
    std::unique_ptr<std::vector<EncryptedRecord>> records,
    DmServerUploadService::CompletionCallback upload_complete_cb,
    DmServerUploadService::EncryptionKeyAttachedCallback
        encryption_key_attached_cb) {
  Start<RecordHandlerImpl::ReportUploader>(
      need_encryption_key, std::move(records), GetClient(),
      std::move(upload_complete_cb), std::move(encryption_key_attached_cb),
      sequenced_task_runner_);
}

}  // namespace reporting
