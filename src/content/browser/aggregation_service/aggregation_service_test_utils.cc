// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_test_utils.h"

#include <stddef.h>
#include <stdint.h>

#include <ostream>
#include <string>
#include <tuple>
#include <vector>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/aggregation_service/aggregation_service_storage_sql.h"
#include "content/browser/aggregation_service/public_key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/hpke.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace aggregation_service {

testing::AssertionResult PublicKeysEqual(const std::vector<PublicKey>& expected,
                                         const std::vector<PublicKey>& actual) {
  const auto tie = [](const PublicKey& key) {
    return std::make_tuple(key.id, key.key);
  };

  if (expected.size() != actual.size()) {
    return testing::AssertionFailure() << "Expected length " << expected.size()
                                       << ", actual: " << actual.size();
  }

  for (size_t i = 0; i < expected.size(); i++) {
    if (tie(expected[i]) != tie(actual[i])) {
      return testing::AssertionFailure()
             << "Expected " << expected[i] << " at index " << i
             << ", actual: " << actual[i];
    }
  }

  return testing::AssertionSuccess();
}

using AggregationServicePayload = AggregatableReport::AggregationServicePayload;

testing::AssertionResult AggregatableReportsEqual(
    const AggregatableReport& expected,
    const AggregatableReport& actual) {
  if (expected.payloads().size() != actual.payloads().size()) {
    return testing::AssertionFailure()
           << "Expected payloads size " << expected.payloads().size()
           << ", actual: " << actual.payloads().size();
  }

  for (size_t i = 0; i < expected.payloads().size(); ++i) {
    const AggregationServicePayload& expected_payload = expected.payloads()[i];
    const AggregationServicePayload& actual_payload = actual.payloads()[i];

    if (expected_payload.origin != actual_payload.origin) {
      return testing::AssertionFailure()
             << "Expected origin " << expected_payload.origin
             << " at payload index " << i
             << ", actual: " << actual_payload.origin;
    }

    if (expected_payload.payload != actual_payload.payload) {
      return testing::AssertionFailure()
             << "Expected payloads at payload index " << i << " to match";
    }

    if (expected_payload.key_id != actual_payload.key_id) {
      return testing::AssertionFailure()
             << "Expected key_id " << expected_payload.key_id
             << " at payload index " << i
             << ", actual: " << actual_payload.key_id;
    }
  }

  return SharedInfoEqual(expected.shared_info(), actual.shared_info());
}

testing::AssertionResult ReportRequestsEqual(
    const AggregatableReportRequest& expected,
    const AggregatableReportRequest& actual) {
  if (expected.processing_origins().size() !=
      actual.processing_origins().size()) {
    return testing::AssertionFailure()
           << "Expected processing_origins size "
           << expected.processing_origins().size()
           << ", actual: " << actual.processing_origins().size();
  }
  for (size_t i = 0; i < expected.processing_origins().size(); ++i) {
    if (expected.processing_origins()[i] != actual.processing_origins()[i]) {
      return testing::AssertionFailure()
             << "Expected processing_origins()[" << i << "] to be "
             << expected.processing_origins()[i]
             << ", actual: " << actual.processing_origins()[i];
    }
  }

  testing::AssertionResult payload_contents_equal = PayloadContentsEqual(
      expected.payload_contents(), actual.payload_contents());
  if (!payload_contents_equal)
    return payload_contents_equal;

  return SharedInfoEqual(expected.shared_info(), actual.shared_info());
}

testing::AssertionResult PayloadContentsEqual(
    const AggregationServicePayloadContents& expected,
    const AggregationServicePayloadContents& actual) {
  if (expected.operation != actual.operation) {
    return testing::AssertionFailure()
           << "Expected operation " << expected.operation
           << ", actual: " << actual.operation;
  }
  if (expected.bucket != actual.bucket) {
    return testing::AssertionFailure() << "Expected bucket " << expected.bucket
                                       << ", actual: " << actual.bucket;
  }
  if (expected.value != actual.value) {
    return testing::AssertionFailure() << "Expected value " << expected.value
                                       << ", actual: " << actual.value;
  }
  if (expected.processing_type != actual.processing_type) {
    return testing::AssertionFailure()
           << "Expected processing_type " << expected.processing_type
           << ", actual: " << actual.processing_type;
  }

  return testing::AssertionSuccess();
}

testing::AssertionResult SharedInfoEqual(
    const AggregatableReportSharedInfo& expected,
    const AggregatableReportSharedInfo& actual) {
  if (expected.scheduled_report_time != actual.scheduled_report_time) {
    return testing::AssertionFailure()
           << "Expected scheduled_report_time "
           << expected.scheduled_report_time
           << ", actual: " << actual.scheduled_report_time;
  }
  if (expected.privacy_budget_key != actual.privacy_budget_key) {
    return testing::AssertionFailure()
           << "Expected privacy_budget_key " << expected.privacy_budget_key
           << ", actual: " << actual.privacy_budget_key;
  }

  return testing::AssertionSuccess();
}

std::vector<url::Origin> GetExampleProcessingOrigins() {
  return {url::Origin::Create(GURL("https://a.example")),
          url::Origin::Create(GURL("https://b.example"))};
}

AggregatableReportRequest CreateExampleRequest(
    AggregationServicePayloadContents::ProcessingType processing_type) {
  return CreateExampleRequest(processing_type, GetExampleProcessingOrigins());
}

AggregatableReportRequest CreateExampleRequest(
    AggregationServicePayloadContents::ProcessingType processing_type,
    std::vector<url::Origin> processing_origins) {
  return AggregatableReportRequest::Create(
             std::move(processing_origins),
             AggregationServicePayloadContents(
                 AggregationServicePayloadContents::Operation::kHistogram,
                 /*bucket=*/123, /*value=*/456, processing_type,
                 url::Origin::Create(GURL("https://reporting.example"))),
             AggregatableReportSharedInfo(
                 /*scheduled_report_time=*/base::Time::Now(),
                 /*privacy_budget_key=*/"example_budget_key"))
      .value();
}

AggregatableReportRequest CloneReportRequest(
    const AggregatableReportRequest& request) {
  return AggregatableReportRequest::Create(request.processing_origins(),
                                           request.payload_contents(),
                                           request.shared_info())
      .value();
}

AggregatableReport CloneAggregatableReport(const AggregatableReport& report) {
  std::vector<AggregationServicePayload> payloads;
  for (const AggregationServicePayload& payload : report.payloads()) {
    payloads.emplace_back(payload.origin, payload.payload, payload.key_id);
  }

  return AggregatableReport(std::move(payloads), report.shared_info());
}

TestHpkeKey GenerateKey(std::string key_id) {
  bssl::ScopedEVP_HPKE_KEY key;
  EXPECT_TRUE(EVP_HPKE_KEY_generate(key.get(), EVP_hpke_x25519_hkdf_sha256()));

  std::vector<uint8_t> public_key(X25519_PUBLIC_VALUE_LEN);
  size_t public_key_len;
  EXPECT_TRUE(EVP_HPKE_KEY_public_key(
      /*key=*/key.get(), /*out=*/public_key.data(),
      /*out_len=*/&public_key_len, /*max_out=*/public_key.size()));
  EXPECT_EQ(public_key.size(), public_key_len);

  TestHpkeKey hpke_key{
      {}, PublicKey(key_id, public_key), base::Base64Encode(public_key)};
  EVP_HPKE_KEY_copy(&hpke_key.full_hpke_key, key.get());

  return hpke_key;
}

}  // namespace aggregation_service

TestAggregationServiceStorageContext::TestAggregationServiceStorageContext(
    const base::Clock* clock)
    : storage_(base::SequenceBound<AggregationServiceStorageSql>(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}),
          /*run_in_memory=*/true,
          /*path_to_database=*/base::FilePath(),
          clock)) {}

TestAggregationServiceStorageContext::~TestAggregationServiceStorageContext() =
    default;

const base::SequenceBound<content::AggregationServiceKeyStorage>&
TestAggregationServiceStorageContext::GetKeyStorage() {
  return storage_;
}

TestAggregationServiceKeyFetcher::TestAggregationServiceKeyFetcher()
    : AggregationServiceKeyFetcher(/*storage_context=*/nullptr,
                                   /*network_fetcher=*/nullptr) {}

TestAggregationServiceKeyFetcher::~TestAggregationServiceKeyFetcher() = default;

void TestAggregationServiceKeyFetcher::GetPublicKey(const url::Origin& origin,
                                                    FetchCallback callback) {
  callbacks_[origin].push_back(std::move(callback));
}

void TestAggregationServiceKeyFetcher::TriggerPublicKeyResponse(
    const url::Origin& origin,
    absl::optional<PublicKey> key,
    PublicKeyFetchStatus status) {
  ASSERT_TRUE(base::Contains(callbacks_, origin))
      << "No corresponding GetPublicKeys call for origin " << origin;
  ASSERT_EQ(key.has_value(), status == PublicKeyFetchStatus::kOk)
      << "Key must be returned if and only if status is kOk";

  std::vector<FetchCallback> callbacks = std::move(callbacks_[origin]);
  callbacks_.erase(origin);
  for (FetchCallback& callback : callbacks) {
    std::move(callback).Run(key, status);
  }
}

void TestAggregationServiceKeyFetcher::TriggerPublicKeyResponseForAllOrigins(
    absl::optional<PublicKey> key,
    PublicKeyFetchStatus status) {
  std::vector<url::Origin> all_origins_;
  for (const auto& elem : callbacks_) {
    all_origins_.push_back(elem.first);
  }
  for (auto& origin : all_origins_) {
    TriggerPublicKeyResponse(std::move(origin), key, status);
  }
}

bool TestAggregationServiceKeyFetcher::HasPendingCallbacks() {
  return !callbacks_.empty();
}

TestAggregatableReportProvider::TestAggregatableReportProvider() = default;
TestAggregatableReportProvider::~TestAggregatableReportProvider() = default;

absl::optional<AggregatableReport>
TestAggregatableReportProvider::CreateFromRequestAndPublicKeys(
    AggregatableReportRequest report_request,
    std::vector<PublicKey> public_keys) const {
  ++num_calls_;
  previous_request_ = aggregation_service::CloneReportRequest(report_request);
  previous_public_keys_ = public_keys;

  EXPECT_TRUE(report_to_return_.has_value());
  return aggregation_service::CloneAggregatableReport(
      report_to_return_.value());
}

std::ostream& operator<<(
    std::ostream& out,
    const AggregationServicePayloadContents::Operation& operation) {
  switch (operation) {
    case AggregationServicePayloadContents::Operation::kHistogram:
      return out << "kHistogram";
  }
}

std::ostream& operator<<(
    std::ostream& out,
    const AggregationServicePayloadContents::ProcessingType& processing_type) {
  switch (processing_type) {
    case AggregationServicePayloadContents::ProcessingType::kTwoParty:
      return out << "kTwoParty";
    case AggregationServicePayloadContents::ProcessingType::kSingleServer:
      return out << "kSingleServer";
  }
}

}  // namespace content
