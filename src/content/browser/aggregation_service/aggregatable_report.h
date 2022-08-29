// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATABLE_REPORT_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATABLE_REPORT_H_

#include <stddef.h>
#include <stdint.h>

#include <ostream>
#include <string>
#include <vector>

#include "base/guid.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/aggregation_service/public_key.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class AggregatableReportRequest;

// The underlying private information which will be sent to the processing
// servers for aggregation. Each payload encodes a single contribution to a
// histogram bucket. This will be encrypted and won't be readable by the
// reporting endpoint.
struct CONTENT_EXPORT AggregationServicePayloadContents {
  // TODO(alexmt): Add kDistinctCount option.
  enum class Operation {
    kHistogram,
  };

  // Corresponds to the 'alternative aggregation mode' optional setting, but
  // also includes the default option (if no alternative is set).
  enum class AggregationMode {
    // Uses a server-side Trusted Execution Environment (TEE) to process the
    // encrypted payloads, see
    // https://github.com/WICG/attribution-reporting-api/blob/main/AGGREGATION_SERVICE_TEE.md.
    kTeeBased,

    // Implements a protocol similar to poplar VDAF in the PPM Framework, see
    // https://github.com/WICG/attribution-reporting-api/blob/main/AGGREGATE.md#choosing-among-aggregation-services.
    kExperimentalPoplar,

    kDefault = kTeeBased,
  };

  struct HistogramContribution {
    absl::uint128 bucket;
    int value;
  };

  AggregationServicePayloadContents(
      Operation operation,
      std::vector<HistogramContribution> contributions,
      AggregationMode aggregation_mode);

  AggregationServicePayloadContents(
      const AggregationServicePayloadContents& other);
  AggregationServicePayloadContents& operator=(
      const AggregationServicePayloadContents& other);
  AggregationServicePayloadContents(AggregationServicePayloadContents&& other);
  AggregationServicePayloadContents& operator=(
      AggregationServicePayloadContents&& other);
  ~AggregationServicePayloadContents();

  Operation operation;
  std::vector<HistogramContribution> contributions;
  AggregationMode aggregation_mode;
};

// Represents the information that will be provided to both the reporting
// endpoint and the processing server(s), i.e. stored in the encrypted payload
// and in the plaintext report.
struct CONTENT_EXPORT AggregatableReportSharedInfo {
  enum class DebugMode {
    kDisabled,
    kEnabled,
  };

  AggregatableReportSharedInfo(base::Time scheduled_report_time,
                               base::GUID report_id,
                               url::Origin reporting_origin,
                               DebugMode debug_mode,
                               base::Value::Dict additional_fields,
                               std::string api_version,
                               std::string api_identifier);

  AggregatableReportSharedInfo(const AggregatableReportSharedInfo& other) =
      delete;
  AggregatableReportSharedInfo& operator=(
      const AggregatableReportSharedInfo& other) = delete;
  AggregatableReportSharedInfo(AggregatableReportSharedInfo&& other);
  AggregatableReportSharedInfo& operator=(AggregatableReportSharedInfo&& other);
  ~AggregatableReportSharedInfo();

  // Creates a deep copy of this object.
  AggregatableReportSharedInfo Clone() const;

  // Serializes to a JSON dictionary, represented as a string.
  std::string SerializeAsJson() const;

  base::Time scheduled_report_time;
  base::GUID report_id;  // Used to prevent double counting.
  url::Origin reporting_origin;
  DebugMode debug_mode;
  base::Value::Dict additional_fields;
  std::string api_version;

  // Enum string that indicates which API created the report.
  std::string api_identifier;
};

// An AggregatableReport contains all the information needed for sending the
// report to its reporting endpoint. All nested information has already been
// serialized and encrypted as necessary.
class CONTENT_EXPORT AggregatableReport {
 public:
  // This is used to encapsulate the data that is specific to a single
  // processing server.
  struct CONTENT_EXPORT AggregationServicePayload {
    AggregationServicePayload(
        std::vector<uint8_t> payload,
        std::string key_id,
        absl::optional<std::vector<uint8_t>> debug_cleartext_payload);
    AggregationServicePayload(const AggregationServicePayload& other);
    AggregationServicePayload& operator=(
        const AggregationServicePayload& other);
    AggregationServicePayload(AggregationServicePayload&& other);
    AggregationServicePayload& operator=(AggregationServicePayload&& other);
    ~AggregationServicePayload();

    // This payload is constructed using the data in the
    // AggregationServicePayloadContents and then encrypted with one of
    // `url`'s public keys. For the `kTeeBased` aggregation mode, the plaintext
    // of the encrypted payload is a serialized CBOR map structured as follows:
    // {
    //   "operation": "<chosen operation as string>",
    //   "data": [{
    //     "bucket": <a 16-byte (i.e. 128-bit) big-endian bytestring>,
    //     "value": <a 4-byte (i.e. 32-bit) big-endian bytestring>
    //   }, ...],
    // }
    // Note that the "data" array may contain multiple contributions.
    // For the `kExperimentalPoplar` aggregation mode, the "data" field is
    // replaced with:
    //   "dpf_key": <binary serialization of the DPF key>
    std::vector<uint8_t> payload;

    // Indicates the chosen encryption key.
    std::string key_id;

    // If the request's shared info had a `kEnabled` debug_mode, contains the
    // cleartext payload for debugging. Otherwise, it is `absl::nullopt`.
    absl::optional<std::vector<uint8_t>> debug_cleartext_payload;
  };

  // Used to allow mocking `CreateFromRequestAndPublicKeys()` in tests.
  class CONTENT_EXPORT Provider {
   public:
    virtual ~Provider();

    // Processes and serializes the information in `report_request` and encrypts
    // using the `public_keys` as necessary. The order of `public_keys` should
    // correspond to `report_request.processing_urls`, which should be
    // sorted. Returns `absl::nullopt` if an error occurred during construction.
    virtual absl::optional<AggregatableReport> CreateFromRequestAndPublicKeys(
        AggregatableReportRequest report_request,
        std::vector<PublicKey> public_keys) const;

    // Sets whether to disable encryption of the payload(s). Should only be used
    // by the AggregationServiceTool.
    static void SetDisableEncryptionForTestingTool(bool should_disable);

   private:
    static bool g_disable_encryption_for_testing_tool_;
  };

  // log_2 of the number of buckets
  static constexpr size_t kBucketDomainBitLength = 32;

  // log_2 of the value output space
  static constexpr size_t kValueDomainBitLength = 64;

  // Used as a prefix for the authenticated information (i.e. context info).
  // This value must not be reused for new protocols or versions of this
  // protocol unless the ciphertexts are intended to be compatible. This ensures
  // that, even if public keys are reused, the same ciphertext cannot be (i.e.
  // no cross-protocol attacks).
  static constexpr base::StringPiece kDomainSeparationPrefix =
      "aggregation_service";

  AggregatableReport(std::vector<AggregationServicePayload> payloads,
                     std::string shared_info);
  AggregatableReport(const AggregatableReport& other);
  AggregatableReport& operator=(const AggregatableReport& other);
  AggregatableReport(AggregatableReport&& other);
  AggregatableReport& operator=(AggregatableReport&& other);
  ~AggregatableReport();

  const std::vector<AggregationServicePayload>& payloads() const {
    return payloads_;
  }
  const std::string& shared_info() const { return shared_info_; }

  // Returns the JSON representation of this report of the form
  // {
  //   "shared_info": "<shared_info>",
  //   "aggregation_service_payloads": [
  //     {
  //       "payload": "<base64 encoded encrypted data>",
  //       "key_id": "<string identifying public key used>"
  //     },
  //     {
  //       "payload": "<base64 encoded encrypted data>",
  //       "key_id": "<string identifying public key used>"
  //     }
  //   ]
  // }
  //
  // Where <shared_info> is the serialization of the JSON (with all whitespace
  // removed):
  // {
  //   "report_id":"[UUID]",
  //   "reporting_origin":"https://reporter.example"
  //   "scheduled_report_time":"[timestamp in seconds]",
  //   "version":"[api version]",
  // }
  // Callers may insert API-specific fields into the shared_info dictionary.
  // In those cases, the keys are inserted in lexicographic order.
  //
  // If requested, each "aggregation_service_payloads" element has an extra
  // field: `"debug_cleartext_payload": "<base64 encoded payload cleartext>"`.
  // Note that APIs may wish to add additional key-value pairs to this returned
  // value.
  base::Value::Dict GetAsJson() const;

  // TODO(crbug.com/1247409): Expose static method to validate that a
  // base::Value appears to represent a valid report.

  // Returns whether `number` is a valid number of processing URLs for the
  // `aggregation_mode`.
  static bool IsNumberOfProcessingUrlsValid(
      size_t number,
      AggregationServicePayloadContents::AggregationMode aggregation_mode);

  // Returns whether `number` is a valid number of histogram contributions for
  // the `aggregation_mode`.
  static bool IsNumberOfHistogramContributionsValid(
      size_t number,
      AggregationServicePayloadContents::AggregationMode aggregation_mode);

 private:
  // This vector should have an entry for each processing URL specified in
  // the original AggregatableReportRequest.
  std::vector<AggregationServicePayload> payloads_;

  std::string shared_info_;
};

// Represents a request for an AggregatableReport. Contains all the data
// necessary to construct the report except for the PublicKey for each
// processing URL.
class CONTENT_EXPORT AggregatableReportRequest {
 public:
  // Returns `absl::nullopt` if `payload_contents.contributions.size()` is not
  // valid for the `payload_contents.aggregation_mode` (see
  // `IsNumberOfHistogramContributionsValid()` above). Also returns
  // `absl::nullopt` if any contribution has a negative value or if
  // `shared_info.report_id` is not valid.
  static absl::optional<AggregatableReportRequest> Create(
      AggregationServicePayloadContents payload_contents,
      AggregatableReportSharedInfo shared_info);

  // Returns `absl::nullopt` if `payload_contents.contributions.size()` or
  // `processing_url.size()` is not valid for the
  // `payload_contents.aggregation_mode` (see
  // `IsNumberOfHistogramContributionsValid()` and
  // `IsNumberOfProcessingUrlsValid`, respectively). Also returns
  // `absl::nullopt` if any contribution has a negative value or if
  // `shared_info.report_id` is not valid.
  static absl::optional<AggregatableReportRequest> CreateForTesting(
      std::vector<GURL> processing_urls,
      AggregationServicePayloadContents payload_contents,
      AggregatableReportSharedInfo shared_info);

  // Move-only.
  AggregatableReportRequest(AggregatableReportRequest&& other);
  AggregatableReportRequest& operator=(AggregatableReportRequest&& other);
  ~AggregatableReportRequest();

  const std::vector<GURL>& processing_urls() const { return processing_urls_; }
  const AggregationServicePayloadContents& payload_contents() const {
    return payload_contents_;
  }
  const AggregatableReportSharedInfo& shared_info() const {
    return shared_info_;
  }

 private:
  static absl::optional<AggregatableReportRequest> CreateInternal(
      std::vector<GURL> processing_urls,
      AggregationServicePayloadContents payload_contents,
      AggregatableReportSharedInfo shared_info);

  AggregatableReportRequest(std::vector<GURL> processing_urls,
                            AggregationServicePayloadContents payload_contents,
                            AggregatableReportSharedInfo shared_info);

  std::vector<GURL> processing_urls_;
  AggregationServicePayloadContents payload_contents_;
  AggregatableReportSharedInfo shared_info_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATABLE_REPORT_H_
