// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ATTESTATION_TPM_CHALLENGE_KEY_RESULT_H_
#define CHROME_BROWSER_CHROMEOS_ATTESTATION_TPM_CHALLENGE_KEY_RESULT_H_

#include <string>

namespace chromeos {
namespace attestation {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TpmChallengeKeyResultCode {
  // Not a result code. Convenient for UMA metrics.
  kSuccess = 0,
  kDevicePolicyDisabledError = 1,
  kSignChallengeFailedError = 2,
  kUserNotManagedError = 3,
  kKeyRegistrationFailedError = 4,
  kUserKeyNotAvailableError = 5,
  kUserPolicyDisabledError = 6,
  kNonEnterpriseDeviceError = 7,
  kDbusError = 8,
  kUserRejectedError = 9,
  kGetCertificateFailedError = 10,
  kResetRequiredError = 11,
  kAttestationUnsupportedError = 12,
  kTimeoutError = 13,
  kDeviceWebBasedAttestationUrlError = 14,
  kExtensionNotWhitelistedError = 15,
  kChallengeBadBase64Error = 16,
  kDeviceWebBasedAttestationNotOobeError = 17,
  kGetPublicKeyFailedError = 18,
  kMaxValue = kGetPublicKeyFailedError,
};

// If |IsSuccess| returns false, |result_code| contains error code and
// |public_key| and |challenge_response| are empty.
// Otherwise, if the object was returned from
// |TpmChallengeKeySubtle::PrepareKey|, the |public_key| is filled. If the
// object was returned from |TpmChallengeKey::BuildResponse| or
// |TpmChallengeKeySubtle::SignChallenge|, the |challenge_response| is filled.
struct TpmChallengeKeyResult {
  static const char kDevicePolicyDisabledErrorMsg[];
  static const char kSignChallengeFailedErrorMsg[];
  static const char kUserNotManagedErrorMsg[];
  static const char kKeyRegistrationFailedErrorMsg[];
  static const char kUserPolicyDisabledErrorMsg[];
  static const char kUserKeyNotAvailableErrorMsg[];
  static const char kNonEnterpriseDeviceErrorMsg[];
  static const char kDbusErrorMsg[];
  static const char kUserRejectedErrorMsg[];
  static const char kGetCertificateFailedErrorMsg[];
  static const char kResetRequiredErrorMsg[];
  static const char kAttestationUnsupportedErrorMsg[];
  static const char kTimeoutErrorMsg[];
  static const char kDeviceWebBasedAttestationUrlErrorMsg[];
  static const char kExtensionNotWhitelistedErrorMsg[];
  static const char kChallengeBadBase64ErrorMsg[];
  static const char kDeviceWebBasedAttestationNotOobeErrorMsg[];
  static const char kGetPublicKeyFailedErrorMsg[];

  static TpmChallengeKeyResult MakeChallengeResponse(
      const std::string& challenge_response);
  static TpmChallengeKeyResult MakePublicKey(const std::string& public_key);
  static TpmChallengeKeyResult MakeSuccess();
  static TpmChallengeKeyResult MakeError(TpmChallengeKeyResultCode error_code);

  const char* GetErrorMessage() const;
  bool IsSuccess() const;

  TpmChallengeKeyResultCode result_code = TpmChallengeKeyResultCode::kSuccess;
  std::string public_key;
  std::string challenge_response;
};

}  // namespace attestation
}  // namespace chromeos

#endif  //  CHROME_BROWSER_CHROMEOS_ATTESTATION_TPM_CHALLENGE_KEY_RESULT_H_
