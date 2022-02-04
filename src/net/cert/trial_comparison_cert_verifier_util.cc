// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/trial_comparison_cert_verifier_util.h"

#include "build/build_config.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/ev_root_ca_metadata.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/parsed_certificate.h"
#include "net/cert/x509_util.h"

namespace net {

namespace {

scoped_refptr<ParsedCertificate> ParsedCertificateFromBuffer(
    CRYPTO_BUFFER* cert_handle,
    CertErrors* errors) {
  return ParsedCertificate::Create(bssl::UpRef(cert_handle),
                                   x509_util::DefaultParseCertificateOptions(),
                                   errors);
}

ParsedCertificateList ParsedCertificateListFromX509Certificate(
    const X509Certificate* cert) {
  CertErrors parsing_errors;

  ParsedCertificateList certs;
  scoped_refptr<ParsedCertificate> target =
      ParsedCertificateFromBuffer(cert->cert_buffer(), &parsing_errors);
  if (!target)
    return {};
  certs.push_back(target);

  for (const auto& buf : cert->intermediate_buffers()) {
    scoped_refptr<ParsedCertificate> intermediate =
        ParsedCertificateFromBuffer(buf.get(), &parsing_errors);
    if (!intermediate)
      return {};
    certs.push_back(intermediate);
  }

  return certs;
}

// Tests whether cert has multiple EV policies, and at least one matches the
// root. This is not a complete test of EV, but just enough to give a possible
// explanation as to why the platform verifier did not validate as EV while
// builtin did. (Since only the builtin verifier correctly handles multiple
// candidate EV policies.)
bool CertHasMultipleEVPoliciesAndOneMatchesRoot(const X509Certificate* cert) {
  if (cert->intermediate_buffers().empty())
    return false;

  ParsedCertificateList certs = ParsedCertificateListFromX509Certificate(cert);
  if (certs.empty())
    return false;

  ParsedCertificate* leaf = certs.front().get();
  ParsedCertificate* root = certs.back().get();

  if (!leaf->has_policy_oids())
    return false;

  const EVRootCAMetadata* ev_metadata = EVRootCAMetadata::GetInstance();
  std::set<der::Input> candidate_oids;
  for (const der::Input& oid : leaf->policy_oids()) {
    if (ev_metadata->IsEVPolicyOIDGivenBytes(oid))
      candidate_oids.insert(oid);
  }

  if (candidate_oids.size() <= 1)
    return false;

  SHA256HashValue root_fingerprint;
  crypto::SHA256HashString(root->der_cert().AsStringPiece(),
                           root_fingerprint.data,
                           sizeof(root_fingerprint.data));

  for (const der::Input& oid : candidate_oids) {
    if (ev_metadata->HasEVPolicyOIDGivenBytes(root_fingerprint, oid))
      return true;
  }

  return false;
}

}  // namespace

// Note: This ignores the result of stapled OCSP (which is the same for both
// verifiers) and informational statuses about the certificate algorithms and
// the hashes, since they will be the same if the certificate chains are the
// same.
bool CertVerifyResultEqual(const CertVerifyResult& a,
                           const CertVerifyResult& b) {
  return std::tie(a.cert_status, a.is_issued_by_known_root) ==
             std::tie(b.cert_status, b.is_issued_by_known_root) &&
         (!!a.verified_cert == !!b.verified_cert) &&
         (!a.verified_cert ||
          a.verified_cert->EqualsIncludingChain(b.verified_cert.get()));
}

TrialComparisonResult IsSynchronouslyIgnorableDifference(
    int primary_error,
    const CertVerifyResult& primary_result,
    int trial_error,
    const CertVerifyResult& trial_result,
    bool sha1_local_anchors_enabled) {
  DCHECK(primary_result.verified_cert);
  DCHECK(trial_result.verified_cert);

  if (primary_error == OK &&
      primary_result.verified_cert->intermediate_buffers().empty()) {
    // Platform may support trusting a leaf certificate directly. Builtin
    // verifier does not. See https://crbug.com/814994.
    return TrialComparisonResult::kIgnoredLocallyTrustedLeaf;
  }

  const bool chains_equal = primary_result.verified_cert->EqualsIncludingChain(
      trial_result.verified_cert.get());

  if (chains_equal && (trial_result.cert_status & CERT_STATUS_IS_EV) &&
      !(primary_result.cert_status & CERT_STATUS_IS_EV) &&
      (primary_error == trial_error)) {
    // The platform CertVerifyProc impls only check a single potential EV
    // policy from the leaf.  If the leaf had multiple policies, builtin
    // verifier may verify it as EV when the platform verifier did not.
    if (CertHasMultipleEVPoliciesAndOneMatchesRoot(
            trial_result.verified_cert.get())) {
      return TrialComparisonResult::kIgnoredMultipleEVPoliciesAndOneMatchesRoot;
    }
  }

  // SHA-1 signatures are not supported; ignore any results with expected SHA-1
  // errors. There are however a few cases with SHA-1 signatures where we might
  // want to see the difference:
  //
  //  * local anchors enabled, and one verifier built to a SHA-1 local root but
  //     the other built to a known root.
  //  * If a verifier returned a SHA-1 signature status but did not return an
  //    error.
  if (!(sha1_local_anchors_enabled &&
        (!primary_result.is_issued_by_known_root ||
         !trial_result.is_issued_by_known_root)) &&
      (primary_result.cert_status & CERT_STATUS_SHA1_SIGNATURE_PRESENT) &&
      (trial_result.cert_status & CERT_STATUS_SHA1_SIGNATURE_PRESENT) &&
      primary_error != OK && trial_error != OK) {
    return TrialComparisonResult::kIgnoredSHA1SignaturePresent;
  }

#if BUILDFLAG(IS_WIN)
  // cert_verify_proc_win has some oddities around revocation checking
  // and EV certs; if the only difference is the primary windows verifier
  // had CERT_STATUS_REV_CHECKING_ENABLED in addition then we can ignore.
  if (chains_equal &&
      (primary_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED) &&
      ((primary_result.cert_status ^ CERT_STATUS_REV_CHECKING_ENABLED) ==
       trial_result.cert_status)) {
    return TrialComparisonResult::kIgnoredWindowsRevCheckingEnabled;
  }
#endif

  // Differences in chain or errors don't matter much if both
  // return AUTHORITY_INVALID.
  if ((primary_result.cert_status & CERT_STATUS_AUTHORITY_INVALID) &&
      (trial_result.cert_status & CERT_STATUS_AUTHORITY_INVALID)) {
    return TrialComparisonResult::kIgnoredBothAuthorityInvalid;
  }

  // Due to differences in path building preferences we may end up with
  // different chains in cross-signing situations. These cases are ignorable if
  // the errors are equivalent and both chains end up at a known_root.
  if (!chains_equal && (primary_error == trial_error) &&
      primary_result.is_issued_by_known_root &&
      trial_result.is_issued_by_known_root &&
      (primary_result.cert_status == trial_result.cert_status)) {
    return TrialComparisonResult::kIgnoredBothKnownRoot;
  }

  // If the primary has an error and cert_status reports that a Symantec legacy
  // cert is present, ignore the error if trial reports
  // ERR_CERT_AUTHORITY_INVALID as trial will report AUTHORITY_INVALID and short
  // circuits other checks resulting in mismatching errors and cert status.
  if (primary_error != OK && trial_error == ERR_CERT_AUTHORITY_INVALID &&
      (primary_result.cert_status & CERT_STATUS_SYMANTEC_LEGACY)) {
    return TrialComparisonResult::
        kIgnoredBuiltinAuthorityInvalidPlatformSymantec;
  }

  return TrialComparisonResult::kInvalid;
}

}  // namespace net