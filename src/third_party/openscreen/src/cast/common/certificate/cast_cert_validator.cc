// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/certificate/cast_cert_validator.h"

#include <openssl/digest.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "cast/common/certificate/cast_cert_validator_internal.h"

namespace cast {
namespace certificate {
namespace {

using CastCertError = openscreen::Error::Code;

// -------------------------------------------------------------------------
// Cast trust anchors.
// -------------------------------------------------------------------------

// There are two trusted roots for Cast certificate chains:
//
//   (1) CN=Cast Root CA    (kCastRootCaDer)
//   (2) CN=Eureka Root CA  (kEurekaRootCaDer)
//
// These constants are defined by the files included next:

#include "cast/common/certificate/cast_root_ca_cert_der-inc.h"
#include "cast/common/certificate/eureka_root_ca_der-inc.h"

constexpr static int32_t kMinRsaModulusLengthBits = 2048;

// Adds a trust anchor given a DER-encoded certificate from static
// storage.
template <size_t N>
bssl::UniquePtr<X509> MakeTrustAnchor(const uint8_t (&data)[N]) {
  const uint8_t* dptr = data;
  return bssl::UniquePtr<X509>{d2i_X509(nullptr, &dptr, N)};
}

// Stores intermediate state while attempting to find a valid certificate chain
// from a set of trusted certificates to a target certificate.  Together, a
// sequence of these forms a certificate chain to be verified as well as a stack
// that can be unwound for searching more potential paths.
struct CertPathStep {
  X509* cert;

  // The next index that can be checked in |trust_store| if the choice |cert| on
  // the path needs to be reverted.
  uint32_t trust_store_index;

  // The next index that can be checked in |intermediate_certs| if the choice
  // |cert| on the path needs to be reverted.
  uint32_t intermediate_cert_index;
};

// These values are bit positions from RFC 5280 4.2.1.3 and will be passed to
// ASN1_BIT_STRING_get_bit.
enum KeyUsageBits {
  kDigitalSignature = 0,
  kKeyCertSign = 5,
};

bool VerifySignedData(const EVP_MD* digest,
                      EVP_PKEY* public_key,
                      const ConstDataSpan& data,
                      const ConstDataSpan& signature) {
  // This code assumes the signature algorithm was RSASSA PKCS#1 v1.5 with
  // |digest|.
  bssl::ScopedEVP_MD_CTX ctx;
  if (!EVP_DigestVerifyInit(ctx.get(), nullptr, digest, nullptr, public_key)) {
    return false;
  }
  return (EVP_DigestVerify(ctx.get(), signature.data, signature.length,
                           data.data, data.length) == 1);
}

// Returns the OID for the Audio-Only Cast policy
// (1.3.6.1.4.1.11129.2.5.2) in DER form.
const ConstDataSpan& AudioOnlyPolicyOid() {
  static const uint8_t kAudioOnlyPolicy[] = {0x2B, 0x06, 0x01, 0x04, 0x01,
                                             0xD6, 0x79, 0x02, 0x05, 0x02};
  static ConstDataSpan kPolicySpan{kAudioOnlyPolicy, sizeof(kAudioOnlyPolicy)};
  return kPolicySpan;
}

class CertVerificationContextImpl final : public CertVerificationContext {
 public:
  CertVerificationContextImpl(bssl::UniquePtr<EVP_PKEY>&& cert,
                              std::string&& common_name)
      : public_key_{std::move(cert)}, common_name_(std::move(common_name)) {}

  ~CertVerificationContextImpl() override = default;

  bool VerifySignatureOverData(
      const ConstDataSpan& signature,
      const ConstDataSpan& data,
      DigestAlgorithm digest_algorithm) const override {
    const EVP_MD* digest;
    switch (digest_algorithm) {
      case DigestAlgorithm::kSha1:
        digest = EVP_sha1();
        break;
      case DigestAlgorithm::kSha256:
        digest = EVP_sha256();
        break;
      case DigestAlgorithm::kSha384:
        digest = EVP_sha384();
        break;
      case DigestAlgorithm::kSha512:
        digest = EVP_sha512();
        break;
      default:
        return false;
    }

    return VerifySignedData(digest, public_key_.get(), data, signature);
  }

  const std::string& GetCommonName() const override { return common_name_; }

 private:
  const bssl::UniquePtr<EVP_PKEY> public_key_;
  const std::string common_name_;
};

bool CertInPath(X509_NAME* name,
                const std::vector<CertPathStep>& steps,
                uint32_t start,
                uint32_t stop) {
  for (uint32_t i = start; i < stop; ++i) {
    if (X509_NAME_cmp(name, X509_get_subject_name(steps[i].cert)) == 0) {
      return true;
    }
  }
  return false;
}

uint8_t ParseAsn1TimeDoubleDigit(ASN1_GENERALIZEDTIME* time, int index) {
  return (time->data[index] - '0') * 10 + (time->data[index + 1] - '0');
}

// Parses DateTime with additional restrictions laid out by RFC 5280
// 4.1.2.5.2.
bool ParseAsn1GeneralizedTime(ASN1_GENERALIZEDTIME* time, DateTime* out) {
  static constexpr uint8_t kDaysPerMonth[] = {
      31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
  };

  if (time->length != 15) {
    return false;
  }
  if (time->data[14] != 'Z') {
    return false;
  }
  for (int i = 0; i < 14; ++i) {
    if (time->data[i] < '0' || time->data[i] > '9') {
      return false;
    }
  }
  out->year = ParseAsn1TimeDoubleDigit(time, 0) * 100 +
              ParseAsn1TimeDoubleDigit(time, 2);
  out->month = ParseAsn1TimeDoubleDigit(time, 4);
  out->day = ParseAsn1TimeDoubleDigit(time, 6);
  out->hour = ParseAsn1TimeDoubleDigit(time, 8);
  out->minute = ParseAsn1TimeDoubleDigit(time, 10);
  out->second = ParseAsn1TimeDoubleDigit(time, 12);
  if (out->month == 0 || out->month > 12) {
    return false;
  }
  int days_per_month = kDaysPerMonth[out->month - 1];
  if (out->month == 2) {
    if (out->year % 4 == 0 && (out->year % 100 != 0 || out->year % 400 == 0)) {
      days_per_month = 29;
    } else {
      days_per_month = 28;
    }
  }
  if (out->day == 0 || out->day > days_per_month) {
    return false;
  }
  if (out->hour > 23) {
    return false;
  }
  if (out->minute > 59) {
    return false;
  }
  // Leap seconds are allowed.
  if (out->second > 60) {
    return false;
  }
  return true;
}

bool IsDateTimeBefore(const DateTime& a, const DateTime& b) {
  if (a.year < b.year) {
    return true;
  } else if (a.year > b.year) {
    return false;
  }
  if (a.month < b.month) {
    return true;
  } else if (a.month > b.month) {
    return false;
  }
  if (a.day < b.day) {
    return true;
  } else if (a.day > b.day) {
    return false;
  }
  if (a.hour < b.hour) {
    return true;
  } else if (a.hour > b.hour) {
    return false;
  }
  if (a.minute < b.minute) {
    return true;
  } else if (a.minute > b.minute) {
    return false;
  }
  if (a.second < b.second) {
    return true;
  } else if (a.second > b.second) {
    return false;
  }
  return false;
}

CastCertError VerifyCertTime(X509* cert, const DateTime& time) {
  ASN1_GENERALIZEDTIME* not_before_asn1 = ASN1_TIME_to_generalizedtime(
      cert->cert_info->validity->notBefore, nullptr);
  ASN1_GENERALIZEDTIME* not_after_asn1 = ASN1_TIME_to_generalizedtime(
      cert->cert_info->validity->notAfter, nullptr);
  if (!not_before_asn1 || !not_after_asn1) {
    return CastCertError::kErrCertsVerifyGeneric;
  }
  DateTime not_before;
  DateTime not_after;
  bool times_valid = ParseAsn1GeneralizedTime(not_before_asn1, &not_before) &&
                     ParseAsn1GeneralizedTime(not_after_asn1, &not_after);
  ASN1_GENERALIZEDTIME_free(not_before_asn1);
  ASN1_GENERALIZEDTIME_free(not_after_asn1);
  if (!times_valid) {
    return CastCertError::kErrCertsVerifyGeneric;
  }
  if (IsDateTimeBefore(time, not_before) || IsDateTimeBefore(not_after, time)) {
    return CastCertError::kErrCertsDateInvalid;
  }
  return CastCertError::kNone;
}

bool VerifyPublicKeyLength(EVP_PKEY* public_key) {
  return EVP_PKEY_bits(public_key) >= kMinRsaModulusLengthBits;
}

bssl::UniquePtr<ASN1_BIT_STRING> GetKeyUsage(X509* cert) {
  int pos = X509_get_ext_by_NID(cert, NID_key_usage, -1);
  if (pos == -1) {
    return nullptr;
  }
  X509_EXTENSION* key_usage = X509_get_ext(cert, pos);
  const uint8_t* value = key_usage->value->data;
  ASN1_BIT_STRING* key_usage_bit_string = nullptr;
  if (!d2i_ASN1_BIT_STRING(&key_usage_bit_string, &value,
                           key_usage->value->length)) {
    return nullptr;
  }
  return bssl::UniquePtr<ASN1_BIT_STRING>{key_usage_bit_string};
}

CastCertError VerifyCertificateChain(const std::vector<CertPathStep>& path,
                                     uint32_t step_index,
                                     const DateTime& time) {
  // Default max path length is the number of intermediate certificates.
  int max_pathlen = path.size() - 2;

  std::vector<NAME_CONSTRAINTS*> path_name_constraints;
  CastCertError error = CastCertError::kNone;
  uint32_t i = step_index;
  for (; i < path.size() - 1; ++i) {
    X509* subject = path[i + 1].cert;
    X509* issuer = path[i].cert;
    bool is_root = (i == step_index);
    if (!is_root) {
      if ((error = VerifyCertTime(issuer, time)) != CastCertError::kNone) {
        return error;
      }
      if (X509_NAME_cmp(X509_get_subject_name(issuer),
                        X509_get_issuer_name(issuer)) != 0) {
        if (max_pathlen == 0) {
          return CastCertError::kErrCertsPathlen;
        }
        --max_pathlen;
      } else {
        issuer->ex_flags |= EXFLAG_SI;
      }
    } else {
      issuer->ex_flags |= EXFLAG_SI;
    }

    bssl::UniquePtr<ASN1_BIT_STRING> key_usage = GetKeyUsage(issuer);
    if (key_usage) {
      const int bit =
          ASN1_BIT_STRING_get_bit(key_usage.get(), KeyUsageBits::kKeyCertSign);
      if (bit == 0) {
        return CastCertError::kErrCertsVerifyGeneric;
      }
    }

    // Check that basicConstraints is present, specifies the CA bit, and use
    // pathLenConstraint if present.
    const int basic_constraints_index =
        X509_get_ext_by_NID(issuer, NID_basic_constraints, -1);
    if (basic_constraints_index == -1) {
      return CastCertError::kErrCertsVerifyGeneric;
    }
    X509_EXTENSION* const basic_constraints_extension =
        X509_get_ext(issuer, basic_constraints_index);
    bssl::UniquePtr<BASIC_CONSTRAINTS> basic_constraints{
        reinterpret_cast<BASIC_CONSTRAINTS*>(
            X509V3_EXT_d2i(basic_constraints_extension))};

    if (!basic_constraints || !basic_constraints->ca) {
      return CastCertError::kErrCertsVerifyGeneric;
    }

    if (basic_constraints->pathlen) {
      if (basic_constraints->pathlen->length != 1) {
        return CastCertError::kErrCertsVerifyGeneric;
      } else {
        const int pathlen = *basic_constraints->pathlen->data;
        if (pathlen < 0) {
          return CastCertError::kErrCertsVerifyGeneric;
        }
        if (pathlen < max_pathlen) {
          max_pathlen = pathlen;
        }
      }
    }

    if (X509_ALGOR_cmp(issuer->sig_alg, issuer->cert_info->signature) != 0) {
      return CastCertError::kErrCertsVerifyGeneric;
    }

    bssl::UniquePtr<EVP_PKEY> public_key{X509_get_pubkey(issuer)};
    if (!VerifyPublicKeyLength(public_key.get())) {
      return CastCertError::kErrCertsVerifyGeneric;
    }

    // NOTE: (!self-issued || target) -> verify name constraints.  Target case
    // is after the loop.
    const bool is_self_issued = issuer->ex_flags & EXFLAG_SI;
    if (!is_self_issued) {
      for (NAME_CONSTRAINTS* name_constraints : path_name_constraints) {
        if (NAME_CONSTRAINTS_check(subject, name_constraints) != X509_V_OK) {
          return CastCertError::kErrCertsVerifyGeneric;
        }
      }
    }

    if (issuer->nc) {
      path_name_constraints.push_back(issuer->nc);
    } else {
      const int index = X509_get_ext_by_NID(issuer, NID_name_constraints, -1);
      if (index != -1) {
        X509_EXTENSION* ext = X509_get_ext(issuer, index);
        auto* nc = reinterpret_cast<NAME_CONSTRAINTS*>(X509V3_EXT_d2i(ext));
        if (nc) {
          issuer->nc = nc;
          path_name_constraints.push_back(nc);
        } else {
          return CastCertError::kErrCertsVerifyGeneric;
        }
      }
    }

    // Check that any policy mappings present are _not_ the anyPolicy OID.  Even
    // though we don't otherwise handle policies, this is required by RFC 5280
    // 6.1.4(a).
    const int policy_mappings_index =
        X509_get_ext_by_NID(issuer, NID_policy_mappings, -1);
    if (policy_mappings_index != -1) {
      X509_EXTENSION* policy_mappings_extension =
          X509_get_ext(issuer, policy_mappings_index);
      auto* policy_mappings = reinterpret_cast<POLICY_MAPPINGS*>(
          X509V3_EXT_d2i(policy_mappings_extension));
      const uint32_t policy_mapping_count =
          sk_POLICY_MAPPING_num(policy_mappings);
      const ASN1_OBJECT* any_policy = OBJ_nid2obj(NID_any_policy);
      for (uint32_t i = 0; i < policy_mapping_count; ++i) {
        POLICY_MAPPING* policy_mapping =
            sk_POLICY_MAPPING_value(policy_mappings, i);
        const bool either_matches =
            ((OBJ_cmp(policy_mapping->issuerDomainPolicy, any_policy) == 0) ||
             (OBJ_cmp(policy_mapping->subjectDomainPolicy, any_policy) == 0));
        if (either_matches) {
          error = CastCertError::kErrCertsVerifyGeneric;
          break;
        }
      }
      sk_POLICY_MAPPING_free(policy_mappings);
      if (error != CastCertError::kNone) {
        return error;
      }
    }

    // Check that we don't have any unhandled extensions marked as critical.
    int extension_count = X509_get_ext_count(issuer);
    for (int i = 0; i < extension_count; ++i) {
      X509_EXTENSION* extension = X509_get_ext(issuer, i);
      if (extension->critical > 0) {
        const int nid = OBJ_obj2nid(extension->object);
        if (nid != NID_name_constraints && nid != NID_basic_constraints &&
            nid != NID_key_usage) {
          return CastCertError::kErrCertsVerifyGeneric;
        }
      }
    }

    int nid = OBJ_obj2nid(subject->sig_alg->algorithm);
    const EVP_MD* digest;
    switch (nid) {
      case NID_sha1WithRSAEncryption:
        digest = EVP_sha1();
        break;
      case NID_sha256WithRSAEncryption:
        digest = EVP_sha256();
        break;
      case NID_sha384WithRSAEncryption:
        digest = EVP_sha384();
        break;
      case NID_sha512WithRSAEncryption:
        digest = EVP_sha512();
        break;
      default:
        return CastCertError::kErrCertsVerifyGeneric;
    }
    if (!VerifySignedData(
            digest, public_key.get(),
            {subject->cert_info->enc.enc,
             static_cast<uint32_t>(subject->cert_info->enc.len)},
            {subject->signature->data,
             static_cast<uint32_t>(subject->signature->length)})) {
      return CastCertError::kErrCertsVerifyGeneric;
    }
  }
  // NOTE: Other half of ((!self-issued || target) -> check name constraints).
  for (NAME_CONSTRAINTS* name_constraints : path_name_constraints) {
    if (NAME_CONSTRAINTS_check(path.back().cert, name_constraints) !=
        X509_V_OK) {
      return CastCertError::kErrCertsVerifyGeneric;
    }
  }
  return error;
}

X509* ParseX509Der(const std::string& der) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(der.data());
  return d2i_X509(nullptr, &data, der.size());
}

CastDeviceCertPolicy GetAudioPolicy(const std::vector<CertPathStep>& path,
                                    uint32_t path_index) {
  // Cast device certificates use the policy 1.3.6.1.4.1.11129.2.5.2 to indicate
  // it is *restricted* to an audio-only device whereas the absence of a policy
  // means it is unrestricted.
  //
  // This is somewhat different than RFC 5280's notion of policies, so policies
  // are checked separately outside of path building.
  //
  // See the unit-tests VerifyCastDeviceCertTest.Policies* for some
  // concrete examples of how this works.
  //
  // Iterate over all the certificates, including the root certificate. If any
  // certificate contains the audio-only policy, the whole chain is considered
  // constrained to audio-only device certificates.
  //
  // Policy mappings are not accounted for. The expectation is that top-level
  // intermediates issued with audio-only will have no mappings. If subsequent
  // certificates in the chain do, it won't matter as the chain is already
  // restricted to being audio-only.
  CastDeviceCertPolicy policy = CastDeviceCertPolicy::kUnrestricted;
  for (uint32_t i = path_index;
       i < path.size() && policy != CastDeviceCertPolicy::kAudioOnly; ++i) {
    X509* cert = path[path.size() - 1 - i].cert;
    int pos = X509_get_ext_by_NID(cert, NID_certificate_policies, -1);
    if (pos != -1) {
      X509_EXTENSION* policies_extension = X509_get_ext(cert, pos);
      const uint8_t* in = policies_extension->value->data;
      CERTIFICATEPOLICIES* policies = d2i_CERTIFICATEPOLICIES(
          nullptr, &in, policies_extension->value->length);

      if (policies) {
        // Check for |audio_only_policy_oid| in the set of policies.
        uint32_t policy_count = sk_POLICYINFO_num(policies);
        for (uint32_t i = 0; i < policy_count; ++i) {
          POLICYINFO* info = sk_POLICYINFO_value(policies, i);
          const ConstDataSpan& audio_only_policy_oid = AudioOnlyPolicyOid();
          if (info->policyid->length ==
                  static_cast<int>(audio_only_policy_oid.length) &&
              memcmp(info->policyid->data, audio_only_policy_oid.data,
                     audio_only_policy_oid.length) == 0) {
            policy = CastDeviceCertPolicy::kAudioOnly;
            break;
          }
        }
        CERTIFICATEPOLICIES_free(policies);
      }
    }
  }
  return policy;
}

}  // namespace

// static
CastTrustStore* CastTrustStore::GetInstance() {
  static CastTrustStore* store = new CastTrustStore();
  return store;
}

CastTrustStore::CastTrustStore() : trust_store_(new TrustStore()) {
  trust_store_->certs.emplace_back(MakeTrustAnchor(kCastRootCaDer));
  trust_store_->certs.emplace_back(MakeTrustAnchor(kEurekaRootCaDer));
}

CastTrustStore::~CastTrustStore() = default;

openscreen::Error VerifyDeviceCert(
    const std::vector<std::string>& der_certs,
    const DateTime& time,
    std::unique_ptr<CertVerificationContext>* context,
    CastDeviceCertPolicy* policy,
    const CastCRL* crl,
    CRLPolicy crl_policy,
    TrustStore* trust_store) {
  if (!trust_store) {
    trust_store = CastTrustStore::GetInstance()->trust_store();
  }

  if (der_certs.empty()) {
    return CastCertError::kErrCertsMissing;
  }

  // Fail early if CRL is required but not provided.
  if (!crl && crl_policy == CRLPolicy::kCrlRequired) {
    return CastCertError::kErrCrlInvalid;
  }

  bssl::UniquePtr<X509> target_cert;
  std::vector<bssl::UniquePtr<X509>> intermediate_certs;
  target_cert.reset(ParseX509Der(der_certs[0]));
  if (!target_cert) {
    return CastCertError::kErrCertsParse;
  }
  for (size_t i = 1; i < der_certs.size(); ++i) {
    intermediate_certs.emplace_back(ParseX509Der(der_certs[i]));
    if (!intermediate_certs.back()) {
      return CastCertError::kErrCertsParse;
    }
  }

  // Basic checks on the target certificate.
  CastCertError error = VerifyCertTime(target_cert.get(), time);
  if (error != CastCertError::kNone) {
    return error;
  }
  bssl::UniquePtr<EVP_PKEY> public_key{X509_get_pubkey(target_cert.get())};
  if (!VerifyPublicKeyLength(public_key.get())) {
    return CastCertError::kErrCertsVerifyGeneric;
  }
  if (X509_ALGOR_cmp(target_cert.get()->sig_alg,
                     target_cert.get()->cert_info->signature) != 0) {
    return CastCertError::kErrCertsVerifyGeneric;
  }
  bssl::UniquePtr<ASN1_BIT_STRING> key_usage = GetKeyUsage(target_cert.get());
  if (!key_usage) {
    return CastCertError::kErrCertsRestrictions;
  }
  int bit =
      ASN1_BIT_STRING_get_bit(key_usage.get(), KeyUsageBits::kDigitalSignature);
  if (bit == 0) {
    return CastCertError::kErrCertsRestrictions;
  }

  X509* path_head = target_cert.get();
  std::vector<CertPathStep> path;

  // This vector isn't used as resizable, so instead we allocate the largest
  // possible single path up front.  This would be a single trusted cert, all
  // the intermediate certs used once, and the target cert.
  path.resize(1 + intermediate_certs.size() + 1);

  // Additionally, the path is slightly simpler to deal with if the list is
  // sorted from trust->target, so the path is actually built starting from the
  // end.
  uint32_t first_index = path.size() - 1;
  path[first_index].cert = path_head;

  // Index into |path| of the current frontier of path construction.
  uint32_t path_index = first_index;

  // Whether |path| has reached a certificate in |trust_store| and is ready for
  // verification.
  bool path_cert_in_trust_store = false;

  // Attempt to build a valid certificate chain from |target_cert| to a
  // certificate in |trust_store|.  This loop tries all possible paths in a
  // depth-first-search fashion.  If no valid paths are found, the error
  // returned is whatever the last error was from the last path tried.
  uint32_t trust_store_index = 0;
  uint32_t intermediate_cert_index = 0;
  CastCertError last_error = CastCertError::kNone;
  for (;;) {
    X509_NAME* target_issuer_name = X509_get_issuer_name(path_head);

    // The next issuer certificate to add to the current path.
    X509* next_issuer = nullptr;

    for (uint32_t i = trust_store_index; i < trust_store->certs.size(); ++i) {
      X509* trust_store_cert = trust_store->certs[i].get();
      X509_NAME* trust_store_cert_name =
          X509_get_subject_name(trust_store_cert);
      if (X509_NAME_cmp(trust_store_cert_name, target_issuer_name) == 0) {
        CertPathStep& next_step = path[--path_index];
        next_step.cert = trust_store_cert;
        next_step.trust_store_index = i + 1;
        next_step.intermediate_cert_index = 0;
        next_issuer = trust_store_cert;
        path_cert_in_trust_store = true;
        break;
      }
    }
    trust_store_index = 0;
    if (!next_issuer) {
      for (uint32_t i = intermediate_cert_index; i < intermediate_certs.size();
           ++i) {
        X509* intermediate_cert = intermediate_certs[i].get();
        X509_NAME* intermediate_cert_name =
            X509_get_subject_name(intermediate_cert);
        if (X509_NAME_cmp(intermediate_cert_name, target_issuer_name) == 0 &&
            !CertInPath(intermediate_cert_name, path, path_index,
                        first_index)) {
          CertPathStep& next_step = path[--path_index];
          next_step.cert = intermediate_cert;
          next_step.trust_store_index = trust_store->certs.size();
          next_step.intermediate_cert_index = i + 1;
          next_issuer = intermediate_cert;
          break;
        }
      }
    }
    intermediate_cert_index = 0;
    if (!next_issuer) {
      if (path_index == first_index) {
        // There are no more paths to try.  Ensure an error is returned.
        if (last_error == CastCertError::kNone) {
          return CastCertError::kErrCertsVerifyGeneric;
        }
        return last_error;
      } else {
        CertPathStep& last_step = path[path_index++];
        trust_store_index = last_step.trust_store_index;
        intermediate_cert_index = last_step.intermediate_cert_index;
        continue;
      }
    }

    // TODO(btolsch): Check against revocation list
    if (path_cert_in_trust_store) {
      last_error = VerifyCertificateChain(path, path_index, time);
      if (last_error != CastCertError::kNone) {
        CertPathStep& last_step = path[path_index++];
        trust_store_index = last_step.trust_store_index;
        intermediate_cert_index = last_step.intermediate_cert_index;
        path_cert_in_trust_store = false;
      } else {
        break;
      }
    }
    path_head = next_issuer;
  }

  if (last_error != CastCertError::kNone) {
    return last_error;
  }

  *policy = GetAudioPolicy(path, path_index);

  // Finally, make sure there is a common name to give to
  // CertVerificationContextImpl.
  X509_NAME* target_subject = X509_get_subject_name(target_cert.get());
  std::string common_name(target_subject->canon_enclen, 0);
  int len = X509_NAME_get_text_by_NID(target_subject, NID_commonName,
                                      &common_name[0], common_name.size());
  if (len == 0) {
    return CastCertError::kErrCertsRestrictions;
  }
  common_name.resize(len);

  context->reset(new CertVerificationContextImpl(
      bssl::UniquePtr<EVP_PKEY>{X509_get_pubkey(target_cert.get())},
      std::move(common_name)));

  return CastCertError::kNone;
}

}  // namespace certificate
}  // namespace cast
