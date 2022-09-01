// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CTAP_GET_ASSERTION_REQUEST_H_
#define DEVICE_FIDO_CTAP_GET_ASSERTION_REQUEST_H_

#include <stdint.h>

#include <array>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "crypto/sha2.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/fido_constants.h"
#include "device/fido/large_blob.h"
#include "device/fido/pin.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace cbor {
class Value;
}

namespace device {

// CtapGetAssertionOptions contains values that are pertinent to a
// |GetAssertionTask|, but are not specific to an individual
// authenticatorGetAssertion command, i.e. would not be directly serialised into
// the CBOR.
struct COMPONENT_EXPORT(DEVICE_FIDO) CtapGetAssertionOptions {
  CtapGetAssertionOptions();
  CtapGetAssertionOptions(const CtapGetAssertionOptions&);
  CtapGetAssertionOptions(CtapGetAssertionOptions&&);
  ~CtapGetAssertionOptions();

  // PRFInput contains salts for the hmac_secret extension, potentially specific
  // to a given credential ID.
  struct COMPONENT_EXPORT(DEVICE_FIDO) PRFInput {
    PRFInput();
    PRFInput(const PRFInput&);
    PRFInput(PRFInput&&);
    ~PRFInput();

    absl::optional<std::vector<uint8_t>> credential_id;
    std::array<uint8_t, 32> salt1;
    absl::optional<std::array<uint8_t, 32>> salt2;
  };

  absl::optional<pin::KeyAgreementResponse> pin_key_agreement;

  // prf_inputs may contain a default PRFInput without a |credential_id|. If so,
  // it will be the first element and all others will have |credential_id|s.
  // Elements are sorted by |credential_id|s, where present.
  std::vector<PRFInput> prf_inputs;
};

// Object that encapsulates request parameters for AuthenticatorGetAssertion as
// specified in the CTAP spec.
// https://fidoalliance.org/specs/fido-v2.0-rd-20161004/fido-client-to-authenticator-protocol-v2.0-rd-20161004.html#authenticatorgetassertion
struct COMPONENT_EXPORT(DEVICE_FIDO) CtapGetAssertionRequest {
 public:
  using ClientDataHash = std::array<uint8_t, kClientDataHashLength>;

  // ParseOpts are optional parameters passed to Parse().
  struct ParseOpts {
    // reject_all_extensions makes parsing fail if any extensions are present.
    bool reject_all_extensions = false;
  };

  // HMACSecret contains the inputs to the hmac-secret extension:
  // https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#sctn-hmac-secret-extension
  struct HMACSecret {
    HMACSecret(base::span<const uint8_t, kP256X962Length> public_key_x962,
               base::span<const uint8_t> encrypted_salts,
               base::span<const uint8_t> salts_auth);
    HMACSecret(const HMACSecret&);
    ~HMACSecret();
    HMACSecret& operator=(const HMACSecret&);

    std::array<uint8_t, kP256X962Length> public_key_x962;
    std::vector<uint8_t> encrypted_salts;
    std::vector<uint8_t> salts_auth;
  };

  // Decodes a CTAP2 authenticatorGetAssertion request message. The request's
  // |client_data_json| will be empty and |client_data_hash| will be set.
  //
  // A |uv| bit of 0 is mapped to UserVerificationRequirement::kDiscouraged.
  static absl::optional<CtapGetAssertionRequest> Parse(
      const cbor::Value::MapValue& request_map) {
    return Parse(request_map, ParseOpts());
  }
  static absl::optional<CtapGetAssertionRequest> Parse(
      const cbor::Value::MapValue& request_map,
      const ParseOpts& opts);

  CtapGetAssertionRequest(std::string rp_id, std::string client_data_json);
  CtapGetAssertionRequest(const CtapGetAssertionRequest& that);
  CtapGetAssertionRequest(CtapGetAssertionRequest&& that);
  CtapGetAssertionRequest& operator=(const CtapGetAssertionRequest& other);
  CtapGetAssertionRequest& operator=(CtapGetAssertionRequest&& other);
  ~CtapGetAssertionRequest();

  std::string rp_id;
  std::string client_data_json;
  std::array<uint8_t, kClientDataHashLength> client_data_hash;
  UserVerificationRequirement user_verification =
      UserVerificationRequirement::kDiscouraged;
  bool user_presence_required = true;

  std::vector<PublicKeyCredentialDescriptor> allow_list;
  absl::optional<std::vector<uint8_t>> pin_auth;
  absl::optional<PINUVAuthProtocol> pin_protocol;
  absl::optional<std::vector<CableDiscoveryData>> cable_extension;
  absl::optional<std::string> app_id;
  absl::optional<std::array<uint8_t, crypto::kSHA256Length>>
      alternative_application_parameter;
  absl::optional<HMACSecret> hmac_secret;
  bool large_blob_key = false;
  bool large_blob_read = false;
  absl::optional<LargeBlob> large_blob_write;
  bool get_cred_blob = false;

  // Instructs the request handler only to dispatch this request via U2F.
  bool is_u2f_only = false;

  // Indicates whether the request was created in an off-the-record
  // BrowserContext (e.g. Incognito or Guest mode in Chrome).
  bool is_off_the_record_context = false;
};

struct CtapGetNextAssertionRequest {};

// Serializes GetAssertion request parameter into CBOR encoded map with
// integer keys and CBOR encoded values as defined by the CTAP spec.
// https://drafts.fidoalliance.org/fido-2/latest/fido-client-to-authenticator-protocol-v2.0-wd-20180305.html#authenticatorGetAssertion
COMPONENT_EXPORT(DEVICE_FIDO)
std::pair<CtapRequestCommand, absl::optional<cbor::Value>>
AsCTAPRequestValuePair(const CtapGetAssertionRequest&);

COMPONENT_EXPORT(DEVICE_FIDO)
std::pair<CtapRequestCommand, absl::optional<cbor::Value>>
AsCTAPRequestValuePair(const CtapGetNextAssertionRequest&);

}  // namespace device

#endif  // DEVICE_FIDO_CTAP_GET_ASSERTION_REQUEST_H_
