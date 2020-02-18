// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CERTIFICATE_CAST_CERT_VALIDATOR_H_
#define CAST_COMMON_CERTIFICATE_CAST_CERT_VALIDATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "platform/base/error.h"
#include "platform/base/macros.h"

namespace cast {
namespace certificate {

class CastCRL;

// Describes the policy for a Device certificate.
enum class CastDeviceCertPolicy {
  // The device certificate is unrestricted.
  kUnrestricted,

  // The device certificate is for an audio-only device.
  kAudioOnly,
};

enum class CRLPolicy {
  // Revocation is only checked if a CRL is provided.
  kCrlOptional,

  // Revocation is always checked. A missing CRL results in failure.
  kCrlRequired,
};

enum class DigestAlgorithm {
  kSha1,
  kSha256,
  kSha384,
  kSha512,
};

struct ConstDataSpan {
  const uint8_t* data;
  uint32_t length;
};

struct DateTime {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
};

struct TrustStore;

class CastTrustStore {
 public:
  // Singleton for the Cast trust store for legacy networkingPrivate use.
  static CastTrustStore* GetInstance();

  CastTrustStore();
  ~CastTrustStore();

  TrustStore* trust_store() const { return trust_store_.get(); }

 private:
  std::unique_ptr<TrustStore> trust_store_;
  OSP_DISALLOW_COPY_AND_ASSIGN(CastTrustStore);
};

// An object of this type is returned by the VerifyDeviceCert function, and can
// be used for additional certificate-related operations, using the verified
// certificate.
class CertVerificationContext {
 public:
  CertVerificationContext() = default;
  virtual ~CertVerificationContext() = default;

  // Use the public key from the verified certificate to verify a
  // |digest_algorithm|WithRSAEncryption |signature| over arbitrary |data|.
  // Both |signature| and |data| hold raw binary data. Returns true if the
  // signature was correct.
  virtual bool VerifySignatureOverData(
      const ConstDataSpan& signature,
      const ConstDataSpan& data,
      DigestAlgorithm digest_algorithm) const = 0;

  // Retrieve the Common Name attribute of the subject's distinguished name from
  // the verified certificate, if present.  Returns an empty string if no Common
  // Name is found.
  virtual const std::string& GetCommonName() const = 0;

 private:
  OSP_DISALLOW_COPY_AND_ASSIGN(CertVerificationContext);
};

// Verifies a cast device certficate given a chain of DER-encoded certificates.
//
// Inputs:
//
// * |der_certs| is a chain of DER-encoded certificates:
//   * |der_certs[0]| is the target certificate (i.e. the device certificate).
//   * |der_certs[1..n-1]| are intermediates certificates to use in path
//     building.  Their ordering does not matter.
//
// * |time| is the timestamp to use for determining if the certificate is
//   expired.
//
// * |crl| is the CRL to check for certificate revocation status.
//   If this is a nullptr, then revocation checking is currently disabled.
//
// * |crl_policy| is for choosing how to handle the absence of a CRL.
//   If CRL_REQUIRED is passed, then an empty |crl| input would result
//   in a failed verification. Otherwise, |crl| is ignored if it is absent.
//
// * |trust_store| is an optional set of trusted certificates that may act as
//   root CAs during chain verification.  If this is nullptr, the built-in Cast
//   root certificates will be used.
//
// Outputs:
//
// Returns openscreen::Error::Code::kNone on success. Otherwise, the
// corresponding openscreen::Error::Code. On success, the output parameters are
// filled with more details:
//
//   * |context| is filled with an object that can be used to verify signatures
//     using the device certificate's public key, as well as to extract other
//     properties from the device certificate (Common Name).
//   * |policy| is filled with an indication of the device certificate's policy
//     (i.e. is it for audio-only devices or is it unrestricted?)
MAYBE_NODISCARD openscreen::Error VerifyDeviceCert(
    const std::vector<std::string>& der_certs,
    const DateTime& time,
    std::unique_ptr<CertVerificationContext>* context,
    CastDeviceCertPolicy* policy,
    const CastCRL* crl,
    CRLPolicy crl_policy,
    TrustStore* trust_store = nullptr);

}  // namespace certificate
}  // namespace cast

#endif  // CAST_COMMON_CERTIFICATE_CAST_CERT_VALIDATOR_H_
