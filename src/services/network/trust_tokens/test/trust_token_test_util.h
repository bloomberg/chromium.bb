// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_TEST_TRUST_TOKEN_TEST_UTIL_H_
#define SERVICES_NETWORK_TRUST_TOKENS_TEST_TRUST_TOKEN_TEST_UTIL_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/json/json_string_value_serializer.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/trust_token_request_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
class URLRequest;
}  // namespace net

namespace network {

// TestURLRequestMaker is a mixin allowing consumers to factor out the
// boilerplate of constructing URLRequests in tests.
class TestURLRequestMaker {
 public:
  TestURLRequestMaker();
  virtual ~TestURLRequestMaker();

  TestURLRequestMaker(const TestURLRequestMaker&) = delete;
  TestURLRequestMaker& operator=(const TestURLRequestMaker&) = delete;

  // Constructs and returns a URLRequest with destination |spec|.
  std::unique_ptr<net::URLRequest> MakeURLRequest(base::StringPiece spec);

 protected:
  net::TestDelegate delegate_;
  net::TestURLRequestContext context_;
};

// TrustTokenRequestHelperTest is a fixture common to tests for Trust Tokens
// issuance, redemption, and signing. It factors out the boilerplate of
// waiting for asynchronous store operations' results.
class TrustTokenRequestHelperTest : public ::testing::Test {
 public:
  explicit TrustTokenRequestHelperTest(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::DEFAULT);
  ~TrustTokenRequestHelperTest() override;

  TrustTokenRequestHelperTest(const TrustTokenRequestHelperTest&) = delete;
  TrustTokenRequestHelperTest& operator=(const TrustTokenRequestHelperTest&) =
      delete;

 protected:
  base::test::TaskEnvironment env_;

  TestURLRequestMaker request_maker_;

  std::unique_ptr<net::URLRequest> MakeURLRequest(base::StringPiece spec) {
    return request_maker_.MakeURLRequest(spec);
  }

  // Executes a request helper's Begin operation synchronously, removing some
  // boilerplate from waiting for the results of the (actually asynchronous)
  // operation's result.
  mojom::TrustTokenOperationStatus ExecuteBeginOperationAndWaitForResult(
      TrustTokenRequestHelper* helper,
      net::URLRequest* request);

  mojom::TrustTokenOperationStatus ExecuteFinalizeAndWaitForResult(
      TrustTokenRequestHelper* helper,
      mojom::URLResponseHead* reponse);
};

// The following helper methods unify parameterized unit/integration testing of
// the Trust Tokens interface.
//
// They provide a way to serialize a number of Trust Tokens parameter structures
// to JSON in a manner that covers all of the Trust Tokens
// parameters, and all of the permitted values of the enum and bool parameters,
// in order to verify that the parameters' values correctly
// serialize/deserialize and are properly propagated to the network stack.
//
// Intended use:
// - parameterize tests by k{Issuance, Signing, Redemption}TrustTokenParameters;
// - in the tests, call |SerializeTrustTokenParametersAndConstructExpectation|
// to construct (1) a string representation of the trustToken JS argument and
// (2) a corresponding mojom::TrustTokenParams object expected to
// appear downstream;
// - pass the provided argument to the API (fetch, iframe, XHR, ...) and check
// that the corresponding Mojo struct does, in fact, subsequently materialize.

// The instantiations of this struct will be serialized and passed to a
// `fetch` call in executed JS. This class is declared out-of-line so that it
// can be shared between embedder- and Blink-side code.
struct TrustTokenTestParameters final {
  // TrustTokenTestParameters (when serialized, nullopt in an optional field
  // will be omitted from the parameter's value):
  TrustTokenTestParameters(
      mojom::TrustTokenOperationType type,
      base::Optional<mojom::TrustTokenRefreshPolicy> refresh_policy,
      base::Optional<mojom::TrustTokenSignRequestData> sign_request_data,
      base::Optional<bool> include_timestamp_header,
      base::Optional<std::string> issuer_spec,
      base::Optional<std::vector<std::string>> additional_signed_headers);

  ~TrustTokenTestParameters();

  TrustTokenTestParameters(const TrustTokenTestParameters&);
  TrustTokenTestParameters& operator=(const TrustTokenTestParameters&);

  mojom::TrustTokenOperationType type;
  base::Optional<mojom::TrustTokenRefreshPolicy> refresh_policy;
  base::Optional<mojom::TrustTokenSignRequestData> sign_request_data;
  base::Optional<bool> include_timestamp_header;
  // Because static initialization of GURLs/Origins isn't allowed in tests, use
  // the string representation of the issuer origin and convert it to an Origin
  // in the test.
  base::Optional<std::string> issuer_spec;
  base::Optional<std::vector<std::string>> additional_signed_headers;
};

// Serializes the value of a Trust Tokens enum parameter to its JS string
// representation. Must be kept in sync with the corresponding IDL enum
// definition.
std::string TrustTokenEnumToString(mojom::TrustTokenOperationType type);
std::string TrustTokenEnumToString(mojom::TrustTokenRefreshPolicy policy);
std::string TrustTokenEnumToString(
    mojom::TrustTokenSignRequestData sign_request_data);

// For a given test case, creates and returns:
// 1. a serialized JSON dictionary suitable for passing as the value of
//    `fetch`'s (and XHR's, and iframe's) `trustToken` parameter.
// 2. a TrustTokenParams object that should equal the
//    value eventually passed downstream when a fetch/XHR/iframe load
//    is provided the serialized parameters.
struct TrustTokenParametersAndSerialization {
  TrustTokenParametersAndSerialization(mojom::TrustTokenParamsPtr params,
                                       std::string serialized_params);
  ~TrustTokenParametersAndSerialization();

  TrustTokenParametersAndSerialization(
      const TrustTokenParametersAndSerialization&) = delete;
  TrustTokenParametersAndSerialization& operator=(
      const TrustTokenParametersAndSerialization&) = delete;

  TrustTokenParametersAndSerialization(TrustTokenParametersAndSerialization&&);
  TrustTokenParametersAndSerialization& operator=(
      TrustTokenParametersAndSerialization&&);

  mojom::TrustTokenParamsPtr params;
  std::string serialized_params;
};
TrustTokenParametersAndSerialization
SerializeTrustTokenParametersAndConstructExpectation(
    const TrustTokenTestParameters& input);

// These groups of parameters are defined in this utility file so that they can
// be shared among different tests deserializing and propagating Trust Tokens
// parameters; see above for a more detailed description of the intended use.
const TrustTokenTestParameters kIssuanceTrustTokenTestParameters[]{
    // For issuance, there are no additional parameters to specify.
    TrustTokenTestParameters(mojom::TrustTokenOperationType::kIssuance,
                             base::nullopt,
                             base::nullopt,
                             base::nullopt,
                             base::nullopt,
                             base::nullopt)};

const TrustTokenTestParameters kRedemptionTrustTokenTestParameters[]{
    // For redemption, there is one free parameter, refreshPolicy, with two
    // values (and a default).
    TrustTokenTestParameters(mojom::TrustTokenOperationType::kRedemption,
                             mojom::TrustTokenRefreshPolicy::kRefresh,
                             base::nullopt,
                             base::nullopt,
                             base::nullopt,
                             base::nullopt),
    TrustTokenTestParameters(mojom::TrustTokenOperationType::kRedemption,
                             mojom::TrustTokenRefreshPolicy::kUseCached,
                             base::nullopt,
                             base::nullopt,
                             base::nullopt,
                             base::nullopt),
    TrustTokenTestParameters(mojom::TrustTokenOperationType::kRedemption,
                             base::nullopt,
                             base::nullopt,
                             base::nullopt,
                             base::nullopt,
                             base::nullopt)};

const TrustTokenTestParameters kSigningTrustTokenTestParameters[]{
    // Signing's inputs are issuer, signRequestData, additionalSignedHeaders,
    // and includeTimestampHeader; "issuer" has no default and must always be
    // a secure origin.
    TrustTokenTestParameters(
        mojom::TrustTokenOperationType::kSigning,
        base::nullopt,
        mojom::TrustTokenSignRequestData::kOmit,
        /*include_timestamp_header=*/true,
        "https://issuer.example",
        std::vector<std::string>{"one additional header's name",
                                 "another additional header's name"}),
    TrustTokenTestParameters(mojom::TrustTokenOperationType::kSigning,
                             base::nullopt,
                             mojom::TrustTokenSignRequestData::kHeadersOnly,
                             /*include_timestamp_header=*/false,
                             "https://issuer.example",
                             base::nullopt),
    TrustTokenTestParameters(mojom::TrustTokenOperationType::kSigning,
                             base::nullopt,
                             mojom::TrustTokenSignRequestData::kInclude,
                             /*include_timestamp_header=*/base::nullopt,
                             "https://issuer.example",
                             base::nullopt),
};

// Given a well-formed key commitment record JSON and an issuer origin, returns
// a serialized one-item dictionary mapping the commitment to the issuer.
//
// Example:
//   WrapKeyCommitmentForIssuer("https://issuer.com", R"( {"srrkey": "abcd"} )")
//   =  R"( { "https://issuer.com": { "srrkey": "abcd" } } )"
std::string WrapKeyCommitmentForIssuer(const url::Origin& issuer,
                                       base::StringPiece commitment);

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_TEST_TRUST_TOKEN_TEST_UTIL_H_
