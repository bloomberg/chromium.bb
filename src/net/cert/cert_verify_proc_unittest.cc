// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc.h"

#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_type.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc_builtin.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/ev_root_ca_metadata.h"
#include "net/cert/internal/parse_certificate.h"
#include "net/cert/internal/signature_algorithm.h"
#include "net/cert/pem.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/cert_net/cert_net_fetcher_url_request.h"
#include "net/der/input.h"
#include "net/der/parser.h"
#include "net/log/test_net_log.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/gtest_util.h"
#include "net/test/revocation_builder.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/mem.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#include "net/cert/cert_verify_proc_android.h"
#elif defined(OS_IOS)
#include "base/ios/ios_util.h"
#include "net/cert/cert_verify_proc_ios.h"
#elif defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "net/cert/cert_verify_proc_mac.h"
#include "net/cert/internal/trust_store_mac.h"
#elif defined(OS_WIN)
#include "base/win/windows_version.h"
#include "net/cert/cert_verify_proc_win.h"
#endif

// TODO(crbug.com/649017): Add tests that only certificates with
// serverAuth are accepted.

using net::test::IsError;
using net::test::IsOk;

using base::HexEncode;

namespace net {

namespace {

const char kTLSFeatureExtensionHistogram[] =
    "Net.Certificate.TLSFeatureExtensionWithPrivateRoot";
const char kTLSFeatureExtensionOCSPHistogram[] =
    "Net.Certificate.TLSFeatureExtensionWithPrivateRootHasOCSP";
const char kTrustAnchorVerifyHistogram[] = "Net.Certificate.TrustAnchor.Verify";
const char kTrustAnchorVerifyOutOfDateHistogram[] =
    "Net.Certificate.TrustAnchor.VerifyOutOfDate";

// Mock CertVerifyProc that sets the CertVerifyResult to a given value for
// all certificates that are Verify()'d
class MockCertVerifyProc : public CertVerifyProc {
 public:
  explicit MockCertVerifyProc(const CertVerifyResult& result)
      : result_(result) {}
  // CertVerifyProc implementation:
  bool SupportsAdditionalTrustAnchors() const override { return false; }

 protected:
  ~MockCertVerifyProc() override = default;

 private:
  int VerifyInternal(X509Certificate* cert,
                     const std::string& hostname,
                     const std::string& ocsp_response,
                     const std::string& sct_list,
                     int flags,
                     CRLSet* crl_set,
                     const CertificateList& additional_trust_anchors,
                     CertVerifyResult* verify_result,
                     const NetLogWithSource& net_log) override;

  const CertVerifyResult result_;

  DISALLOW_COPY_AND_ASSIGN(MockCertVerifyProc);
};

int MockCertVerifyProc::VerifyInternal(
    X509Certificate* cert,
    const std::string& hostname,
    const std::string& ocsp_response,
    const std::string& sct_list,
    int flags,
    CRLSet* crl_set,
    const CertificateList& additional_trust_anchors,
    CertVerifyResult* verify_result,
    const NetLogWithSource& net_log) {
  *verify_result = result_;
  verify_result->verified_cert = cert;
  return OK;
}

// This enum identifies a concrete implemenation of CertVerifyProc.
//
// The type is erased by CreateCertVerifyProc(), however needs to be known for
// some of the test expectations.
enum CertVerifyProcType {
  CERT_VERIFY_PROC_ANDROID,
  CERT_VERIFY_PROC_IOS,
  CERT_VERIFY_PROC_MAC,
  CERT_VERIFY_PROC_WIN,
  CERT_VERIFY_PROC_BUILTIN,
};

// Wrapper for base::mac::IsAtLeastOS10_12() to avoid littering ifdefs.
bool IsMacAtLeastOS10_12() {
#if defined(OS_MACOSX) && !defined(OS_IOS)
  return base::mac::IsAtLeastOS10_12();
#else
  return false;
#endif
}

// Returns a textual description of the CertVerifyProc implementation
// that is being tested, used to give better names to parameterized
// tests.
std::string VerifyProcTypeToName(
    const testing::TestParamInfo<CertVerifyProcType>& params) {
  switch (params.param) {
    case CERT_VERIFY_PROC_ANDROID:
      return "CertVerifyProcAndroid";
    case CERT_VERIFY_PROC_IOS:
      return "CertVerifyProcIOS";
    case CERT_VERIFY_PROC_MAC:
      return "CertVerifyProcMac";
    case CERT_VERIFY_PROC_WIN:
      return "CertVerifyProcWin";
    case CERT_VERIFY_PROC_BUILTIN:
      return "CertVerifyProcBuiltin";
  }

  return nullptr;
}

scoped_refptr<CertVerifyProc> CreateCertVerifyProc(
    CertVerifyProcType type,
    scoped_refptr<CertNetFetcher> cert_net_fetcher) {
  switch (type) {
#if defined(OS_ANDROID)
    case CERT_VERIFY_PROC_ANDROID:
      return new CertVerifyProcAndroid(std::move(cert_net_fetcher));
#elif defined(OS_IOS)
    case CERT_VERIFY_PROC_IOS:
      return new CertVerifyProcIOS();
#elif defined(OS_MACOSX)
    case CERT_VERIFY_PROC_MAC:
      return new CertVerifyProcMac();
#elif defined(OS_WIN)
    case CERT_VERIFY_PROC_WIN:
      return new CertVerifyProcWin();
#endif
    case CERT_VERIFY_PROC_BUILTIN:
      return CreateCertVerifyProcBuiltin(
          std::move(cert_net_fetcher),
          SystemTrustStoreProvider::CreateDefaultForSSL());
    default:
      return nullptr;
  }
}

// The set of all CertVerifyProcTypes that tests should be parameterized on.
// This needs to be kept in sync with CertVerifyProc::CreateSystemVerifyProc()
// and the platforms where CreateSslSystemTrustStore() is not a dummy store.
// TODO(crbug.com/649017): Enable CERT_VERIFY_PROC_BUILTIN everywhere. Right
// now this is gated on having CertVerifyProcBuiltin understand the roots added
// via TestRootCerts.
const std::vector<CertVerifyProcType> kAllCertVerifiers = {
#if defined(OS_ANDROID)
    CERT_VERIFY_PROC_ANDROID
#elif defined(OS_IOS)
    CERT_VERIFY_PROC_IOS
#elif defined(OS_MACOSX)
    CERT_VERIFY_PROC_MAC, CERT_VERIFY_PROC_BUILTIN
#elif defined(OS_WIN)
    CERT_VERIFY_PROC_WIN
#elif defined(OS_FUCHSIA) || defined(OS_LINUX) || defined(OS_CHROMEOS)
    CERT_VERIFY_PROC_BUILTIN
#else
#error Unsupported platform
#endif
};  // namespace

// Returns true if a test root added through ScopedTestRoot can verify
// successfully as a target certificate with chain of length 1 on the given
// CertVerifyProcType.
bool ScopedTestRootCanTrustTargetCert(CertVerifyProcType verify_proc_type) {
  return verify_proc_type == CERT_VERIFY_PROC_MAC ||
         verify_proc_type == CERT_VERIFY_PROC_IOS ||
         verify_proc_type == CERT_VERIFY_PROC_ANDROID;
}

// Returns true if a non-self-signed CA certificate added through
// ScopedTestRoot can verify successfully as the root of a chain by the given
// CertVerifyProcType.
bool ScopedTestRootCanTrustIntermediateCert(
    CertVerifyProcType verify_proc_type) {
  return verify_proc_type == CERT_VERIFY_PROC_MAC ||
         verify_proc_type == CERT_VERIFY_PROC_IOS ||
         verify_proc_type == CERT_VERIFY_PROC_BUILTIN ||
         verify_proc_type == CERT_VERIFY_PROC_ANDROID;
}

// TODO(crbug.com/649017): This is not parameterized by the CertVerifyProc
// because the CertVerifyProc::Verify() does this unconditionally based on the
// platform.
bool AreSHA1IntermediatesAllowed() {
#if defined(OS_WIN)
  // TODO(rsleevi): Remove this once https://crbug.com/588789 is resolved
  // for Windows 7/2008 users.
  // Note: This must be kept in sync with cert_verify_proc.cc
  return base::win::GetVersion() < base::win::Version::WIN8;
#else
  return false;
#endif
}

std::string MakeRandomHexString(size_t num_bytes) {
  std::vector<char> rand_bytes;
  rand_bytes.resize(num_bytes);

  base::RandBytes(rand_bytes.data(), rand_bytes.size());
  return base::HexEncode(rand_bytes.data(), rand_bytes.size());
}

}  // namespace

// This fixture is for tests that apply to concrete implementations of
// CertVerifyProc. It will be run for all of the concrete CertVerifyProc types.
//
// It is called "Internal" as it tests the internal methods like
// "VerifyInternal()".
class CertVerifyProcInternalTest
    : public testing::TestWithParam<CertVerifyProcType> {
 protected:
  void SetUp() override { SetUpWithCertNetFetcher(nullptr); }

  // CertNetFetcher may be initialized by subclasses that want to use net
  // fetching by calling SetUpWithCertNetFetcher instead of SetUp.
  void SetUpWithCertNetFetcher(scoped_refptr<CertNetFetcher> cert_net_fetcher) {
    CertVerifyProcType type = verify_proc_type();
    verify_proc_ = CreateCertVerifyProc(type, std::move(cert_net_fetcher));
    ASSERT_TRUE(verify_proc_);
  }

  int Verify(X509Certificate* cert,
             const std::string& hostname,
             int flags,
             CRLSet* crl_set,
             const CertificateList& additional_trust_anchors,
             CertVerifyResult* verify_result,
             const NetLogWithSource& net_log) {
    return verify_proc_->Verify(cert, hostname, /*ocsp_response=*/std::string(),
                                /*sct_list=*/std::string(), flags, crl_set,
                                additional_trust_anchors, verify_result,
                                net_log);
  }

  int Verify(X509Certificate* cert,
             const std::string& hostname,
             int flags,
             CRLSet* crl_set,
             const CertificateList& additional_trust_anchors,
             CertVerifyResult* verify_result) {
    return Verify(cert, hostname, flags, crl_set, additional_trust_anchors,
                  verify_result, NetLogWithSource());
  }

  CertVerifyProcType verify_proc_type() const { return GetParam(); }

  bool SupportsAdditionalTrustAnchors() const {
    return verify_proc_->SupportsAdditionalTrustAnchors();
  }

  bool SupportsReturningVerifiedChain() const {
#if defined(OS_ANDROID)
    // Before API level 17 (SDK_VERSION_JELLY_BEAN_MR1), Android does
    // not expose the APIs necessary to get at the verified
    // certificate chain.
    if (verify_proc_type() == CERT_VERIFY_PROC_ANDROID &&
        base::android::BuildInfo::GetInstance()->sdk_int() <
            base::android::SDK_VERSION_JELLY_BEAN_MR1)
      return false;
#endif
    return true;
  }

  // Returns true if the RSA/DSA keysize will be considered weak on the current
  // platform. IsInvalidRsaDsaKeySize should be checked prior, since some very
  // weak keys may be considered invalid.
  bool IsWeakRsaDsaKeySize(int size) const {
#if defined(OS_IOS)
    // Beginning with iOS 13, the minimum key size for RSA/DSA algorithms is
    // 2048 bits. See https://support.apple.com/en-us/HT210176
    if (verify_proc_type() == CERT_VERIFY_PROC_IOS &&
        base::ios::IsRunningOnIOS13OrLater()) {
      return size < 2048;
    }
#elif defined(OS_MACOSX)
    // Beginning with macOS 10.15, the minimum key size for RSA/DSA algorithms
    // is 2048 bits. See https://support.apple.com/en-us/HT210176
    if (verify_proc_type() == CERT_VERIFY_PROC_MAC &&
        base::mac::IsAtLeastOS10_15()) {
      return size < 2048;
    }
#endif

    return size < 1024;
  }

  // Returns true if the RSA/DSA keysize will be considered invalid on the
  // current platform.
  bool IsInvalidRsaDsaKeySize(int size) const {
#if defined(OS_IOS)
    if (base::ios::IsRunningOnIOS12OrLater()) {
      // On iOS using SecTrustEvaluateWithError it is not possible to
      // distinguish between weak and invalid key sizes.
      return IsWeakRsaDsaKeySize(size);
    }
#elif defined(OS_MACOSX)
    // Starting with Mac OS 10.12, certs with keys < 1024 are invalid.
    if (verify_proc_type() == CERT_VERIFY_PROC_MAC &&
        base::mac::IsAtLeastOS10_12()) {
      return size < 1024;
    }
#endif

    // This platform does not mark certificates with weak keys as invalid.
    return false;
  }

  static bool ParseKeyType(const std::string& key_type,
                           std::string* type,
                           int* size) {
    size_t pos = key_type.find("-");
    std::string size_str = key_type.substr(0, pos);
    *type = key_type.substr(pos + 1);
    return base::StringToInt(size_str, size);
  }

  // Some platforms may reject certificates with very weak keys as invalid.
  bool IsInvalidKeyType(const std::string& key_type) const {
    std::string type;
    int size = 0;
    if (!ParseKeyType(key_type, &type, &size))
      return false;

    if (type == "rsa" || type == "dsa")
      return IsInvalidRsaDsaKeySize(size);

    return false;
  }

  // Currently, only RSA and DSA keys are checked for weakness, and our example
  // weak size is 768. These could change in the future.
  //
  // Note that this means there may be false negatives: keys for other
  // algorithms and which are weak will pass this test.
  //
  // Also, IsInvalidKeyType should be checked prior, since some weak keys may be
  // considered invalid.
  bool IsWeakKeyType(const std::string& key_type) const {
    std::string type;
    int size = 0;
    if (!ParseKeyType(key_type, &type, &size))
      return false;

    if (type == "rsa" || type == "dsa")
      return IsWeakRsaDsaKeySize(size);

    return false;
  }

  bool SupportsCRLSet() const {
    return verify_proc_type() == CERT_VERIFY_PROC_WIN ||
           verify_proc_type() == CERT_VERIFY_PROC_MAC ||
           verify_proc_type() == CERT_VERIFY_PROC_BUILTIN;
  }

  bool SupportsCRLSetsInPathBuilding() const {
    return verify_proc_type() == CERT_VERIFY_PROC_WIN ||
           verify_proc_type() == CERT_VERIFY_PROC_BUILTIN;
  }

  bool SupportsEV() const {
    // TODO(crbug.com/117478): Android and iOS do not support EV.
    return verify_proc_type() == CERT_VERIFY_PROC_WIN ||
           verify_proc_type() == CERT_VERIFY_PROC_MAC ||
           verify_proc_type() == CERT_VERIFY_PROC_BUILTIN;
  }

  bool SupportsSoftFailRevChecking() const {
    return verify_proc_type() == CERT_VERIFY_PROC_WIN ||
           verify_proc_type() == CERT_VERIFY_PROC_MAC ||
           verify_proc_type() == CERT_VERIFY_PROC_BUILTIN;
  }

  bool SupportsRevCheckingRequiredLocalAnchors() const {
    return verify_proc_type() == CERT_VERIFY_PROC_WIN ||
           verify_proc_type() == CERT_VERIFY_PROC_BUILTIN;
  }

  CertVerifyProc* verify_proc() const { return verify_proc_.get(); }

 private:
  scoped_refptr<CertVerifyProc> verify_proc_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         CertVerifyProcInternalTest,
                         testing::ValuesIn(kAllCertVerifiers),
                         VerifyProcTypeToName);

// Tests that a certificate is recognized as EV, when the valid EV policy OID
// for the trust anchor is the second candidate EV oid in the target
// certificate. This is a regression test for crbug.com/705285.
// Started failing: https://crbug.com/1094358
TEST_P(CertVerifyProcInternalTest, DISABLED_EVVerificationMultipleOID) {
  if (!SupportsEV()) {
    LOG(INFO) << "Skipping test as EV verification is not yet supported";
    return;
  }

  // TODO(eroman): Update this test to use a synthetic certificate, so the test
  // does not break in the future. The certificate chain in question expires on
  // Jun 12 14:33:43 2020 GMT, at which point this test will start failing.
  if (base::Time::Now() >
      base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(1591972423)) {
    FAIL() << "This test uses a certificate chain which is now expired. Please "
              "disable and file a bug.";
    return;
  }

  scoped_refptr<X509Certificate> chain = CreateCertificateChainFromFile(
      GetTestCertsDirectory(), "login.trustwave.com.pem",
      X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  ASSERT_TRUE(chain);

  // Build a CRLSet that covers the target certificate.
  //
  // This way CRLSet coverage will be sufficient for EV revocation checking,
  // so this test does not depend on online revocation checking.
  ASSERT_GE(chain->intermediate_buffers().size(), 1u);
  base::StringPiece spki;
  ASSERT_TRUE(
      asn1::ExtractSPKIFromDERCert(x509_util::CryptoBufferAsStringPiece(
                                       chain->intermediate_buffers()[0].get()),
                                   &spki));
  SHA256HashValue spki_sha256;
  crypto::SHA256HashString(spki, spki_sha256.data, sizeof(spki_sha256.data));
  scoped_refptr<CRLSet> crl_set(
      CRLSet::ForTesting(false, &spki_sha256, "", "", {}));

  CertVerifyResult verify_result;
  int flags = 0;
  int error = Verify(chain.get(), "login.trustwave.com", flags, crl_set.get(),
                     CertificateList(), &verify_result);
  EXPECT_THAT(error, IsOk());
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_IS_EV);
}

// Target cert has an EV policy, and verifies successfully, but has a chain of
// length 1 because the target cert was directly trusted in the trust store.
// Should verify OK but not with STATUS_IS_EV.
TEST_P(CertVerifyProcInternalTest, TrustedTargetCertWithEVPolicy) {
  // The policy that "explicit-policy-chain.pem" target certificate asserts.
  static const char kEVTestCertPolicy[] = "1.2.3.4";
  ScopedTestEVPolicy scoped_test_ev_policy(
      EVRootCAMetadata::GetInstance(), SHA256HashValue(), kEVTestCertPolicy);

  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(GetTestCertsDirectory(), "explicit-policy-chain.pem");
  ASSERT_TRUE(cert);
  ScopedTestRoot scoped_test_root(cert.get());

  CertVerifyResult verify_result;
  int flags = 0;
  int error =
      Verify(cert.get(), "policy_test.example", flags,
             CRLSet::BuiltinCRLSet().get(), CertificateList(), &verify_result);
  if (ScopedTestRootCanTrustTargetCert(verify_proc_type())) {
    EXPECT_THAT(error, IsOk());
    ASSERT_TRUE(verify_result.verified_cert);
    EXPECT_TRUE(verify_result.verified_cert->intermediate_buffers().empty());
  } else {
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  }
  EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_IS_EV);
}

// Target cert has an EV policy, and verifies successfully with a chain of
// length 1, and its fingerprint matches the cert fingerprint for that ev
// policy. This should never happen in reality, but just test that things don't
// explode if it does.
TEST_P(CertVerifyProcInternalTest,
       TrustedTargetCertWithEVPolicyAndEVFingerprint) {
  // The policy that "explicit-policy-chain.pem" target certificate asserts.
  static const char kEVTestCertPolicy[] = "1.2.3.4";
  // This the fingerprint of the "explicit-policy-chain.pem" target certificate.
  // See net/data/ssl/certificates/explicit-policy-chain.pem
  static const SHA256HashValue kEVTestCertFingerprint = {
      {0x71, 0xac, 0xfa, 0x12, 0xa4, 0x42, 0x31, 0x3c, 0xff, 0x10, 0xd2,
       0x9d, 0xb6, 0x1b, 0x4a, 0xe8, 0x25, 0x4e, 0x77, 0xd3, 0x9f, 0xa3,
       0x2f, 0xb3, 0x19, 0x8d, 0x46, 0x9f, 0xb7, 0x73, 0x07, 0x30}};
  ScopedTestEVPolicy scoped_test_ev_policy(EVRootCAMetadata::GetInstance(),
                                           kEVTestCertFingerprint,
                                           kEVTestCertPolicy);

  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(GetTestCertsDirectory(), "explicit-policy-chain.pem");
  ASSERT_TRUE(cert);
  ScopedTestRoot scoped_test_root(cert.get());

  CertVerifyResult verify_result;
  int flags = 0;
  int error =
      Verify(cert.get(), "policy_test.example", flags,
             CRLSet::BuiltinCRLSet().get(), CertificateList(), &verify_result);
  if (ScopedTestRootCanTrustTargetCert(verify_proc_type())) {
    EXPECT_THAT(error, IsOk());
    ASSERT_TRUE(verify_result.verified_cert);
    EXPECT_TRUE(verify_result.verified_cert->intermediate_buffers().empty());
  } else {
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  }
  // An EV Root certificate should never be used as an end-entity certificate.
  EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_IS_EV);
}

// Target cert has an EV policy, and has a valid path to the EV root, but the
// intermediate has been trusted directly. Should stop building the path at the
// intermediate and verify OK but not with STATUS_IS_EV.
// See https://crbug.com/979801
TEST_P(CertVerifyProcInternalTest, TrustedIntermediateCertWithEVPolicy) {
  if (!SupportsEV()) {
    LOG(INFO) << "Skipping test as EV verification is not yet supported";
    return;
  }
  if (!ScopedTestRootCanTrustIntermediateCert(verify_proc_type())) {
    LOG(INFO) << "Skipping test as intermediate cert cannot be trusted";
    return;
  }

  CertificateList orig_certs = CreateCertificateListFromFile(
      GetTestCertsDirectory(), "explicit-policy-chain.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3U, orig_certs.size());

  for (bool trust_the_intermediate : {false, true}) {
    // Need to build unique certs for each try otherwise caching can break
    // things.
    CertBuilder root(orig_certs[2]->cert_buffer(), nullptr);
    CertBuilder intermediate(orig_certs[1]->cert_buffer(), &root);
    CertBuilder leaf(orig_certs[0]->cert_buffer(), &intermediate);

    // The policy that "explicit-policy-chain.pem" target certificate asserts.
    static const char kEVTestCertPolicy[] = "1.2.3.4";
    // Consider the root of the test chain a valid EV root for the test policy.
    ScopedTestEVPolicy scoped_test_ev_policy(
        EVRootCAMetadata::GetInstance(),
        X509Certificate::CalculateFingerprint256(root.GetCertBuffer()),
        kEVTestCertPolicy);

    // CRLSet which covers the leaf.
    base::StringPiece intermediate_spki;
    ASSERT_TRUE(asn1::ExtractSPKIFromDERCert(
        x509_util::CryptoBufferAsStringPiece(intermediate.GetCertBuffer()),
        &intermediate_spki));
    SHA256HashValue intermediate_spki_hash;
    crypto::SHA256HashString(intermediate_spki, &intermediate_spki_hash,
                             sizeof(SHA256HashValue));
    scoped_refptr<CRLSet> crl_set =
        CRLSet::ForTesting(false, &intermediate_spki_hash, "", "", {});

    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
    intermediates.push_back(bssl::UpRef(intermediate.GetCertBuffer()));
    scoped_refptr<X509Certificate> cert = X509Certificate::CreateFromBuffer(
        bssl::UpRef(leaf.GetCertBuffer()), std::move(intermediates));
    ASSERT_TRUE(cert.get());

    scoped_refptr<X509Certificate> intermediate_cert =
        X509Certificate::CreateFromBuffer(
            bssl::UpRef(intermediate.GetCertBuffer()), {});
    ASSERT_TRUE(intermediate_cert.get());

    scoped_refptr<X509Certificate> root_cert =
        X509Certificate::CreateFromBuffer(bssl::UpRef(root.GetCertBuffer()),
                                          {});
    ASSERT_TRUE(root_cert.get());

    if (!trust_the_intermediate) {
      // First trust just the root. This verifies that the test setup is
      // actually correct.
      ScopedTestRoot scoped_test_root({root_cert});
      CertVerifyResult verify_result;
      int flags = 0;
      int error = Verify(cert.get(), "policy_test.example", flags,
                         crl_set.get(), CertificateList(), &verify_result);
      EXPECT_THAT(error, IsOk());
      ASSERT_TRUE(verify_result.verified_cert);
      // Verified chain should include the intermediate and the root.
      EXPECT_EQ(2U, verify_result.verified_cert->intermediate_buffers().size());
      // Should be EV.
      EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_IS_EV);
    } else {
      // Now try with trusting both the intermediate and the root.
      ScopedTestRoot scoped_test_root({intermediate_cert, root_cert});
      CertVerifyResult verify_result;
      int flags = 0;
      int error = Verify(cert.get(), "policy_test.example", flags,
                         crl_set.get(), CertificateList(), &verify_result);
      EXPECT_THAT(error, IsOk());
      ASSERT_TRUE(verify_result.verified_cert);
      // Verified chain should only go to the trusted intermediate, not the
      // root.
      EXPECT_EQ(1U, verify_result.verified_cert->intermediate_buffers().size());
      // Should not be EV.
      EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_IS_EV);
    }
  }
}

TEST_P(CertVerifyProcInternalTest, CertWithNullInCommonNameAndNoSAN) {
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CertBuilder::CreateSimpleChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);

  leaf->EraseExtension(SubjectAltNameOid());

  std::string common_name;
  common_name += "www.fake.com";
  common_name += '\0';
  common_name += "a" + MakeRandomHexString(12) + ".example.com";
  leaf->SetSubjectCommonName(common_name);

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate().get());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), "www.fake.com", flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);

  // This actually fails because Chrome only looks for hostnames in
  // SubjectAltNames now and no SubjectAltName is present.
  EXPECT_THAT(error, IsError(ERR_CERT_COMMON_NAME_INVALID));
}

TEST_P(CertVerifyProcInternalTest, CertWithNullInCommonNameAndValidSAN) {
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CertBuilder::CreateSimpleChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);

  leaf->SetSubjectAltName("www.fake.com");

  std::string common_name;
  common_name += "www.fake.com";
  common_name += '\0';
  common_name += "a" + MakeRandomHexString(12) + ".example.com";
  leaf->SetSubjectCommonName(common_name);

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate().get());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), "www.fake.com", flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);

  // SubjectAltName is valid and Chrome does not use the common name.
  EXPECT_THAT(error, IsOk());
}

TEST_P(CertVerifyProcInternalTest, CertWithNullInSAN) {
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CertBuilder::CreateSimpleChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);

  std::string hostname;
  hostname += "www.fake.com";
  hostname += '\0';
  hostname += "a" + MakeRandomHexString(12) + ".example.com";
  leaf->SetSubjectAltName(hostname);

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate().get());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), "www.fake.com", flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);

  // SubjectAltName is invalid.
  EXPECT_THAT(error, IsError(ERR_CERT_COMMON_NAME_INVALID));
}

// Tests the case where the target certificate is accepted by
// X509CertificateBytes, but has errors that should cause verification to fail.
TEST_P(CertVerifyProcInternalTest, InvalidTarget) {
  base::FilePath certs_dir =
      GetTestNetDataDirectory().AppendASCII("parse_certificate_unittest");
  scoped_refptr<X509Certificate> bad_cert =
      ImportCertFromFile(certs_dir, "signature_algorithm_null.pem");
  ASSERT_TRUE(bad_cert);

  scoped_refptr<X509Certificate> ok_cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(ok_cert);

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(bssl::UpRef(ok_cert->cert_buffer()));
  scoped_refptr<X509Certificate> cert_with_bad_target(
      X509Certificate::CreateFromBuffer(bssl::UpRef(bad_cert->cert_buffer()),
                                        std::move(intermediates)));
  ASSERT_TRUE(cert_with_bad_target);
  EXPECT_EQ(1U, cert_with_bad_target->intermediate_buffers().size());

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(cert_with_bad_target.get(), "127.0.0.1", flags,
             CRLSet::BuiltinCRLSet().get(), CertificateList(), &verify_result);

  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_INVALID);
  EXPECT_THAT(error, IsError(ERR_CERT_INVALID));
}

// Tests the case where an intermediate certificate is accepted by
// X509CertificateBytes, but has errors that should prevent using it during
// verification.  The verification should succeed, since the intermediate
// wasn't necessary.
TEST_P(CertVerifyProcInternalTest, UnnecessaryInvalidIntermediate) {
  ScopedTestRoot test_root(
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem").get());

  base::FilePath certs_dir =
      GetTestNetDataDirectory().AppendASCII("parse_certificate_unittest");
  bssl::UniquePtr<CRYPTO_BUFFER> bad_cert =
      x509_util::CreateCryptoBuffer(base::StringPiece("invalid"));
  ASSERT_TRUE(bad_cert);

  scoped_refptr<X509Certificate> ok_cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(ok_cert);

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(std::move(bad_cert));
  scoped_refptr<X509Certificate> cert_with_bad_intermediate(
      X509Certificate::CreateFromBuffer(bssl::UpRef(ok_cert->cert_buffer()),
                                        std::move(intermediates)));
  ASSERT_TRUE(cert_with_bad_intermediate);
  EXPECT_EQ(1U, cert_with_bad_intermediate->intermediate_buffers().size());

  RecordingNetLogObserver net_log_observer(NetLogCaptureMode::kDefault);
  NetLogWithSource net_log(NetLogWithSource::Make(
      net::NetLog::Get(), net::NetLogSourceType::CERT_VERIFIER_TASK));
  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(cert_with_bad_intermediate.get(), "127.0.0.1", flags,
                     CRLSet::BuiltinCRLSet().get(), CertificateList(),
                     &verify_result, net_log);

  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0u, verify_result.cert_status);

  auto events = net_log_observer.GetEntriesForSource(net_log.source());
  EXPECT_FALSE(events.empty());

  auto event = std::find_if(events.begin(), events.end(), [](const auto& e) {
    return e.type == NetLogEventType::CERT_VERIFY_PROC;
  });
  ASSERT_NE(event, events.end());
  EXPECT_EQ(net::NetLogEventPhase::BEGIN, event->phase);
  ASSERT_TRUE(event->params.is_dict());
  const std::string* host = event->params.FindStringKey("host");
  ASSERT_TRUE(host);
  EXPECT_EQ("127.0.0.1", *host);

  if (verify_proc_type() == CERT_VERIFY_PROC_BUILTIN) {
    event = std::find_if(events.begin(), events.end(), [](const auto& e) {
      return e.type == NetLogEventType::CERT_VERIFY_PROC_INPUT_CERT;
    });
    ASSERT_NE(event, events.end());
    EXPECT_EQ(net::NetLogEventPhase::NONE, event->phase);
    ASSERT_TRUE(event->params.is_dict());
    const std::string* errors = event->params.FindStringKey("errors");
    ASSERT_TRUE(errors);
    EXPECT_EQ(
        "ERROR: Failed parsing Certificate SEQUENCE\nERROR: Failed parsing "
        "Certificate\n",
        *errors);
  }
}

// A regression test for http://crbug.com/31497.
TEST_P(CertVerifyProcInternalTest, IntermediateCARequireExplicitPolicy) {
  if (verify_proc_type() == CERT_VERIFY_PROC_ANDROID) {
    // Disabled on Android, as the Android verification libraries require an
    // explicit policy to be specified, even when anyPolicy is permitted.
    LOG(INFO) << "Skipping test on Android";
    return;
  }

  base::FilePath certs_dir = GetTestCertsDirectory();

  CertificateList certs = CreateCertificateListFromFile(
      certs_dir, "explicit-policy-chain.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3U, certs.size());

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(bssl::UpRef(certs[1]->cert_buffer()));

  scoped_refptr<X509Certificate> cert = X509Certificate::CreateFromBuffer(
      bssl::UpRef(certs[0]->cert_buffer()), std::move(intermediates));
  ASSERT_TRUE(cert.get());

  ScopedTestRoot scoped_root(certs[2].get());

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(cert.get(), "policy_test.example", flags,
             CRLSet::BuiltinCRLSet().get(), CertificateList(), &verify_result);
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0u, verify_result.cert_status);
}

TEST_P(CertVerifyProcInternalTest, RejectExpiredCert) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  // Load root_ca_cert.pem into the test root store.
  ScopedTestRoot test_root(
      ImportCertFromFile(certs_dir, "root_ca_cert.pem").get());

  scoped_refptr<X509Certificate> cert = CreateCertificateChainFromFile(
      certs_dir, "expired_cert.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert);
  ASSERT_EQ(0U, cert->intermediate_buffers().size());

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(cert.get(), "127.0.0.1", flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);
  EXPECT_THAT(error, IsError(ERR_CERT_DATE_INVALID));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_DATE_INVALID);
}

TEST_P(CertVerifyProcInternalTest, RejectWeakKeys) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  typedef std::vector<std::string> Strings;
  Strings key_types;

  // generate-weak-test-chains.sh currently has:
  //     key_types="768-rsa 1024-rsa 2048-rsa prime256v1-ecdsa"
  // We must use the same key types here. The filenames generated look like:
  //     2048-rsa-ee-by-768-rsa-intermediate.pem
  key_types.push_back("768-rsa");
  key_types.push_back("1024-rsa");
  key_types.push_back("2048-rsa");
  key_types.push_back("prime256v1-ecdsa");

  // Add the root that signed the intermediates for this test.
  scoped_refptr<X509Certificate> root_cert =
      ImportCertFromFile(certs_dir, "2048-rsa-root.pem");
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), root_cert.get());
  ScopedTestRoot scoped_root(root_cert.get());

  // Now test each chain.
  for (Strings::const_iterator ee_type = key_types.begin();
       ee_type != key_types.end(); ++ee_type) {
    for (Strings::const_iterator signer_type = key_types.begin();
         signer_type != key_types.end(); ++signer_type) {
      std::string basename =
          *ee_type + "-ee-by-" + *signer_type + "-intermediate.pem";
      SCOPED_TRACE(basename);
      scoped_refptr<X509Certificate> ee_cert =
          ImportCertFromFile(certs_dir, basename);
      ASSERT_NE(static_cast<X509Certificate*>(nullptr), ee_cert.get());

      basename = *signer_type + "-intermediate.pem";
      scoped_refptr<X509Certificate> intermediate =
          ImportCertFromFile(certs_dir, basename);
      ASSERT_NE(static_cast<X509Certificate*>(nullptr), intermediate.get());

      std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
      intermediates.push_back(bssl::UpRef(intermediate->cert_buffer()));
      scoped_refptr<X509Certificate> cert_chain =
          X509Certificate::CreateFromBuffer(bssl::UpRef(ee_cert->cert_buffer()),
                                            std::move(intermediates));
      ASSERT_TRUE(cert_chain);

      CertVerifyResult verify_result;
      int error = Verify(cert_chain.get(), "127.0.0.1", 0,
                         CRLSet::BuiltinCRLSet().get(), CertificateList(),
                         &verify_result);

      if (IsInvalidKeyType(*ee_type) || IsInvalidKeyType(*signer_type)) {
        EXPECT_NE(OK, error);
        EXPECT_EQ(CERT_STATUS_INVALID,
                  verify_result.cert_status & CERT_STATUS_INVALID);
      } else if (IsWeakKeyType(*ee_type) || IsWeakKeyType(*signer_type)) {
        EXPECT_NE(OK, error);
        EXPECT_EQ(CERT_STATUS_WEAK_KEY,
                  verify_result.cert_status & CERT_STATUS_WEAK_KEY);
        EXPECT_EQ(0u, verify_result.cert_status & CERT_STATUS_INVALID);
      } else {
        EXPECT_THAT(error, IsOk());
        EXPECT_EQ(0U, verify_result.cert_status & CERT_STATUS_WEAK_KEY);
      }
    }
  }
}

// Regression test for http://crbug.com/108514.
TEST_P(CertVerifyProcInternalTest, ExtraneousMD5RootCert) {
  if (!SupportsReturningVerifiedChain()) {
    LOG(INFO) << "Skipping this test in this platform.";
    return;
  }

  if (verify_proc_type() == CERT_VERIFY_PROC_MAC) {
    // Disabled on OS X - Security.framework doesn't ignore superflous
    // certificates provided by servers.
    // TODO(eroman): Is this still needed?
    LOG(INFO) << "Skipping this test as Security.framework doesn't ignore "
                 "superflous certificates provided by servers.";
    return;
  }

  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> server_cert =
      ImportCertFromFile(certs_dir, "cross-signed-leaf.pem");
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), server_cert.get());

  scoped_refptr<X509Certificate> extra_cert =
      ImportCertFromFile(certs_dir, "cross-signed-root-md5.pem");
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), extra_cert.get());

  scoped_refptr<X509Certificate> root_cert =
      ImportCertFromFile(certs_dir, "cross-signed-root-sha256.pem");
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), root_cert.get());

  ScopedTestRoot scoped_root(root_cert.get());

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(bssl::UpRef(extra_cert->cert_buffer()));
  scoped_refptr<X509Certificate> cert_chain = X509Certificate::CreateFromBuffer(
      bssl::UpRef(server_cert->cert_buffer()), std::move(intermediates));
  ASSERT_TRUE(cert_chain);

  CertVerifyResult verify_result;
  int flags = 0;
  int error =
      Verify(cert_chain.get(), "127.0.0.1", flags,
             CRLSet::BuiltinCRLSet().get(), CertificateList(), &verify_result);
  EXPECT_THAT(error, IsOk());

  // The extra MD5 root should be discarded
  ASSERT_TRUE(verify_result.verified_cert.get());
  ASSERT_EQ(1u, verify_result.verified_cert->intermediate_buffers().size());
  EXPECT_TRUE(x509_util::CryptoBufferEqual(
      verify_result.verified_cert->intermediate_buffers().front().get(),
      root_cert->cert_buffer()));

  EXPECT_FALSE(verify_result.has_md5);
}

// Test for bug 94673.
TEST_P(CertVerifyProcInternalTest, GoogleDigiNotarTest) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> server_cert =
      ImportCertFromFile(certs_dir, "google_diginotar.pem");
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), server_cert.get());

  scoped_refptr<X509Certificate> intermediate_cert =
      ImportCertFromFile(certs_dir, "diginotar_public_ca_2025.pem");
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), intermediate_cert.get());

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(bssl::UpRef(intermediate_cert->cert_buffer()));
  scoped_refptr<X509Certificate> cert_chain = X509Certificate::CreateFromBuffer(
      bssl::UpRef(server_cert->cert_buffer()), std::move(intermediates));
  ASSERT_TRUE(cert_chain);

  CertVerifyResult verify_result;
  int flags = CertVerifyProc::VERIFY_REV_CHECKING_ENABLED;
  int error =
      Verify(cert_chain.get(), "mail.google.com", flags,
             CRLSet::BuiltinCRLSet().get(), CertificateList(), &verify_result);
  EXPECT_NE(OK, error);

  // Now turn off revocation checking.  Certificate verification should still
  // fail.
  flags = 0;
  error =
      Verify(cert_chain.get(), "mail.google.com", flags,
             CRLSet::BuiltinCRLSet().get(), CertificateList(), &verify_result);
  EXPECT_NE(OK, error);
}

TEST_P(CertVerifyProcInternalTest, NameConstraintsOk) {
  CertificateList ca_cert_list =
      CreateCertificateListFromFile(GetTestCertsDirectory(), "root_ca_cert.pem",
                                    X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, ca_cert_list.size());
  ScopedTestRoot test_root(ca_cert_list[0].get());

  scoped_refptr<X509Certificate> leaf = CreateCertificateChainFromFile(
      GetTestCertsDirectory(), "name_constraint_good.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(leaf);
  ASSERT_EQ(0U, leaf->intermediate_buffers().size());

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(leaf.get(), "test.example.com", flags,
             CRLSet::BuiltinCRLSet().get(), CertificateList(), &verify_result);
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0U, verify_result.cert_status);

  error =
      Verify(leaf.get(), "foo.test2.example.com", flags,
             CRLSet::BuiltinCRLSet().get(), CertificateList(), &verify_result);
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0U, verify_result.cert_status);
}

// This fixture is for testing the verification of a certificate chain which
// has some sort of mismatched signature algorithm (i.e.
// Certificate.signatureAlgorithm and TBSCertificate.algorithm are different).
class CertVerifyProcInspectSignatureAlgorithmsTest : public ::testing::Test {
 protected:
  // In the test setup, SHA384 is given special treatment as an unknown
  // algorithm.
  static constexpr DigestAlgorithm kUnknownDigestAlgorithm =
      DigestAlgorithm::Sha384;

  struct CertParams {
    // Certificate.signatureAlgorithm
    DigestAlgorithm cert_algorithm;

    // TBSCertificate.algorithm
    DigestAlgorithm tbs_algorithm;
  };

  // On some platforms trying to import a certificate with mismatched signature
  // will fail. Consequently the rest of the tests can't be performed.
  WARN_UNUSED_RESULT bool SupportsImportingMismatchedAlgorithms() const {
#if defined(OS_IOS)
    LOG(INFO) << "Skipping test on iOS because certs with mismatched "
                 "algorithms cannot be imported";
    return false;
#elif defined(OS_MACOSX)
    if (base::mac::IsAtLeastOS10_12()) {
      LOG(INFO) << "Skipping test on macOS >= 10.12 because certs with "
                   "mismatched algorithms cannot be imported";
      return false;
    }
    return true;
#else
    return true;
#endif
  }

  // Shorthand for VerifyChain() where only the leaf's parameters need
  // to be specified.
  WARN_UNUSED_RESULT int VerifyLeaf(const CertParams& leaf_params) {
    return VerifyChain({// Target
                        leaf_params,
                        // Root
                        {DigestAlgorithm::Sha256, DigestAlgorithm::Sha256}});
  }

  // Shorthand for VerifyChain() where only the intermediate's parameters need
  // to be specified.
  WARN_UNUSED_RESULT int VerifyIntermediate(
      const CertParams& intermediate_params) {
    return VerifyChain({// Target
                        {DigestAlgorithm::Sha256, DigestAlgorithm::Sha256},
                        // Intermediate
                        intermediate_params,
                        // Root
                        {DigestAlgorithm::Sha256, DigestAlgorithm::Sha256}});
  }

  // Shorthand for VerifyChain() where only the root's parameters need to be
  // specified.
  WARN_UNUSED_RESULT int VerifyRoot(const CertParams& root_params) {
    return VerifyChain({// Target
                        {DigestAlgorithm::Sha256, DigestAlgorithm::Sha256},
                        // Intermediate
                        {DigestAlgorithm::Sha256, DigestAlgorithm::Sha256},
                        // Root
                        root_params});
  }

  // Manufactures a certificate chain where each certificate has the indicated
  // signature algorithms, and then returns the result of verifying this chain.
  //
  // TODO(eroman): Instead of building certificates at runtime, move their
  //               generation to external scripts.
  WARN_UNUSED_RESULT int VerifyChain(
      const std::vector<CertParams>& chain_params) {
    auto chain = CreateChain(chain_params);
    if (!chain) {
      ADD_FAILURE() << "Failed creating certificate chain";
      return ERR_UNEXPECTED;
    }

    int flags = 0;
    CertVerifyResult dummy_result;
    CertVerifyResult verify_result;

    scoped_refptr<CertVerifyProc> verify_proc =
        new MockCertVerifyProc(dummy_result);

    return verify_proc->Verify(
        chain.get(), "test.example.com", /*ocsp_response=*/std::string(),
        /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
        CertificateList(), &verify_result, NetLogWithSource());
  }

 private:
  // Overwrites the AlgorithmIdentifier pointed to by |algorithm_sequence| with
  // |algorithm|. Note this violates the constness of StringPiece.
  WARN_UNUSED_RESULT static bool SetAlgorithmSequence(
      DigestAlgorithm algorithm,
      base::StringPiece* algorithm_sequence) {
    // This string of bytes is the full SEQUENCE for an AlgorithmIdentifier.
    std::vector<uint8_t> replacement_sequence;
    switch (algorithm) {
      case DigestAlgorithm::Sha1:
        // sha1WithRSAEncryption
        replacement_sequence = {0x30, 0x0D, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
                                0xf7, 0x0d, 0x01, 0x01, 0x05, 0x05, 0x00};
        break;
      case DigestAlgorithm::Sha256:
        // sha256WithRSAEncryption
        replacement_sequence = {0x30, 0x0D, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
                                0xf7, 0x0d, 0x01, 0x01, 0x0b, 0x05, 0x00};
        break;
      case kUnknownDigestAlgorithm:
        // This shouldn't be anything meaningful (modified numbers at random).
        replacement_sequence = {0x30, 0x0D, 0x06, 0x09, 0x8a, 0x87, 0x18, 0x46,
                                0xd7, 0x0d, 0x01, 0x01, 0x0b, 0x05, 0x00};
        break;
      default:
        ADD_FAILURE() << "Unsupported digest algorithm";
        return false;
    }

    // For this simple replacement to work (without modifying any
    // other sequence lengths) the original algorithm and replacement
    // algorithm must have the same encoded length.
    if (algorithm_sequence->size() != replacement_sequence.size()) {
      ADD_FAILURE() << "AlgorithmIdentifier must have length "
                    << replacement_sequence.size();
      return false;
    }

    memcpy(const_cast<char*>(algorithm_sequence->data()),
           replacement_sequence.data(), replacement_sequence.size());
    return true;
  }

  // Locate the serial number bytes.
  WARN_UNUSED_RESULT static bool ExtractSerialNumberFromDERCert(
      base::StringPiece der_cert,
      base::StringPiece* serial_value) {
    der::Parser parser((der::Input(der_cert)));
    der::Parser certificate;
    if (!parser.ReadSequence(&certificate))
      return false;

    der::Parser tbs_certificate;
    if (!certificate.ReadSequence(&tbs_certificate))
      return false;

    bool unused;
    if (!tbs_certificate.SkipOptionalTag(
            der::kTagConstructed | der::kTagContextSpecific | 0, &unused)) {
      return false;
    }

    // serialNumber
    der::Input serial_value_der;
    if (!tbs_certificate.ReadTag(der::kInteger, &serial_value_der))
      return false;

    *serial_value = serial_value_der.AsStringPiece();
    return true;
  }

  // Creates a certificate (based on some base certificate file) using the
  // specified signature algorithms.
  static scoped_refptr<X509Certificate> CreateCertificate(
      const CertParams& params) {
    // Dosn't really matter which base certificate is used, so long as it is
    // valid and uses a signature AlgorithmIdentifier with the same encoded
    // length as sha1WithRSASignature.
    const char* kLeafFilename = "name_constraint_good.pem";

    auto cert = CreateCertificateChainFromFile(
        GetTestCertsDirectory(), kLeafFilename, X509Certificate::FORMAT_AUTO);
    if (!cert) {
      ADD_FAILURE() << "Failed to load certificate: " << kLeafFilename;
      return nullptr;
    }

    // Start with the DER bytes of a valid certificate. The der data is copied
    // to a new std::string as it will modified to create a new certificate.
    std::string cert_der(
        x509_util::CryptoBufferAsStringPiece(cert->cert_buffer()));

    // Parse the certificate and identify the locations of interest within
    // |cert_der|.
    base::StringPiece cert_algorithm_sequence;
    base::StringPiece tbs_algorithm_sequence;
    if (!asn1::ExtractSignatureAlgorithmsFromDERCert(
            cert_der, &cert_algorithm_sequence, &tbs_algorithm_sequence)) {
      ADD_FAILURE() << "Failed parsing certificate algorithms";
      return nullptr;
    }

    base::StringPiece serial_value;
    if (!ExtractSerialNumberFromDERCert(cert_der, &serial_value)) {
      ADD_FAILURE() << "Failed parsing certificate serial number";
      return nullptr;
    }

    // Give each certificate a unique serial number based on its content (which
    // in turn is a function of |params|, otherwise importing it may fail.

    // Upper bound for last entry in DigestAlgorithm
    const int kNumDigestAlgorithms = 15;
    *const_cast<char*>(serial_value.data()) +=
        static_cast<int>(params.tbs_algorithm) * kNumDigestAlgorithms +
        static_cast<int>(params.cert_algorithm);

    // Change the signature AlgorithmIdentifiers.
    if (!SetAlgorithmSequence(params.cert_algorithm,
                              &cert_algorithm_sequence) ||
        !SetAlgorithmSequence(params.tbs_algorithm, &tbs_algorithm_sequence)) {
      return nullptr;
    }

    // NOTE: The signature is NOT recomputed over TBSCertificate -- for these
    // tests it isn't needed.
    return X509Certificate::CreateFromBytes(cert_der.data(), cert_der.size());
  }

  static scoped_refptr<X509Certificate> CreateChain(
      const std::vector<CertParams>& params) {
    // Manufacture a chain with the given combinations of signature algorithms.
    // This chain isn't actually a valid chain, but it is good enough for
    // testing the base CertVerifyProc.
    CertificateList certs;
    for (const auto& cert_params : params) {
      certs.push_back(CreateCertificate(cert_params));
      if (!certs.back())
        return nullptr;
    }

    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
    for (size_t i = 1; i < certs.size(); ++i)
      intermediates.push_back(bssl::UpRef(certs[i]->cert_buffer()));

    return X509Certificate::CreateFromBuffer(
        bssl::UpRef(certs[0]->cert_buffer()), std::move(intermediates));
  }
};

// This is a control test to make sure that the test helper
// VerifyLeaf() works as expected. There is no actual mismatch in the
// algorithms used here.
//
//  Certificate.signatureAlgorithm:  sha1WithRSASignature
//  TBSCertificate.algorithm:        sha1WithRSAEncryption
TEST_F(CertVerifyProcInspectSignatureAlgorithmsTest, LeafSha1Sha1) {
  int rv = VerifyLeaf({DigestAlgorithm::Sha1, DigestAlgorithm::Sha1});
  ASSERT_THAT(rv, IsError(ERR_CERT_WEAK_SIGNATURE_ALGORITHM));
}

// This is a control test to make sure that the test helper
// VerifyLeaf() works as expected. There is no actual mismatch in the
// algorithms used here.
//
//  Certificate.signatureAlgorithm:  sha256WithRSASignature
//  TBSCertificate.algorithm:        sha256WithRSAEncryption
TEST_F(CertVerifyProcInspectSignatureAlgorithmsTest, LeafSha256Sha256) {
  int rv = VerifyLeaf({DigestAlgorithm::Sha256, DigestAlgorithm::Sha256});
  ASSERT_THAT(rv, IsOk());
}

// Mismatched signature algorithms in the leaf certificate.
//
//  Certificate.signatureAlgorithm:  sha1WithRSASignature
//  TBSCertificate.algorithm:        sha256WithRSAEncryption
TEST_F(CertVerifyProcInspectSignatureAlgorithmsTest, LeafSha1Sha256) {
  if (!SupportsImportingMismatchedAlgorithms())
    return;

  int rv = VerifyLeaf({DigestAlgorithm::Sha1, DigestAlgorithm::Sha256});
  ASSERT_THAT(rv, IsError(ERR_CERT_INVALID));
}

// Mismatched signature algorithms in the leaf certificate.
//
//  Certificate.signatureAlgorithm:  sha256WithRSAEncryption
//  TBSCertificate.algorithm:        sha1WithRSASignature
TEST_F(CertVerifyProcInspectSignatureAlgorithmsTest, LeafSha256Sha1) {
  if (!SupportsImportingMismatchedAlgorithms())
    return;

  int rv = VerifyLeaf({DigestAlgorithm::Sha256, DigestAlgorithm::Sha1});
  ASSERT_THAT(rv, IsError(ERR_CERT_INVALID));
}

// Unrecognized signature algorithm in the leaf certificate.
//
//  Certificate.signatureAlgorithm:  sha256WithRSAEncryption
//  TBSCertificate.algorithm:        ?
TEST_F(CertVerifyProcInspectSignatureAlgorithmsTest, LeafSha256Unknown) {
  if (!SupportsImportingMismatchedAlgorithms())
    return;

  int rv = VerifyLeaf({DigestAlgorithm::Sha256, kUnknownDigestAlgorithm});
  ASSERT_THAT(rv, IsError(ERR_CERT_INVALID));
}

// Unrecognized signature algorithm in the leaf certificate.
//
//  Certificate.signatureAlgorithm:  ?
//  TBSCertificate.algorithm:        sha256WithRSAEncryption
TEST_F(CertVerifyProcInspectSignatureAlgorithmsTest, LeafUnknownSha256) {
  if (!SupportsImportingMismatchedAlgorithms())
    return;

  int rv = VerifyLeaf({kUnknownDigestAlgorithm, DigestAlgorithm::Sha256});
  ASSERT_THAT(rv, IsError(ERR_CERT_INVALID));
}

// Mismatched signature algorithms in the intermediate certificate.
//
//  Certificate.signatureAlgorithm:  sha1WithRSASignature
//  TBSCertificate.algorithm:        sha256WithRSAEncryption
TEST_F(CertVerifyProcInspectSignatureAlgorithmsTest, IntermediateSha1Sha256) {
  if (!SupportsImportingMismatchedAlgorithms())
    return;

  int rv = VerifyIntermediate({DigestAlgorithm::Sha1, DigestAlgorithm::Sha256});
  ASSERT_THAT(rv, IsError(ERR_CERT_INVALID));
}

// Mismatched signature algorithms in the intermediate certificate.
//
//  Certificate.signatureAlgorithm:  sha256WithRSAEncryption
//  TBSCertificate.algorithm:        sha1WithRSASignature
TEST_F(CertVerifyProcInspectSignatureAlgorithmsTest, IntermediateSha256Sha1) {
  if (!SupportsImportingMismatchedAlgorithms())
    return;

  int rv = VerifyIntermediate({DigestAlgorithm::Sha256, DigestAlgorithm::Sha1});
  ASSERT_THAT(rv, IsError(ERR_CERT_INVALID));
}

// Mismatched signature algorithms in the root certificate.
//
//  Certificate.signatureAlgorithm:  sha256WithRSAEncryption
//  TBSCertificate.algorithm:        sha1WithRSASignature
TEST_F(CertVerifyProcInspectSignatureAlgorithmsTest, RootSha256Sha1) {
  if (!SupportsImportingMismatchedAlgorithms())
    return;

  int rv = VerifyRoot({DigestAlgorithm::Sha256, DigestAlgorithm::Sha1});
  ASSERT_THAT(rv, IsOk());
}

// Unrecognized signature algorithm in the root certificate.
//
//  Certificate.signatureAlgorithm:  ?
//  TBSCertificate.algorithm:        sha256WithRSAEncryption
TEST_F(CertVerifyProcInspectSignatureAlgorithmsTest, RootUnknownSha256) {
  if (!SupportsImportingMismatchedAlgorithms())
    return;

  int rv = VerifyRoot({kUnknownDigestAlgorithm, DigestAlgorithm::Sha256});
  ASSERT_THAT(rv, IsOk());
}

TEST_P(CertVerifyProcInternalTest, NameConstraintsFailure) {
  if (!SupportsReturningVerifiedChain()) {
    LOG(INFO) << "Skipping this test in this platform.";
    return;
  }

  CertificateList ca_cert_list =
      CreateCertificateListFromFile(GetTestCertsDirectory(), "root_ca_cert.pem",
                                    X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, ca_cert_list.size());
  ScopedTestRoot test_root(ca_cert_list[0].get());

  CertificateList cert_list = CreateCertificateListFromFile(
      GetTestCertsDirectory(), "name_constraint_bad.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, cert_list.size());

  scoped_refptr<X509Certificate> leaf = X509Certificate::CreateFromBuffer(
      bssl::UpRef(cert_list[0]->cert_buffer()), {});
  ASSERT_TRUE(leaf);

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(leaf.get(), "test.example.com", flags,
             CRLSet::BuiltinCRLSet().get(), CertificateList(), &verify_result);
  EXPECT_THAT(error, IsError(ERR_CERT_NAME_CONSTRAINT_VIOLATION));
  EXPECT_EQ(CERT_STATUS_NAME_CONSTRAINT_VIOLATION,
            verify_result.cert_status & CERT_STATUS_NAME_CONSTRAINT_VIOLATION);
}

TEST(CertVerifyProcTest, TestHasTooLongValidity) {
  struct {
    const char* const file;
    bool is_valid_too_long;
  } tests[] = {
      {"daltonridgeapts.com-chain.pem", false},
      {"start_after_expiry.pem", true},
      {"pre_br_validity_ok.pem", false},
      {"pre_br_validity_bad_121.pem", true},
      {"pre_br_validity_bad_2020.pem", true},
      {"10_year_validity.pem", false},
      {"11_year_validity.pem", true},
      {"39_months_after_2015_04.pem", false},
      {"40_months_after_2015_04.pem", true},
      {"60_months_after_2012_07.pem", false},
      {"61_months_after_2012_07.pem", true},
      {"825_days_after_2018_03_01.pem", false},
      {"826_days_after_2018_03_01.pem", true},
      {"825_days_1_second_after_2018_03_01.pem", true},
      {"39_months_based_on_last_day.pem", false},
  };

  base::FilePath certs_dir = GetTestCertsDirectory();

  for (const auto& test : tests) {
    SCOPED_TRACE(test.file);

    scoped_refptr<X509Certificate> certificate =
        ImportCertFromFile(certs_dir, test.file);
    ASSERT_TRUE(certificate);
    EXPECT_EQ(test.is_valid_too_long,
              CertVerifyProc::HasTooLongValidity(*certificate));
  }
}

TEST_P(CertVerifyProcInternalTest, TestKnownRoot) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> cert_chain = CreateCertificateChainFromFile(
      certs_dir, "daltonridgeapts.com-chain.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert_chain);

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(cert_chain.get(), "daltonridgeapts.com", flags,
             CRLSet::BuiltinCRLSet().get(), CertificateList(), &verify_result);
  EXPECT_THAT(error, IsOk()) << "This test relies on a real certificate that "
                             << "expires on May 28, 2021. If failing on/after "
                             << "that date, please disable and file a bug "
                             << "against rsleevi.";
  EXPECT_TRUE(verify_result.is_issued_by_known_root);
#if defined(OS_MACOSX) && !defined(OS_IOS)
  if (verify_proc_type() == CERT_VERIFY_PROC_BUILTIN) {
    auto* mac_trust_debug_info =
        net::TrustStoreMac::ResultDebugData::Get(&verify_result);
    ASSERT_TRUE(mac_trust_debug_info);
    // Since this test queries the real trust store, can't know exactly
    // what bits should be set in the trust debug info, but it should at
    // least have something set.
    EXPECT_NE(0, mac_trust_debug_info->combined_trust_debug_info());
  }
#endif
}

// This tests that on successful certificate verification,
// CertVerifyResult::public_key_hashes is filled with a SHA256 hash for each
// of the certificates in the chain.
TEST_P(CertVerifyProcInternalTest, PublicKeyHashes) {
  if (!SupportsReturningVerifiedChain()) {
    LOG(INFO) << "Skipping this test in this platform.";
    return;
  }

  base::FilePath certs_dir = GetTestCertsDirectory();
  CertificateList certs = CreateCertificateListFromFile(
      certs_dir, "x509_verify_results.chain.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3U, certs.size());

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(bssl::UpRef(certs[1]->cert_buffer()));
  intermediates.push_back(bssl::UpRef(certs[2]->cert_buffer()));

  ScopedTestRoot scoped_root(certs[2].get());
  scoped_refptr<X509Certificate> cert_chain = X509Certificate::CreateFromBuffer(
      bssl::UpRef(certs[0]->cert_buffer()), std::move(intermediates));
  ASSERT_TRUE(cert_chain);
  ASSERT_EQ(2U, cert_chain->intermediate_buffers().size());

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(cert_chain.get(), "127.0.0.1", flags,
             CRLSet::BuiltinCRLSet().get(), CertificateList(), &verify_result);
  EXPECT_THAT(error, IsOk());

  EXPECT_EQ(3u, verify_result.public_key_hashes.size());

  // Convert |public_key_hashes| to strings for ease of comparison.
  std::vector<std::string> public_key_hash_strings;
  for (const auto& public_key_hash : verify_result.public_key_hashes)
    public_key_hash_strings.push_back(public_key_hash.ToString());

  std::vector<std::string> expected_public_key_hashes = {
      // Target
      "sha256/DZMTp9cNNYkzUG6baDB6T306ekLUYJpeEEtYpaeQpYE=",

      // Intermediate
      "sha256/D9u0epgvPYlG9YiVp7V+IMT+xhUpB5BhsS/INjDXc4Y=",

      // Trust anchor
      "sha256/VypP3VWL7OaqTJ7mIBehWYlv8khPuFHpWiearZI2YjI="};

  // |public_key_hashes| does not have an ordering guarantee.
  EXPECT_THAT(expected_public_key_hashes,
              testing::UnorderedElementsAreArray(public_key_hash_strings));
}

// A regression test for http://crbug.com/70293.
// The certificate in question has a key purpose of clientAuth, and also lacks
// the required key usage for serverAuth.
// TODO(mattm): This cert fails for many reasons, replace with a generated one
// that tests only the desired case.
TEST_P(CertVerifyProcInternalTest, WrongKeyPurpose) {
  base::FilePath certs_dir = GetTestCertsDirectory();

  scoped_refptr<X509Certificate> server_cert =
      ImportCertFromFile(certs_dir, "invalid_key_usage_cert.der");
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), server_cert.get());

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(server_cert.get(), "jira.aquameta.com", flags,
             CRLSet::BuiltinCRLSet().get(), CertificateList(), &verify_result);

  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_COMMON_NAME_INVALID);

#if defined(OS_IOS)
  if (verify_proc_type() == CERT_VERIFY_PROC_IOS) {
    if (base::ios::IsRunningOnIOS13OrLater() ||
        !base::ios::IsRunningOnIOS12OrLater()) {
      EXPECT_THAT(error, IsError(ERR_CERT_INVALID));
    } else {
      EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
    }
    return;
  }
#endif

  // TODO(crbug.com/649017): Don't special-case builtin verifier.
  if (verify_proc_type() != CERT_VERIFY_PROC_BUILTIN)
    EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_INVALID);

  if (verify_proc_type() != CERT_VERIFY_PROC_ANDROID) {
    // The certificate is issued by an unknown CA.
    EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_AUTHORITY_INVALID);
  }

  // TODO(crbug.com/649017): Don't special-case builtin verifier.
  if (verify_proc_type() == CERT_VERIFY_PROC_BUILTIN) {
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  } else {
    EXPECT_THAT(error, IsError(ERR_CERT_INVALID));
  }
}

// Tests that a Netscape Server Gated crypto is accepted in place of a
// serverAuth EKU.
// TODO(crbug.com/843735): Deprecate support for this.
TEST_P(CertVerifyProcInternalTest, Sha1IntermediateUsesServerGatedCrypto) {
  base::FilePath certs_dir =
      GetTestNetDataDirectory()
          .AppendASCII("verify_certificate_chain_unittest")
          .AppendASCII("intermediate-eku-server-gated-crypto");

  scoped_refptr<X509Certificate> cert_chain = CreateCertificateChainFromFile(
      certs_dir, "sha1-chain.pem", X509Certificate::FORMAT_AUTO);

  ASSERT_TRUE(cert_chain);
  ASSERT_FALSE(cert_chain->intermediate_buffers().empty());

  auto root = X509Certificate::CreateFromBuffer(
      bssl::UpRef(cert_chain->intermediate_buffers().back().get()), {});

  ScopedTestRoot scoped_root(root.get());

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(cert_chain.get(), "test.example", flags,
             CRLSet::BuiltinCRLSet().get(), CertificateList(), &verify_result);

  if (AreSHA1IntermediatesAllowed()) {
    EXPECT_THAT(error, IsOk());
    EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_SHA1_SIGNATURE_PRESENT);
  } else {
    EXPECT_NE(error, OK);
    EXPECT_TRUE(verify_result.cert_status &
                CERT_STATUS_WEAK_SIGNATURE_ALGORITHM);
    EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_SHA1_SIGNATURE_PRESENT);
  }
}

// Basic test for returning the chain in CertVerifyResult. Note that the
// returned chain may just be a reflection of the originally supplied chain;
// that is, if any errors occur, the default chain returned is an exact copy
// of the certificate to be verified. The remaining VerifyReturn* tests are
// used to ensure that the actual, verified chain is being returned by
// Verify().
TEST_P(CertVerifyProcInternalTest, VerifyReturnChainBasic) {
  if (!SupportsReturningVerifiedChain()) {
    LOG(INFO) << "Skipping this test in this platform.";
    return;
  }

  base::FilePath certs_dir = GetTestCertsDirectory();
  CertificateList certs = CreateCertificateListFromFile(
      certs_dir, "x509_verify_results.chain.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3U, certs.size());

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(bssl::UpRef(certs[1]->cert_buffer()));
  intermediates.push_back(bssl::UpRef(certs[2]->cert_buffer()));

  ScopedTestRoot scoped_root(certs[2].get());

  scoped_refptr<X509Certificate> google_full_chain =
      X509Certificate::CreateFromBuffer(bssl::UpRef(certs[0]->cert_buffer()),
                                        std::move(intermediates));
  ASSERT_NE(static_cast<X509Certificate*>(nullptr), google_full_chain.get());
  ASSERT_EQ(2U, google_full_chain->intermediate_buffers().size());

  CertVerifyResult verify_result;
  EXPECT_EQ(static_cast<X509Certificate*>(nullptr),
            verify_result.verified_cert.get());
  int error =
      Verify(google_full_chain.get(), "127.0.0.1", 0,
             CRLSet::BuiltinCRLSet().get(), CertificateList(), &verify_result);
  EXPECT_THAT(error, IsOk());
  ASSERT_NE(static_cast<X509Certificate*>(nullptr),
            verify_result.verified_cert.get());

  EXPECT_NE(google_full_chain, verify_result.verified_cert);
  EXPECT_TRUE(
      x509_util::CryptoBufferEqual(google_full_chain->cert_buffer(),
                                   verify_result.verified_cert->cert_buffer()));
  const auto& return_intermediates =
      verify_result.verified_cert->intermediate_buffers();
  ASSERT_EQ(2U, return_intermediates.size());
  EXPECT_TRUE(x509_util::CryptoBufferEqual(return_intermediates[0].get(),
                                           certs[1]->cert_buffer()));
  EXPECT_TRUE(x509_util::CryptoBufferEqual(return_intermediates[1].get(),
                                           certs[2]->cert_buffer()));
}

// Test that certificates issued for 'intranet' names (that is, containing no
// known public registry controlled domain information) issued by well-known
// CAs are flagged appropriately, while certificates that are issued by
// internal CAs are not flagged.
TEST(CertVerifyProcTest, IntranetHostsRejected) {
  CertificateList cert_list = CreateCertificateListFromFile(
      GetTestCertsDirectory(), "reject_intranet_hosts.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, cert_list.size());
  scoped_refptr<X509Certificate> cert(cert_list[0]);

  CertVerifyResult verify_result;
  int error = 0;

  // Intranet names for public CAs should be flagged:
  CertVerifyResult dummy_result;
  dummy_result.is_issued_by_known_root = true;
  scoped_refptr<CertVerifyProc> verify_proc =
      new MockCertVerifyProc(dummy_result);
  error = verify_proc->Verify(
      cert.get(), "webmail", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), 0, CRLSet::BuiltinCRLSet().get(),
      CertificateList(), &verify_result, NetLogWithSource());
  EXPECT_THAT(error, IsOk());
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_NON_UNIQUE_NAME);

  // However, if the CA is not well known, these should not be flagged:
  dummy_result.Reset();
  dummy_result.is_issued_by_known_root = false;
  verify_proc = base::MakeRefCounted<MockCertVerifyProc>(dummy_result);
  error = verify_proc->Verify(
      cert.get(), "webmail", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), 0, CRLSet::BuiltinCRLSet().get(),
      CertificateList(), &verify_result, NetLogWithSource());
  EXPECT_THAT(error, IsOk());
  EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_NON_UNIQUE_NAME);
}

// Tests that certificates issued by Symantec's legacy infrastructure
// are rejected according to the policies outlined in
// https://security.googleblog.com/2017/09/chromes-plan-to-distrust-symantec.html
// unless the caller has explicitly disabled that enforcement.
TEST(CertVerifyProcTest, SymantecCertsRejected) {
  constexpr SHA256HashValue kSymantecHashValue = {
      {0xb2, 0xde, 0xf5, 0x36, 0x2a, 0xd3, 0xfa, 0xcd, 0x04, 0xbd, 0x29,
       0x04, 0x7a, 0x43, 0x84, 0x4f, 0x76, 0x70, 0x34, 0xea, 0x48, 0x92,
       0xf8, 0x0e, 0x56, 0xbe, 0xe6, 0x90, 0x24, 0x3e, 0x25, 0x02}};
  constexpr SHA256HashValue kGoogleHashValue = {
      {0xec, 0x72, 0x29, 0x69, 0xcb, 0x64, 0x20, 0x0a, 0xb6, 0x63, 0x8f,
       0x68, 0xac, 0x53, 0x8e, 0x40, 0xab, 0xab, 0x5b, 0x19, 0xa6, 0x48,
       0x56, 0x61, 0x04, 0x2a, 0x10, 0x61, 0xc4, 0x61, 0x27, 0x76}};

  // Test that certificates from the legacy Symantec infrastructure are
  // rejected:
  // - dec_2017.pem : A certificate issued after 2017-12-01, which is rejected
  //                  as of M65
  // - pre_june_2016.pem : A certificate issued prior to 2016-06-01, which is
  //                       rejected as of M66.
  for (const char* rejected_cert : {"dec_2017.pem", "pre_june_2016.pem"}) {
    scoped_refptr<X509Certificate> cert = CreateCertificateChainFromFile(
        GetTestCertsDirectory(), rejected_cert, X509Certificate::FORMAT_AUTO);
    ASSERT_TRUE(cert);

    scoped_refptr<CertVerifyProc> verify_proc;
    int error = 0;

    // Test that a legacy Symantec certificate is rejected.
    CertVerifyResult symantec_result;
    symantec_result.verified_cert = cert;
    symantec_result.public_key_hashes.push_back(HashValue(kSymantecHashValue));
    symantec_result.is_issued_by_known_root = true;
    verify_proc = base::MakeRefCounted<MockCertVerifyProc>(symantec_result);

    CertVerifyResult test_result_1;
    error = verify_proc->Verify(
        cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
        /*sct_list=*/std::string(), 0, CRLSet::BuiltinCRLSet().get(),
        CertificateList(), &test_result_1, NetLogWithSource());
    EXPECT_THAT(error, IsError(ERR_CERT_SYMANTEC_LEGACY));
    EXPECT_TRUE(test_result_1.cert_status & CERT_STATUS_SYMANTEC_LEGACY);

    // ... Unless the Symantec cert chains through a allowlisted intermediate.
    CertVerifyResult allowlisted_result;
    allowlisted_result.verified_cert = cert;
    allowlisted_result.public_key_hashes.push_back(
        HashValue(kSymantecHashValue));
    allowlisted_result.public_key_hashes.push_back(HashValue(kGoogleHashValue));
    allowlisted_result.is_issued_by_known_root = true;
    verify_proc = base::MakeRefCounted<MockCertVerifyProc>(allowlisted_result);

    CertVerifyResult test_result_2;
    error = verify_proc->Verify(
        cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
        /*sct_list=*/std::string(), 0, CRLSet::BuiltinCRLSet().get(),
        CertificateList(), &test_result_2, NetLogWithSource());
    EXPECT_THAT(error, IsOk());
    EXPECT_FALSE(test_result_2.cert_status & CERT_STATUS_AUTHORITY_INVALID);

    // ... Or the caller disabled enforcement of Symantec policies.
    CertVerifyResult test_result_3;
    error = verify_proc->Verify(
        cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
        /*sct_list=*/std::string(),
        CertVerifyProc::VERIFY_DISABLE_SYMANTEC_ENFORCEMENT,
        CRLSet::BuiltinCRLSet().get(), CertificateList(), &test_result_3,
        NetLogWithSource());
    EXPECT_THAT(error, IsOk());
    EXPECT_FALSE(test_result_3.cert_status & CERT_STATUS_SYMANTEC_LEGACY);
  }

  // Test that certificates from the legacy Symantec infrastructure issued
  // after 2016-06-01 approriately accept or reject based on the base::Feature
  // flag.
  for (bool feature_flag_enabled : {false, true}) {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureState(
        CertVerifyProc::kLegacySymantecPKIEnforcement, feature_flag_enabled);

    scoped_refptr<X509Certificate> cert = CreateCertificateChainFromFile(
        GetTestCertsDirectory(), "post_june_2016.pem",
        X509Certificate::FORMAT_AUTO);
    ASSERT_TRUE(cert);

    scoped_refptr<CertVerifyProc> verify_proc;
    int error = 0;

    // Test that a legacy Symantec certificate is rejected if the feature
    // flag is enabled, and accepted if it is not.
    CertVerifyResult symantec_result;
    symantec_result.verified_cert = cert;
    symantec_result.public_key_hashes.push_back(HashValue(kSymantecHashValue));
    symantec_result.is_issued_by_known_root = true;
    verify_proc = base::MakeRefCounted<MockCertVerifyProc>(symantec_result);

    CertVerifyResult test_result_1;
    error = verify_proc->Verify(
        cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
        /*sct_list=*/std::string(), 0, CRLSet::BuiltinCRLSet().get(),
        CertificateList(), &test_result_1, NetLogWithSource());
    if (feature_flag_enabled) {
      EXPECT_THAT(error, IsError(ERR_CERT_SYMANTEC_LEGACY));
      EXPECT_TRUE(test_result_1.cert_status & CERT_STATUS_SYMANTEC_LEGACY);
    } else {
      EXPECT_THAT(error, IsOk());
      EXPECT_FALSE(test_result_1.cert_status & CERT_STATUS_SYMANTEC_LEGACY);
    }

    // ... Unless the Symantec cert chains through a allowlisted intermediate.
    CertVerifyResult allowlisted_result;
    allowlisted_result.verified_cert = cert;
    allowlisted_result.public_key_hashes.push_back(
        HashValue(kSymantecHashValue));
    allowlisted_result.public_key_hashes.push_back(HashValue(kGoogleHashValue));
    allowlisted_result.is_issued_by_known_root = true;
    verify_proc = base::MakeRefCounted<MockCertVerifyProc>(allowlisted_result);

    CertVerifyResult test_result_2;
    error = verify_proc->Verify(
        cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
        /*sct_list=*/std::string(), 0, CRLSet::BuiltinCRLSet().get(),
        CertificateList(), &test_result_2, NetLogWithSource());
    EXPECT_THAT(error, IsOk());
    EXPECT_FALSE(test_result_2.cert_status & CERT_STATUS_AUTHORITY_INVALID);

    // ... Or the caller disabled enforcement of Symantec policies.
    CertVerifyResult test_result_3;
    error = verify_proc->Verify(
        cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
        /*sct_list=*/std::string(),
        CertVerifyProc::VERIFY_DISABLE_SYMANTEC_ENFORCEMENT,
        CRLSet::BuiltinCRLSet().get(), CertificateList(), &test_result_3,
        NetLogWithSource());
    EXPECT_THAT(error, IsOk());
    EXPECT_FALSE(test_result_3.cert_status & CERT_STATUS_SYMANTEC_LEGACY);
  }
}

// Test that the certificate returned in CertVerifyResult is able to reorder
// certificates that are not ordered from end-entity to root. While this is
// a protocol violation if sent during a TLS handshake, if multiple sources
// of intermediate certificates are combined, it's possible that order may
// not be maintained.
TEST_P(CertVerifyProcInternalTest, VerifyReturnChainProperlyOrdered) {
  if (!SupportsReturningVerifiedChain()) {
    LOG(INFO) << "Skipping this test in this platform.";
    return;
  }

  base::FilePath certs_dir = GetTestCertsDirectory();
  CertificateList certs = CreateCertificateListFromFile(
      certs_dir, "x509_verify_results.chain.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3U, certs.size());

  // Construct the chain out of order.
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(bssl::UpRef(certs[2]->cert_buffer()));
  intermediates.push_back(bssl::UpRef(certs[1]->cert_buffer()));

  ScopedTestRoot scoped_root(certs[2].get());

  scoped_refptr<X509Certificate> google_full_chain =
      X509Certificate::CreateFromBuffer(bssl::UpRef(certs[0]->cert_buffer()),
                                        std::move(intermediates));
  ASSERT_TRUE(google_full_chain);
  ASSERT_EQ(2U, google_full_chain->intermediate_buffers().size());

  CertVerifyResult verify_result;
  EXPECT_FALSE(verify_result.verified_cert);
  int error =
      Verify(google_full_chain.get(), "127.0.0.1", 0,
             CRLSet::BuiltinCRLSet().get(), CertificateList(), &verify_result);
  EXPECT_THAT(error, IsOk());
  ASSERT_TRUE(verify_result.verified_cert);

  EXPECT_NE(google_full_chain, verify_result.verified_cert);
  EXPECT_TRUE(
      x509_util::CryptoBufferEqual(google_full_chain->cert_buffer(),
                                   verify_result.verified_cert->cert_buffer()));
  const auto& return_intermediates =
      verify_result.verified_cert->intermediate_buffers();
  ASSERT_EQ(2U, return_intermediates.size());
  EXPECT_TRUE(x509_util::CryptoBufferEqual(return_intermediates[0].get(),
                                           certs[1]->cert_buffer()));
  EXPECT_TRUE(x509_util::CryptoBufferEqual(return_intermediates[1].get(),
                                           certs[2]->cert_buffer()));
}

// Test that Verify() filters out certificates which are not related to
// or part of the certificate chain being verified.
TEST_P(CertVerifyProcInternalTest, VerifyReturnChainFiltersUnrelatedCerts) {
  if (!SupportsReturningVerifiedChain()) {
    LOG(INFO) << "Skipping this test in this platform.";
    return;
  }

  base::FilePath certs_dir = GetTestCertsDirectory();
  CertificateList certs = CreateCertificateListFromFile(
      certs_dir, "x509_verify_results.chain.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3U, certs.size());
  ScopedTestRoot scoped_root(certs[2].get());

  scoped_refptr<X509Certificate> unrelated_certificate =
      ImportCertFromFile(certs_dir, "duplicate_cn_1.pem");
  scoped_refptr<X509Certificate> unrelated_certificate2 =
      ImportCertFromFile(certs_dir, "aia-cert.pem");
  ASSERT_NE(static_cast<X509Certificate*>(nullptr),
            unrelated_certificate.get());
  ASSERT_NE(static_cast<X509Certificate*>(nullptr),
            unrelated_certificate2.get());

  // Interject unrelated certificates into the list of intermediates.
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(bssl::UpRef(unrelated_certificate->cert_buffer()));
  intermediates.push_back(bssl::UpRef(certs[1]->cert_buffer()));
  intermediates.push_back(bssl::UpRef(unrelated_certificate2->cert_buffer()));
  intermediates.push_back(bssl::UpRef(certs[2]->cert_buffer()));

  scoped_refptr<X509Certificate> google_full_chain =
      X509Certificate::CreateFromBuffer(bssl::UpRef(certs[0]->cert_buffer()),
                                        std::move(intermediates));
  ASSERT_TRUE(google_full_chain);
  ASSERT_EQ(4U, google_full_chain->intermediate_buffers().size());

  CertVerifyResult verify_result;
  EXPECT_FALSE(verify_result.verified_cert);
  int error =
      Verify(google_full_chain.get(), "127.0.0.1", 0,
             CRLSet::BuiltinCRLSet().get(), CertificateList(), &verify_result);
  EXPECT_THAT(error, IsOk());
  ASSERT_TRUE(verify_result.verified_cert);

  EXPECT_NE(google_full_chain, verify_result.verified_cert);
  EXPECT_TRUE(
      x509_util::CryptoBufferEqual(google_full_chain->cert_buffer(),
                                   verify_result.verified_cert->cert_buffer()));
  const auto& return_intermediates =
      verify_result.verified_cert->intermediate_buffers();
  ASSERT_EQ(2U, return_intermediates.size());
  EXPECT_TRUE(x509_util::CryptoBufferEqual(return_intermediates[0].get(),
                                           certs[1]->cert_buffer()));
  EXPECT_TRUE(x509_util::CryptoBufferEqual(return_intermediates[1].get(),
                                           certs[2]->cert_buffer()));
}

TEST_P(CertVerifyProcInternalTest, AdditionalTrustAnchors) {
  if (!SupportsAdditionalTrustAnchors()) {
    LOG(INFO) << "Skipping this test in this platform.";
    return;
  }

  // |ca_cert| is the issuer of |cert|.
  CertificateList ca_cert_list =
      CreateCertificateListFromFile(GetTestCertsDirectory(), "root_ca_cert.pem",
                                    X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, ca_cert_list.size());
  scoped_refptr<X509Certificate> ca_cert(ca_cert_list[0]);

  CertificateList cert_list = CreateCertificateListFromFile(
      GetTestCertsDirectory(), "ok_cert.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, cert_list.size());
  scoped_refptr<X509Certificate> cert(cert_list[0]);

  // Verification of |cert| fails when |ca_cert| is not in the trust anchors
  // list.
  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(cert.get(), "127.0.0.1", flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  EXPECT_EQ(CERT_STATUS_AUTHORITY_INVALID, verify_result.cert_status);
  EXPECT_FALSE(verify_result.is_issued_by_additional_trust_anchor);

  // Now add the |ca_cert| to the |trust_anchors|, and verification should pass.
  CertificateList trust_anchors;
  trust_anchors.push_back(ca_cert);
  error = Verify(cert.get(), "127.0.0.1", flags, CRLSet::BuiltinCRLSet().get(),
                 trust_anchors, &verify_result);
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0U, verify_result.cert_status);
  EXPECT_TRUE(verify_result.is_issued_by_additional_trust_anchor);

  // Clearing the |trust_anchors| makes verification fail again (the cache
  // should be skipped).
  error = Verify(cert.get(), "127.0.0.1", flags, CRLSet::BuiltinCRLSet().get(),
                 CertificateList(), &verify_result);
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  EXPECT_EQ(CERT_STATUS_AUTHORITY_INVALID, verify_result.cert_status);
  EXPECT_FALSE(verify_result.is_issued_by_additional_trust_anchor);
}

// Tests that certificates issued by user-supplied roots are not flagged as
// issued by a known root. This should pass whether or not the platform supports
// detecting known roots.
TEST_P(CertVerifyProcInternalTest, IsIssuedByKnownRootIgnoresTestRoots) {
  // Load root_ca_cert.pem into the test root store.
  ScopedTestRoot test_root(
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem").get());

  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));

  // Verification should pass.
  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(cert.get(), "127.0.0.1", flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0U, verify_result.cert_status);
  // But should not be marked as a known root.
  EXPECT_FALSE(verify_result.is_issued_by_known_root);
}

// Test verification with a leaf that does not contain embedded SCTs, and which
// has a notBefore date after 2018/10/15, and passing a valid |sct_list| to
// Verify(). Verification should succeed on all platforms. (Assuming the
// verifier trusts the SCT Logs used in |sct_list|.)
//
// Fails on multiple plaforms, see crbug.com/1050152.
TEST_P(CertVerifyProcInternalTest,
       DISABLED_LeafNewerThan20181015WithTlsSctList) {
  scoped_refptr<X509Certificate> chain = CreateCertificateChainFromFile(
      GetTestCertsDirectory(), "treadclimber.pem",
      X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  ASSERT_TRUE(chain);
  if (base::Time::Now() > chain->valid_expiry()) {
    FAIL() << "This test uses a certificate chain which is now expired. Please "
              "disable and file a bug against mattm.";
    return;
  }

  std::string sct_list;
  ASSERT_TRUE(base::ReadFileToString(
      GetTestCertsDirectory().AppendASCII("treadclimber.sctlist"), &sct_list));

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      verify_proc()->Verify(chain.get(), "treadclimber.com",
                            /*ocsp_response=*/std::string(), sct_list, flags,
                            CRLSet::BuiltinCRLSet().get(), CertificateList(),
                            &verify_result, NetLogWithSource());

  // Since the valid |sct_list| was passed to Verify, verification should
  // succeed on all verifiers and OS versions.
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0U, verify_result.cert_status);
  EXPECT_TRUE(verify_result.is_issued_by_known_root);
}

// Test that CRLSets are effective in making a certificate appear to be
// revoked.
TEST_P(CertVerifyProcInternalTest, CRLSet) {
  if (!SupportsCRLSet()) {
    LOG(INFO) << "Skipping test as verifier doesn't support CRLSet";
    return;
  }

  CertificateList ca_cert_list =
      CreateCertificateListFromFile(GetTestCertsDirectory(), "root_ca_cert.pem",
                                    X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, ca_cert_list.size());
  ScopedTestRoot test_root(ca_cert_list[0].get());

  CertificateList cert_list = CreateCertificateListFromFile(
      GetTestCertsDirectory(), "ok_cert.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, cert_list.size());
  scoped_refptr<X509Certificate> cert(cert_list[0]);

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(cert.get(), "127.0.0.1", flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0U, verify_result.cert_status);

  scoped_refptr<CRLSet> crl_set;
  std::string crl_set_bytes;

  // First test blocking by SPKI.
  EXPECT_TRUE(base::ReadFileToString(
      GetTestCertsDirectory().AppendASCII("crlset_by_leaf_spki.raw"),
      &crl_set_bytes));
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

  error = Verify(cert.get(), "127.0.0.1", flags, crl_set.get(),
                 CertificateList(), &verify_result);
  EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));

  // Second, test revocation by serial number of a cert directly under the
  // root.
  crl_set_bytes.clear();
  EXPECT_TRUE(base::ReadFileToString(
      GetTestCertsDirectory().AppendASCII("crlset_by_root_serial.raw"),
      &crl_set_bytes));
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

  error = Verify(cert.get(), "127.0.0.1", flags, crl_set.get(),
                 CertificateList(), &verify_result);
  EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));
}

TEST_P(CertVerifyProcInternalTest, CRLSetLeafSerial) {
  if (!SupportsCRLSet()) {
    LOG(INFO) << "Skipping test as verifier doesn't support CRLSet";
    return;
  }

  CertificateList ca_cert_list =
      CreateCertificateListFromFile(GetTestCertsDirectory(), "root_ca_cert.pem",
                                    X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, ca_cert_list.size());
  ScopedTestRoot test_root(ca_cert_list[0].get());

  scoped_refptr<X509Certificate> leaf = CreateCertificateChainFromFile(
      GetTestCertsDirectory(), "ok_cert_by_intermediate.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(leaf);
  ASSERT_EQ(1U, leaf->intermediate_buffers().size());

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(leaf.get(), "127.0.0.1", flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);
  EXPECT_THAT(error, IsOk());

  // Test revocation by serial number of a certificate not under the root.
  scoped_refptr<CRLSet> crl_set;
  std::string crl_set_bytes;
  ASSERT_TRUE(base::ReadFileToString(
      GetTestCertsDirectory().AppendASCII("crlset_by_intermediate_serial.raw"),
      &crl_set_bytes));
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

  error = Verify(leaf.get(), "127.0.0.1", flags, crl_set.get(),
                 CertificateList(), &verify_result);
  EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));
}

TEST_P(CertVerifyProcInternalTest, CRLSetRootReturnsChain) {
  if (!SupportsCRLSet()) {
    LOG(INFO) << "Skipping test as verifier doesn't support CRLSet";
    return;
  }

  CertificateList ca_cert_list =
      CreateCertificateListFromFile(GetTestCertsDirectory(), "root_ca_cert.pem",
                                    X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(1U, ca_cert_list.size());
  ScopedTestRoot test_root(ca_cert_list[0].get());

  scoped_refptr<X509Certificate> leaf = CreateCertificateChainFromFile(
      GetTestCertsDirectory(), "ok_cert_by_intermediate.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(leaf);
  ASSERT_EQ(1U, leaf->intermediate_buffers().size());

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(leaf.get(), "127.0.0.1", flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);
  EXPECT_THAT(error, IsOk());

  // Test revocation of the root itself.
  scoped_refptr<CRLSet> crl_set;
  std::string crl_set_bytes;
  ASSERT_TRUE(base::ReadFileToString(
      GetTestCertsDirectory().AppendASCII("crlset_by_root_spki.raw"),
      &crl_set_bytes));
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

  error = Verify(leaf.get(), "127.0.0.1", flags, crl_set.get(),
                 CertificateList(), &verify_result);
  EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));

  EXPECT_EQ(3u, verify_result.public_key_hashes.size());
  ASSERT_TRUE(verify_result.verified_cert);
  EXPECT_EQ(2u, verify_result.verified_cert->intermediate_buffers().size());
}

// Tests that CertVerifyProc implementations apply CRLSet revocations by
// subject.
TEST_P(CertVerifyProcInternalTest, CRLSetRevokedBySubject) {
  if (!SupportsCRLSet()) {
    LOG(INFO) << "Skipping test as verifier doesn't support CRLSet";
    return;
  }

  scoped_refptr<X509Certificate> root(
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem"));
  ASSERT_TRUE(root);

  scoped_refptr<X509Certificate> leaf(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(leaf);

  ScopedTestRoot scoped_root(root.get());

  int flags = 0;
  CertVerifyResult verify_result;

  // Confirm that verifying the certificate chain with an empty CRLSet succeeds.
  scoped_refptr<CRLSet> crl_set = CRLSet::EmptyCRLSetForTesting();
  int error = Verify(leaf.get(), "127.0.0.1", flags, crl_set.get(),
                     CertificateList(), &verify_result);
  EXPECT_THAT(error, IsOk());

  std::string crl_set_bytes;

  // Revoke the leaf by subject. Verification should now fail.
  ASSERT_TRUE(base::ReadFileToString(
      GetTestCertsDirectory().AppendASCII("crlset_by_leaf_subject_no_spki.raw"),
      &crl_set_bytes));
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

  error = Verify(leaf.get(), "127.0.0.1", flags, crl_set.get(),
                 CertificateList(), &verify_result);
  EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));

  // Revoke the root by subject. Verification should now fail.
  ASSERT_TRUE(base::ReadFileToString(
      GetTestCertsDirectory().AppendASCII("crlset_by_root_subject_no_spki.raw"),
      &crl_set_bytes));
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

  error = Verify(leaf.get(), "127.0.0.1", flags, crl_set.get(),
                 CertificateList(), &verify_result);
  EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));

  // Revoke the leaf by subject, but only if the SPKI doesn't match the given
  // one. Verification should pass when using the certificate's actual SPKI.
  ASSERT_TRUE(base::ReadFileToString(
      GetTestCertsDirectory().AppendASCII("crlset_by_root_subject.raw"),
      &crl_set_bytes));
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

  error = Verify(leaf.get(), "127.0.0.1", flags, crl_set.get(),
                 CertificateList(), &verify_result);
  EXPECT_THAT(error, IsOk());
}

// Ensures that CRLSets can be used to block known interception roots on
// platforms that support CRLSets, while otherwise detect known interception
// on platforms that do not.
TEST_P(CertVerifyProcInternalTest, BlockedInterceptionByRoot) {
  scoped_refptr<X509Certificate> root =
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem");
  ASSERT_TRUE(root);
  ScopedTestRoot test_root(root.get());

  scoped_refptr<X509Certificate> cert = CreateCertificateChainFromFile(
      GetTestCertsDirectory(), "ok_cert_by_intermediate.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert);

  // A default/built-in CRLSet should not block
  scoped_refptr<CRLSet> crl_set = CRLSet::BuiltinCRLSet();
  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(cert.get(), "127.0.0.1", flags, crl_set.get(),
                     CertificateList(), &verify_result);
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0U, verify_result.cert_status);

  // Read in a CRLSet that marks the root as blocked for interception.
  std::string crl_set_bytes;
  ASSERT_TRUE(
      base::ReadFileToString(GetTestCertsDirectory().AppendASCII(
                                 "crlset_blocked_interception_by_root.raw"),
                             &crl_set_bytes));
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

  error = Verify(cert.get(), "127.0.0.1", flags, crl_set.get(),
                 CertificateList(), &verify_result);
  if (SupportsCRLSet()) {
    EXPECT_THAT(error, IsError(ERR_CERT_KNOWN_INTERCEPTION_BLOCKED));
    EXPECT_TRUE(verify_result.cert_status &
                CERT_STATUS_KNOWN_INTERCEPTION_BLOCKED);
  } else {
    EXPECT_THAT(error, IsOk());
    EXPECT_TRUE(verify_result.cert_status &
                CERT_STATUS_KNOWN_INTERCEPTION_DETECTED);
  }
}

// Ensures that CRLSets can be used to block known interception intermediates,
// while still allowing other certificates from that root..
TEST_P(CertVerifyProcInternalTest, BlockedInterceptionByIntermediate) {
  scoped_refptr<X509Certificate> root =
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem");
  ASSERT_TRUE(root);
  ScopedTestRoot test_root(root.get());

  scoped_refptr<X509Certificate> cert = CreateCertificateChainFromFile(
      GetTestCertsDirectory(), "ok_cert_by_intermediate.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert);

  // A default/built-in CRLSEt should not block
  scoped_refptr<CRLSet> crl_set = CRLSet::BuiltinCRLSet();
  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(cert.get(), "127.0.0.1", flags, crl_set.get(),
                     CertificateList(), &verify_result);
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0U, verify_result.cert_status);

  // Read in a CRLSet that marks the intermediate as blocked for interception.
  std::string crl_set_bytes;
  ASSERT_TRUE(base::ReadFileToString(
      GetTestCertsDirectory().AppendASCII(
          "crlset_blocked_interception_by_intermediate.raw"),
      &crl_set_bytes));
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

  error = Verify(cert.get(), "127.0.0.1", flags, crl_set.get(),
                 CertificateList(), &verify_result);
  if (SupportsCRLSet()) {
    EXPECT_THAT(error, IsError(ERR_CERT_KNOWN_INTERCEPTION_BLOCKED));
    EXPECT_TRUE(verify_result.cert_status &
                CERT_STATUS_KNOWN_INTERCEPTION_BLOCKED);
  } else {
    EXPECT_THAT(error, IsOk());
    EXPECT_TRUE(verify_result.cert_status &
                CERT_STATUS_KNOWN_INTERCEPTION_DETECTED);
  }

  // Load a different certificate from that root, which should be unaffected.
  scoped_refptr<X509Certificate> second_cert = CreateCertificateChainFromFile(
      GetTestCertsDirectory(), "ok_cert.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(second_cert);

  error = Verify(second_cert.get(), "127.0.0.1", flags, crl_set.get(),
                 CertificateList(), &verify_result);
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0U, verify_result.cert_status);
}

// Ensures that CRLSets can be used to flag known interception roots, even
// when they are not blocked.
TEST_P(CertVerifyProcInternalTest, DetectsInterceptionByRoot) {
  scoped_refptr<X509Certificate> root =
      ImportCertFromFile(GetTestCertsDirectory(), "root_ca_cert.pem");
  ASSERT_TRUE(root);
  ScopedTestRoot test_root(root.get());

  scoped_refptr<X509Certificate> cert = CreateCertificateChainFromFile(
      GetTestCertsDirectory(), "ok_cert_by_intermediate.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert);

  // A default/built-in CRLSet should not block
  scoped_refptr<CRLSet> crl_set = CRLSet::BuiltinCRLSet();
  int flags = 0;
  CertVerifyResult verify_result;
  int error = Verify(cert.get(), "127.0.0.1", flags, crl_set.get(),
                     CertificateList(), &verify_result);
  EXPECT_THAT(error, IsOk());
  EXPECT_EQ(0U, verify_result.cert_status);

  // Read in a CRLSet that marks the root as blocked for interception.
  std::string crl_set_bytes;
  ASSERT_TRUE(
      base::ReadFileToString(GetTestCertsDirectory().AppendASCII(
                                 "crlset_known_interception_by_root.raw"),
                             &crl_set_bytes));
  ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

  error = Verify(cert.get(), "127.0.0.1", flags, crl_set.get(),
                 CertificateList(), &verify_result);
  EXPECT_THAT(error, IsOk());
  EXPECT_TRUE(verify_result.cert_status &
              CERT_STATUS_KNOWN_INTERCEPTION_DETECTED);
}

// Tests that CRLSets participate in path building functions, and that as
// long as a valid path exists within the verification graph, verification
// succeeds.
//
// In this test, there are two roots (D and E), and three possible paths
// to validate a leaf (A):
// 1. A(B) -> B(C) -> C(D) -> D(D)
// 2. A(B) -> B(C) -> C(E) -> E(E)
// 3. A(B) -> B(F) -> F(E) -> E(E)
//
// Each permutation of revocation is tried:
// 1. Revoking E by SPKI, so that only Path 1 is valid (as E is in Paths 2 & 3)
// 2. Revoking C(D) and F(E) by serial, so that only Path 2 is valid.
// 3. Revoking C by SPKI, so that only Path 3 is valid (as C is in Paths 1 & 2)
TEST_P(CertVerifyProcInternalTest, CRLSetDuringPathBuilding) {
  if (!SupportsCRLSetsInPathBuilding()) {
    LOG(INFO) << "Skipping this test on this platform.";
    return;
  }

  CertificateList path_1_certs;
  ASSERT_TRUE(
      LoadCertificateFiles({"multi-root-A-by-B.pem", "multi-root-B-by-C.pem",
                            "multi-root-C-by-D.pem", "multi-root-D-by-D.pem"},
                           &path_1_certs));

  CertificateList path_2_certs;
  ASSERT_TRUE(
      LoadCertificateFiles({"multi-root-A-by-B.pem", "multi-root-B-by-C.pem",
                            "multi-root-C-by-E.pem", "multi-root-E-by-E.pem"},
                           &path_2_certs));

  CertificateList path_3_certs;
  ASSERT_TRUE(
      LoadCertificateFiles({"multi-root-A-by-B.pem", "multi-root-B-by-F.pem",
                            "multi-root-F-by-E.pem", "multi-root-E-by-E.pem"},
                           &path_3_certs));

  // Add D and E as trust anchors.
  ScopedTestRoot test_root_D(path_1_certs[3].get());  // D-by-D
  ScopedTestRoot test_root_E(path_2_certs[3].get());  // E-by-E

  // Create a chain that contains all the certificate paths possible.
  // CertVerifyProcInternalTest.VerifyReturnChainFiltersUnrelatedCerts already
  // ensures that it's safe to send additional certificates as inputs, and
  // that they're ignored if not necessary.
  // This is to avoid relying on AIA or internal object caches when
  // interacting with the underlying library.
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(
      bssl::UpRef(path_1_certs[1]->cert_buffer()));  // B-by-C
  intermediates.push_back(
      bssl::UpRef(path_1_certs[2]->cert_buffer()));  // C-by-D
  intermediates.push_back(
      bssl::UpRef(path_2_certs[2]->cert_buffer()));  // C-by-E
  intermediates.push_back(
      bssl::UpRef(path_3_certs[1]->cert_buffer()));  // B-by-F
  intermediates.push_back(
      bssl::UpRef(path_3_certs[2]->cert_buffer()));  // F-by-E
  scoped_refptr<X509Certificate> cert = X509Certificate::CreateFromBuffer(
      bssl::UpRef(path_1_certs[0]->cert_buffer()), std::move(intermediates));
  ASSERT_TRUE(cert);

  struct TestPermutations {
    const char* crlset;
    bool expect_valid;
    scoped_refptr<X509Certificate> expected_intermediate;
  } kTests[] = {
      {"multi-root-crlset-D-and-E.raw", false, nullptr},
      {"multi-root-crlset-E.raw", true, path_1_certs[2].get()},
      {"multi-root-crlset-CD-and-FE.raw", true, path_2_certs[2].get()},
      {"multi-root-crlset-C.raw", true, path_3_certs[2].get()},
      {"multi-root-crlset-unrelated.raw", true, nullptr}};

  for (const auto& testcase : kTests) {
    SCOPED_TRACE(testcase.crlset);
    scoped_refptr<CRLSet> crl_set;
    std::string crl_set_bytes;
    EXPECT_TRUE(base::ReadFileToString(
        GetTestCertsDirectory().AppendASCII(testcase.crlset), &crl_set_bytes));
    ASSERT_TRUE(CRLSet::Parse(crl_set_bytes, &crl_set));

    int flags = 0;
    CertVerifyResult verify_result;
    int error = Verify(cert.get(), "127.0.0.1", flags, crl_set.get(),
                       CertificateList(), &verify_result);

    if (!testcase.expect_valid) {
      EXPECT_NE(OK, error);
      EXPECT_NE(0U, verify_result.cert_status);
      continue;
    }

    ASSERT_THAT(error, IsOk());
    ASSERT_EQ(0U, verify_result.cert_status);
    ASSERT_TRUE(verify_result.verified_cert.get());

    if (!testcase.expected_intermediate)
      continue;

    const auto& verified_intermediates =
        verify_result.verified_cert->intermediate_buffers();
    ASSERT_EQ(3U, verified_intermediates.size());

    scoped_refptr<X509Certificate> intermediate =
        X509Certificate::CreateFromBuffer(
            bssl::UpRef(verified_intermediates[1].get()), {});
    ASSERT_TRUE(intermediate);

    EXPECT_TRUE(testcase.expected_intermediate->EqualsExcludingChain(
        intermediate.get()))
        << "Expected: " << testcase.expected_intermediate->subject().common_name
        << " issued by " << testcase.expected_intermediate->issuer().common_name
        << "; Got: " << intermediate->subject().common_name << " issued by "
        << intermediate->issuer().common_name;
  }
}

TEST_P(CertVerifyProcInternalTest, ValidityDayPlus5MinutesBeforeNotBefore) {
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CertBuilder::CreateSimpleChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);
  base::Time not_before = base::Time::Now() + base::TimeDelta::FromDays(1) +
                          base::TimeDelta::FromMinutes(5);
  base::Time not_after = base::Time::Now() + base::TimeDelta::FromDays(30);
  leaf->SetValidity(not_before, not_after);

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate().get());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), "www.example.com", flags,
             CRLSet::BuiltinCRLSet().get(), CertificateList(), &verify_result);
  // Current time is before certificate's notBefore. Verification should fail.
  EXPECT_THAT(error, IsError(ERR_CERT_DATE_INVALID));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_DATE_INVALID);
}

TEST_P(CertVerifyProcInternalTest, ValidityDayBeforeNotBefore) {
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CertBuilder::CreateSimpleChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);
  base::Time not_before = base::Time::Now() + base::TimeDelta::FromDays(1);
  base::Time not_after = base::Time::Now() + base::TimeDelta::FromDays(30);
  leaf->SetValidity(not_before, not_after);

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate().get());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), "www.example.com", flags,
             CRLSet::BuiltinCRLSet().get(), CertificateList(), &verify_result);
  // Current time is before certificate's notBefore. Verification should fail.
  EXPECT_THAT(error, IsError(ERR_CERT_DATE_INVALID));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_DATE_INVALID);
}

TEST_P(CertVerifyProcInternalTest, ValidityJustBeforeNotBefore) {
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CertBuilder::CreateSimpleChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);
  base::Time not_before = base::Time::Now() + base::TimeDelta::FromMinutes(5);
  base::Time not_after = base::Time::Now() + base::TimeDelta::FromDays(30);
  leaf->SetValidity(not_before, not_after);

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate().get());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), "www.example.com", flags,
             CRLSet::BuiltinCRLSet().get(), CertificateList(), &verify_result);
  // Current time is before certificate's notBefore. Verification should fail.
  EXPECT_THAT(error, IsError(ERR_CERT_DATE_INVALID));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_DATE_INVALID);
}

TEST_P(CertVerifyProcInternalTest, ValidityJustAfterNotBefore) {
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CertBuilder::CreateSimpleChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);
  base::Time not_before = base::Time::Now() - base::TimeDelta::FromSeconds(1);
  base::Time not_after = base::Time::Now() + base::TimeDelta::FromDays(30);
  leaf->SetValidity(not_before, not_after);

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate().get());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), "www.example.com", flags,
             CRLSet::BuiltinCRLSet().get(), CertificateList(), &verify_result);
  // Current time is between notBefore and notAfter. Verification should
  // succeed.
  EXPECT_THAT(error, IsOk());
}

TEST_P(CertVerifyProcInternalTest, ValidityJustBeforeNotAfter) {
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CertBuilder::CreateSimpleChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);
  base::Time not_before = base::Time::Now() - base::TimeDelta::FromDays(30);
  base::Time not_after = base::Time::Now() + base::TimeDelta::FromMinutes(5);
  leaf->SetValidity(not_before, not_after);

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate().get());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), "www.example.com", flags,
             CRLSet::BuiltinCRLSet().get(), CertificateList(), &verify_result);
  // Current time is between notBefore and notAfter. Verification should
  // succeed.
  EXPECT_THAT(error, IsOk());
}

TEST_P(CertVerifyProcInternalTest, ValidityJustAfterNotAfter) {
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CertBuilder::CreateSimpleChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);
  base::Time not_before = base::Time::Now() - base::TimeDelta::FromDays(30);
  base::Time not_after = base::Time::Now() - base::TimeDelta::FromSeconds(1);
  leaf->SetValidity(not_before, not_after);

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate().get());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), "www.example.com", flags,
             CRLSet::BuiltinCRLSet().get(), CertificateList(), &verify_result);
  // Current time is after certificate's notAfter. Verification should fail.
  EXPECT_THAT(error, IsError(ERR_CERT_DATE_INVALID));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_DATE_INVALID);
}

TEST_P(CertVerifyProcInternalTest, FailedIntermediateSignatureValidation) {
  base::FilePath certs_dir =
      GetTestNetDataDirectory()
          .AppendASCII("verify_certificate_chain_unittest")
          .AppendASCII(
              "intermediate-wrong-signature-no-authority-key-identifier");

  CertificateList certs = CreateCertificateListFromFile(
      certs_dir, "chain.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3U, certs.size());

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(bssl::UpRef(certs[1]->cert_buffer()));

  scoped_refptr<X509Certificate> cert = X509Certificate::CreateFromBuffer(
      bssl::UpRef(certs[0]->cert_buffer()), std::move(intermediates));
  ASSERT_TRUE(cert.get());

  // Trust the root certificate.
  ScopedTestRoot scoped_root(certs.back().get());

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(cert.get(), "test.example", flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);

  // The intermediate was signed by a different root with a different key but
  // with the same name as the trusted one, and the intermediate has no
  // authorityKeyIdentifier, so the verifier must try verifying the signature.
  // Should fail with AUTHORITY_INVALID.
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_AUTHORITY_INVALID);
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
}

TEST_P(CertVerifyProcInternalTest, FailedTargetSignatureValidation) {
  base::FilePath certs_dir =
      GetTestNetDataDirectory()
          .AppendASCII("verify_certificate_chain_unittest")
          .AppendASCII("target-wrong-signature-no-authority-key-identifier");

  CertificateList certs = CreateCertificateListFromFile(
      certs_dir, "chain.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3U, certs.size());

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(bssl::UpRef(certs[1]->cert_buffer()));

  scoped_refptr<X509Certificate> cert = X509Certificate::CreateFromBuffer(
      bssl::UpRef(certs[0]->cert_buffer()), std::move(intermediates));
  ASSERT_TRUE(cert.get());

  // Trust the root certificate.
  ScopedTestRoot scoped_root(certs.back().get());

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(cert.get(), "test.example", flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);

  // The leaf was signed by a different intermediate with a different key but
  // with the same name as the one in the chain, and the leaf has no
  // authorityKeyIdentifier, so the verifier must try verifying the signature.
  // Should fail with AUTHORITY_INVALID.
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_AUTHORITY_INVALID);
  EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
}

class CertVerifyProcNameNormalizationTest : public CertVerifyProcInternalTest {
 protected:
  void SetUp() override {
    CertVerifyProcInternalTest::SetUp();

    scoped_refptr<X509Certificate> root_cert =
        ImportCertFromFile(GetTestCertsDirectory(), "ocsp-test-root.pem");
    ASSERT_TRUE(root_cert);
    test_root_.reset(new ScopedTestRoot(root_cert.get()));
  }

  std::string HistogramName() const {
    std::string prefix("Net.CertVerifier.NameNormalizationPrivateRoots.");
    switch (verify_proc_type()) {
      case CERT_VERIFY_PROC_ANDROID:
        return prefix + "Android";
      case CERT_VERIFY_PROC_IOS:
        return prefix + "IOS";
      case CERT_VERIFY_PROC_MAC:
        return prefix + "Mac";
      case CERT_VERIFY_PROC_WIN:
        return prefix + "Win";
      case CERT_VERIFY_PROC_BUILTIN:
        return prefix + "Builtin";
    }
  }

  void ExpectNormalizationHistogram(int verify_error) {
    if (verify_error == OK) {
      histograms_.ExpectUniqueSample(
          HistogramName(), CertVerifyProc::NameNormalizationResult::kNormalized,
          1);
    } else {
      histograms_.ExpectTotalCount(HistogramName(), 0);
    }
  }

  void ExpectByteEqualHistogram() {
    histograms_.ExpectUniqueSample(
        HistogramName(), CertVerifyProc::NameNormalizationResult::kByteEqual,
        1);
  }

 private:
  std::unique_ptr<ScopedTestRoot> test_root_;
  base::HistogramTester histograms_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         CertVerifyProcNameNormalizationTest,
                         testing::ValuesIn(kAllCertVerifiers),
                         VerifyProcTypeToName);

// Tries to verify a chain where the leaf's issuer CN is PrintableString, while
// the intermediate's subject CN is UTF8String, and verifies the proper
// histogram is logged.
TEST_P(CertVerifyProcNameNormalizationTest, StringType) {
  scoped_refptr<X509Certificate> chain = CreateCertificateChainFromFile(
      GetTestCertsDirectory(), "name-normalization-printable-utf8.pem",
      X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  ASSERT_TRUE(chain);

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), "example.test", flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);

  switch (verify_proc_type()) {
    case CERT_VERIFY_PROC_IOS:
    case CERT_VERIFY_PROC_MAC:
    case CERT_VERIFY_PROC_WIN:
      EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
      break;
    case CERT_VERIFY_PROC_ANDROID:
    case CERT_VERIFY_PROC_BUILTIN:
      EXPECT_THAT(error, IsOk());
      break;
  }

  ExpectNormalizationHistogram(error);
}

// Tries to verify a chain where the leaf's issuer CN and intermediate's
// subject CN are both PrintableString but have differing case on the first
// character, and verifies the proper histogram is logged.
TEST_P(CertVerifyProcNameNormalizationTest, CaseFolding) {
  scoped_refptr<X509Certificate> chain = CreateCertificateChainFromFile(
      GetTestCertsDirectory(), "name-normalization-case-folding.pem",
      X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  ASSERT_TRUE(chain);

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), "example.test", flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);

  switch (verify_proc_type()) {
    case CERT_VERIFY_PROC_WIN:
      EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
      break;
    case CERT_VERIFY_PROC_ANDROID:
    case CERT_VERIFY_PROC_IOS:
    case CERT_VERIFY_PROC_MAC:
    case CERT_VERIFY_PROC_BUILTIN:
      EXPECT_THAT(error, IsOk());
      break;
  }

  ExpectNormalizationHistogram(error);
}

// Confirms that a chain generated by the generate-name-normalization-certs.py
// script which does not require normalization validates ok, and that the
// ByteEqual histogram is logged.
TEST_P(CertVerifyProcNameNormalizationTest, ByteEqual) {
  scoped_refptr<X509Certificate> chain = CreateCertificateChainFromFile(
      GetTestCertsDirectory(), "name-normalization-byteequal.pem",
      X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  ASSERT_TRUE(chain);

  int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), "example.test", flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);

  EXPECT_THAT(error, IsOk());
  ExpectByteEqualHistogram();
}

// This is the same as CertVerifyProcInternalTest, but it additionally sets up
// networking capabilities for the cert verifiers, and a test server that can be
// used to serve mock responses for AIA/OCSP/CRL.
//
// An actual HTTP test server is used rather than simply mocking the network
// layer, since the certificate fetching networking layer is not mockable for
// all of the cert verifier implementations.
//
// The approach taken in this test fixture is to generate certificates
// on the fly so they use randomly chosen URLs, subjects, and serial
// numbers, in order to defeat global caching effects from the platform
// verifiers. Moreover, the AIA needs to be chosen dynamically since the
// test server's port number cannot be known statically.
class CertVerifyProcInternalWithNetFetchingTest
    : public CertVerifyProcInternalTest {
 protected:
  CertVerifyProcInternalWithNetFetchingTest()
      : task_environment_(
            base::test::TaskEnvironment::MainThreadType::DEFAULT) {}

  void SetUp() override {
    // Create a network thread to be used for network fetches, and wait for
    // initialization to complete on that thread.
    base::Thread::Options options(base::MessagePumpType::IO, 0);
    network_thread_ = std::make_unique<base::Thread>("network_thread");
    CHECK(network_thread_->StartWithOptions(options));

    base::WaitableEvent initialization_complete_event(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    network_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&SetUpOnNetworkThread, &context_, &cert_net_fetcher_,
                       &initialization_complete_event));
    initialization_complete_event.Wait();
    EXPECT_TRUE(cert_net_fetcher_);

    CertVerifyProcInternalTest::SetUpWithCertNetFetcher(cert_net_fetcher_);

    EXPECT_FALSE(test_server_.Started());

    // Register a single request handler with the EmbeddedTestServer, that in
    // turn dispatches to the internally managed registry of request handlers.
    //
    // This allows registering subsequent handlers dynamically during the course
    // of the test, since EmbeddedTestServer requires its handlers be registered
    // prior to Start().
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &CertVerifyProcInternalWithNetFetchingTest::DispatchToRequestHandler,
        base::Unretained(this)));
    EXPECT_TRUE(test_server_.Start());
  }

  void TearDown() override {
    // Do cleanup on network thread.
    network_thread_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&ShutdownOnNetworkThread, &context_,
                                  &cert_net_fetcher_));
    network_thread_->Stop();
    network_thread_.reset();

    CertVerifyProcInternalTest::TearDown();
  }

  // Registers a handler with the test server that responds with the given
  // Content-Type, HTTP status code, and response body, for GET requests
  // to |path|.
  // Returns the full URL to |path| for the current test server.
  GURL RegisterSimpleTestServerHandler(std::string path,
                                       HttpStatusCode status_code,
                                       std::string content_type,
                                       std::string content) {
    GURL handler_url(GetTestServerAbsoluteUrl(path));
    base::AutoLock lock(request_handlers_lock_);
    request_handlers_.push_back(base::BindRepeating(
        &SimpleTestServerHandler, std::move(path), status_code,
        std::move(content_type), std::move(content)));
    return handler_url;
  }

  // Returns a random URL path (starting with /) that has the given suffix.
  static std::string MakeRandomPath(base::StringPiece suffix) {
    return "/" + MakeRandomHexString(12) + suffix.as_string();
  }

  // Returns a URL to |path| for the current test server.
  GURL GetTestServerAbsoluteUrl(const std::string& path) {
    return test_server_.GetURL(path);
  }

  // Creates a certificate chain for www.example.com, where the leaf certificate
  // has an AIA URL pointing to the test server.
  void CreateSimpleChainWithAIA(
      scoped_refptr<X509Certificate>* out_leaf,
      std::string* ca_issuers_path,
      bssl::UniquePtr<CRYPTO_BUFFER>* out_intermediate,
      scoped_refptr<X509Certificate>* out_root) {
    std::unique_ptr<CertBuilder> leaf, intermediate, root;
    CertBuilder::CreateSimpleChain(&leaf, &intermediate, &root);
    ASSERT_TRUE(leaf && intermediate && root);

    // Make the leaf certificate have an AIA (CA Issuers) that points to the
    // embedded test server. This uses a random URL for predictable behavior in
    // the presence of global caching.
    *ca_issuers_path = MakeRandomPath(".cer");
    GURL ca_issuers_url = GetTestServerAbsoluteUrl(*ca_issuers_path);
    leaf->SetCaIssuersUrl(ca_issuers_url);

    // The chain being verified is solely the leaf certificate (missing the
    // intermediate and root).
    *out_leaf = leaf->GetX509Certificate();
    *out_root = root->GetX509Certificate();
    *out_intermediate = intermediate->DupCertBuffer();
  }

  // Creates a CRL issued and signed by |crl_issuer|, marking |revoked_serials|
  // as revoked, and registers it to be served by the test server.
  // Returns the full URL to retrieve the CRL from the test server.
  GURL CreateAndServeCrl(CertBuilder* crl_issuer,
                         const std::vector<uint64_t>& revoked_serials,
                         DigestAlgorithm digest = DigestAlgorithm::Sha256) {
    std::string crl = BuildCrl(crl_issuer->GetSubject(), crl_issuer->GetKey(),
                               revoked_serials, digest);
    std::string crl_path = MakeRandomPath(".crl");
    return RegisterSimpleTestServerHandler(crl_path, HTTP_OK,
                                           "application/pkix-crl", crl);
  }

 private:
  std::unique_ptr<test_server::HttpResponse> DispatchToRequestHandler(
      const test_server::HttpRequest& request) {
    // Called on the embedded test server's IO thread.
    base::AutoLock lock(request_handlers_lock_);
    for (const auto& handler : request_handlers_) {
      auto response = handler.Run(request);
      if (response)
        return response;
    }

    return nullptr;
  }

  // Serves (|status_code|, |content_type|, |content|) in response to GET
  // requests for |path|.
  static std::unique_ptr<test_server::HttpResponse> SimpleTestServerHandler(
      const std::string& path,
      HttpStatusCode status_code,
      const std::string& content_type,
      const std::string& content,
      const test_server::HttpRequest& request) {
    if (request.relative_url != path)
      return nullptr;

    auto http_response = std::make_unique<test_server::BasicHttpResponse>();

    http_response->set_code(status_code);
    http_response->set_content_type(content_type);
    http_response->set_content(content);
    return http_response;
  }

  static void SetUpOnNetworkThread(
      std::unique_ptr<URLRequestContext>* context,
      scoped_refptr<CertNetFetcherURLRequest>* cert_net_fetcher,
      base::WaitableEvent* initialization_complete_event) {
    URLRequestContextBuilder url_request_context_builder;
    url_request_context_builder.set_user_agent("cert_verify_proc_unittest/0.1");
    url_request_context_builder.set_proxy_config_service(
        std::make_unique<ProxyConfigServiceFixed>(ProxyConfigWithAnnotation()));
    *context = url_request_context_builder.Build();

    *cert_net_fetcher = base::MakeRefCounted<net::CertNetFetcherURLRequest>();
    (*cert_net_fetcher)->SetURLRequestContext(context->get());
    initialization_complete_event->Signal();
  }

  static void ShutdownOnNetworkThread(
      std::unique_ptr<URLRequestContext>* context,
      scoped_refptr<net::CertNetFetcherURLRequest>* cert_net_fetcher) {
    (*cert_net_fetcher)->Shutdown();
    cert_net_fetcher->reset();
    context->reset();
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<base::Thread> network_thread_;

  // Owned by this thread, but initialized, used, and shutdown on the network
  // thread.
  std::unique_ptr<URLRequestContext> context_;
  scoped_refptr<CertNetFetcherURLRequest> cert_net_fetcher_;

  EmbeddedTestServer test_server_;

  // The list of registered handlers. Can only be accessed when the lock is
  // held, as this data is shared between the embedded server's IO thread, and
  // the test main thread.
  base::Lock request_handlers_lock_;
  std::vector<test_server::EmbeddedTestServer::HandleRequestCallback>
      request_handlers_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         CertVerifyProcInternalWithNetFetchingTest,
                         testing::ValuesIn(kAllCertVerifiers),
                         VerifyProcTypeToName);

// Tries verifying a certificate chain that is missing an intermediate. The
// intermediate is available via AIA, however the server responds with a 404.
//
// NOTE: This test is separate from IntermediateFromAia200 as a different URL
// needs to be used to avoid having the result depend on globally cached success
// or failure of the fetch.
// Test is flaky on iOS crbug.com/860189
#if defined(OS_IOS)
#define MAYBE_IntermediateFromAia404 DISABLED_IntermediateFromAia404
#else
#define MAYBE_IntermediateFromAia404 IntermediateFromAia404
#endif
TEST_P(CertVerifyProcInternalWithNetFetchingTest, MAYBE_IntermediateFromAia404) {
  const char kHostname[] = "www.example.com";

  // Create a chain where the leaf has an AIA that points to test server.
  scoped_refptr<X509Certificate> leaf;
  std::string ca_issuers_path;
  bssl::UniquePtr<CRYPTO_BUFFER> intermediate;
  scoped_refptr<X509Certificate> root;
  CreateSimpleChainWithAIA(&leaf, &ca_issuers_path, &intermediate, &root);

  // Serve a 404 for the AIA url.
  RegisterSimpleTestServerHandler(ca_issuers_path, HTTP_NOT_FOUND, "text/plain",
                                  "Not Found");

  // Trust the root certificate.
  ScopedTestRoot scoped_root(root.get());

  // The chain being verified is solely the leaf certificate (missing the
  // intermediate and root).
  ASSERT_EQ(0u, leaf->intermediate_buffers().size());

  const int flags = 0;
  int error;
  CertVerifyResult verify_result;

  // Verifying the chain should fail as the intermediate is missing, and
  // cannot be fetched via AIA.
  error = Verify(leaf.get(), kHostname, flags, CRLSet::BuiltinCRLSet().get(),
                 CertificateList(), &verify_result);
  EXPECT_NE(OK, error);

  if (verify_proc_type() == CERT_VERIFY_PROC_WIN) {
    // CertVerifyProcWin has a flaky result of ERR_CERT_AUTHORITY_INVALID or
    // ERR_CERT_INVALID (https://crbug.com/859387) - accept either.
    EXPECT_TRUE(error == ERR_CERT_AUTHORITY_INVALID ||
                error == ERR_CERT_INVALID)
        << "Unexpected error: " << error;
  } else {
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  }
}
#undef MAYBE_IntermediateFromAia404

// Tries verifying a certificate chain that is missing an intermediate. The
// intermediate is available via AIA.
// TODO(crbug.com/860189): Failing on iOS
#if defined(OS_IOS)
#define MAYBE_IntermediateFromAia200Der DISABLED_IntermediateFromAia200Der
#else
#define MAYBE_IntermediateFromAia200Der IntermediateFromAia200Der
#endif
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       MAYBE_IntermediateFromAia200Der) {
  const char kHostname[] = "www.example.com";

  // Create a chain where the leaf has an AIA that points to test server.
  scoped_refptr<X509Certificate> leaf;
  std::string ca_issuers_path;
  bssl::UniquePtr<CRYPTO_BUFFER> intermediate;
  scoped_refptr<X509Certificate> root;
  CreateSimpleChainWithAIA(&leaf, &ca_issuers_path, &intermediate, &root);

  // Setup the test server to reply with the correct intermediate.
  RegisterSimpleTestServerHandler(
      ca_issuers_path, HTTP_OK, "application/pkix-cert",
      x509_util::CryptoBufferAsStringPiece(intermediate.get()).as_string());

  // Trust the root certificate.
  ScopedTestRoot scoped_root(root.get());

  // The chain being verified is solely the leaf certificate (missing the
  // intermediate and root).
  ASSERT_EQ(0u, leaf->intermediate_buffers().size());

  const int flags = 0;
  int error;
  CertVerifyResult verify_result;

  // Verifying the chain should succeed as the missing intermediate can be
  // fetched via AIA.
  error = Verify(leaf.get(), kHostname, flags, CRLSet::BuiltinCRLSet().get(),
                 CertificateList(), &verify_result);
  EXPECT_THAT(error, IsOk());
}

// This test is the same as IntermediateFromAia200Der, except the certificate is
// served as PEM rather than DER.
//
// Tries verifying a certificate chain that is missing an intermediate. The
// intermediate is available via AIA, however is served as a PEM file rather
// than DER.
// TODO(crbug.com/860189): Failing on iOS
#if defined(OS_IOS)
#define MAYBE_IntermediateFromAia200Pem DISABLED_IntermediateFromAia200Pem
#else
#define MAYBE_IntermediateFromAia200Pem IntermediateFromAia200Pem
#endif
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       MAYBE_IntermediateFromAia200Pem) {
  const char kHostname[] = "www.example.com";

  // Create a chain where the leaf has an AIA that points to test server.
  scoped_refptr<X509Certificate> leaf;
  std::string ca_issuers_path;
  bssl::UniquePtr<CRYPTO_BUFFER> intermediate;
  scoped_refptr<X509Certificate> root;
  CreateSimpleChainWithAIA(&leaf, &ca_issuers_path, &intermediate, &root);

  std::string intermediate_pem;
  ASSERT_TRUE(
      X509Certificate::GetPEMEncoded(intermediate.get(), &intermediate_pem));

  // Setup the test server to reply with the correct intermediate.
  RegisterSimpleTestServerHandler(
      ca_issuers_path, HTTP_OK, "application/x-x509-ca-cert", intermediate_pem);

  // Trust the root certificate.
  ScopedTestRoot scoped_root(root.get());

  // The chain being verified is solely the leaf certificate (missing the
  // intermediate and root).
  ASSERT_EQ(0u, leaf->intermediate_buffers().size());

  const int flags = 0;
  int error;
  CertVerifyResult verify_result;

  // Verifying the chain should succeed as the missing intermediate can be
  // fetched via AIA.
  error = Verify(leaf.get(), kHostname, flags, CRLSet::BuiltinCRLSet().get(),
                 CertificateList(), &verify_result);

  if (verify_proc_type() == CERT_VERIFY_PROC_ANDROID) {
    // Android doesn't support PEM - https://crbug.com/725180
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  } else {
    EXPECT_THAT(error, IsOk());
  }
}

// This test is the same as IntermediateFromAia200Pem, but with a different
// formatting on the PEM data.
//
// TODO(crbug.com/860189): Failing on iOS
#if defined(OS_IOS)
#define MAYBE_IntermediateFromAia200Pem2 DISABLED_IntermediateFromAia200Pem2
#else
#define MAYBE_IntermediateFromAia200Pem2 IntermediateFromAia200Pem2
#endif
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       MAYBE_IntermediateFromAia200Pem2) {
  const char kHostname[] = "www.example.com";

  // Create a chain where the leaf has an AIA that points to test server.
  scoped_refptr<X509Certificate> leaf;
  std::string ca_issuers_path;
  bssl::UniquePtr<CRYPTO_BUFFER> intermediate;
  scoped_refptr<X509Certificate> root;
  CreateSimpleChainWithAIA(&leaf, &ca_issuers_path, &intermediate, &root);

  std::string intermediate_pem;
  ASSERT_TRUE(
      X509Certificate::GetPEMEncoded(intermediate.get(), &intermediate_pem));
  intermediate_pem = "Text at start of file\n" + intermediate_pem;

  // Setup the test server to reply with the correct intermediate.
  RegisterSimpleTestServerHandler(
      ca_issuers_path, HTTP_OK, "application/x-x509-ca-cert", intermediate_pem);

  // Trust the root certificate.
  ScopedTestRoot scoped_root(root.get());

  // The chain being verified is solely the leaf certificate (missing the
  // intermediate and root).
  ASSERT_EQ(0u, leaf->intermediate_buffers().size());

  const int flags = 0;
  int error;
  CertVerifyResult verify_result;

  // Verifying the chain should succeed as the missing intermediate can be
  // fetched via AIA.
  error = Verify(leaf.get(), kHostname, flags, CRLSet::BuiltinCRLSet().get(),
                 CertificateList(), &verify_result);

  if (verify_proc_type() == CERT_VERIFY_PROC_ANDROID) {
    // Android doesn't support PEM - https://crbug.com/725180
    EXPECT_THAT(error, IsError(ERR_CERT_AUTHORITY_INVALID));
  } else {
    EXPECT_THAT(error, IsOk());
  }
}

// Tries verifying a certificate chain that uses a SHA1 intermediate,
// however, chasing the AIA can discover a SHA256 version of the intermediate.
//
// Path building should discover the stronger intermediate and use it.
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       Sha1IntermediateButAIAHasSha256) {
  const char kHostname[] = "www.example.com";

  base::FilePath certs_dir =
      GetTestNetDataDirectory()
          .AppendASCII("verify_certificate_chain_unittest")
          .AppendASCII("target-and-intermediate");

  CertificateList orig_certs = CreateCertificateListFromFile(
      certs_dir, "chain.pem", X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3U, orig_certs.size());

  // Build slightly modified variants of |orig_certs|.
  CertBuilder root(orig_certs[2]->cert_buffer(), nullptr);
  CertBuilder intermediate(orig_certs[1]->cert_buffer(), &root);
  CertBuilder leaf(orig_certs[0]->cert_buffer(), &intermediate);

  // Make the leaf certificate have an AIA (CA Issuers) that points to the
  // embedded test server. This uses a random URL for predictable behavior in
  // the presence of global caching.
  std::string ca_issuers_path = MakeRandomPath(".cer");
  GURL ca_issuers_url = GetTestServerAbsoluteUrl(ca_issuers_path);
  leaf.SetCaIssuersUrl(ca_issuers_url);
  leaf.SetSubjectAltName(kHostname);

  // Make two versions of the intermediate - one that is SHA256 signed, and one
  // that is SHA1 signed.
  intermediate.SetSignatureAlgorithmRsaPkca1(DigestAlgorithm::Sha256);
  intermediate.SetRandomSerialNumber();
  auto intermediate_sha256 = intermediate.DupCertBuffer();

  intermediate.SetSignatureAlgorithmRsaPkca1(DigestAlgorithm::Sha1);
  intermediate.SetRandomSerialNumber();
  auto intermediate_sha1 = intermediate.DupCertBuffer();

  // Trust the root certificate.
  auto root_cert = root.GetX509Certificate();
  ScopedTestRoot scoped_root(root_cert.get());

  // Setup the test server to reply with the SHA256 intermediate.
  RegisterSimpleTestServerHandler(
      ca_issuers_path, HTTP_OK, "application/pkix-cert",
      x509_util::CryptoBufferAsStringPiece(intermediate_sha256.get())
          .as_string());

  // Build a chain to verify that includes the SHA1 intermediate.
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(bssl::UpRef(intermediate_sha1.get()));
  scoped_refptr<X509Certificate> chain_sha1 = X509Certificate::CreateFromBuffer(
      leaf.DupCertBuffer(), std::move(intermediates));
  ASSERT_TRUE(chain_sha1.get());

  const int flags = 0;
  CertVerifyResult verify_result;
  int error =
      Verify(chain_sha1.get(), kHostname, flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);

  if (verify_proc_type() == CERT_VERIFY_PROC_BUILTIN ||
      verify_proc_type() == CERT_VERIFY_PROC_MAC) {
    // Should have built a chain through the SHA256 intermediate. This was only
    // available via AIA, and not the (SHA1) one provided directly to path
    // building.
    ASSERT_EQ(2u, verify_result.verified_cert->intermediate_buffers().size());
    EXPECT_TRUE(x509_util::CryptoBufferEqual(
        verify_result.verified_cert->intermediate_buffers()[0].get(),
        intermediate_sha256.get()));
    ASSERT_EQ(2u, verify_result.verified_cert->intermediate_buffers().size());

    EXPECT_FALSE(verify_result.has_sha1);
    EXPECT_THAT(error, IsOk());
  } else if (verify_proc_type() == CERT_VERIFY_PROC_WIN) {
    // TODO(eroman): Make these test expectations exact.
    // This seemed to be working on Windows when !AreSHA1IntermediatesAllowed()
    // from previous testing, but then failed on the Windows 10 bot.
    if (error != OK) {
      EXPECT_TRUE(verify_result.cert_status &
                  CERT_STATUS_WEAK_SIGNATURE_ALGORITHM);
      EXPECT_TRUE(verify_result.cert_status &
                  CERT_STATUS_SHA1_SIGNATURE_PRESENT);
      EXPECT_TRUE(verify_result.has_sha1);
      EXPECT_THAT(error, IsError(ERR_CERT_WEAK_SIGNATURE_ALGORITHM));
    }
  } else {
    EXPECT_NE(OK, error);
    EXPECT_TRUE(verify_result.cert_status &
                CERT_STATUS_WEAK_SIGNATURE_ALGORITHM);
    EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_SHA1_SIGNATURE_PRESENT);
    EXPECT_TRUE(verify_result.has_sha1);
  }
}

TEST_P(CertVerifyProcInternalWithNetFetchingTest, RevocationHardFailNoCrls) {
  if (!SupportsRevCheckingRequiredLocalAnchors()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS";
    return;
  }

  // Create certs which have no AIA or CRL distribution points.
  const char kHostname[] = "www.example.com";
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CertBuilder::CreateSimpleChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate().get());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with hard-fail revocation checking for local anchors.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), kHostname, flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);

  EXPECT_THAT(error, IsError(ERR_CERT_NO_REVOCATION_MECHANISM));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

// CRL hard fail test where both leaf and intermediate are covered by valid
// CRLs which have empty (non-present) revokedCertificates list. Verification
// should succeed.
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationHardFailCrlGoodNoRevokedCertificates) {
  if (!SupportsRevCheckingRequiredLocalAnchors()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS";
    return;
  }

  const char kHostname[] = "www.example.com";
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CertBuilder::CreateSimpleChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);

  // Serve a root-issued CRL which does not revoke intermediate.
  intermediate->SetCrlDistributionPointUrl(CreateAndServeCrl(root.get(), {}));

  // Serve an intermediate-issued CRL which does not revoke leaf.
  leaf->SetCrlDistributionPointUrl(CreateAndServeCrl(intermediate.get(), {}));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate().get());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with hard-fail revocation checking for local anchors.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), kHostname, flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);

  // Should pass, leaf and intermediate were covered by CRLs and were not
  // revoked.
  EXPECT_THAT(error, IsOk());
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

// CRL hard fail test where both leaf and intermediate are covered by valid
// CRLs which have revokedCertificates lists that revoke other irrelevant
// serial numbers. Verification should succeed.
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationHardFailCrlGoodIrrelevantSerialsRevoked) {
  if (!SupportsRevCheckingRequiredLocalAnchors()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS";
    return;
  }

  const char kHostname[] = "www.example.com";
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CertBuilder::CreateSimpleChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);

  // Root-issued CRL revokes leaf's serial number. This is irrelevant.
  intermediate->SetCrlDistributionPointUrl(
      CreateAndServeCrl(root.get(), {leaf->GetSerialNumber()}));

  // Intermediate-issued CRL revokes intermediates's serial number. This is
  // irrelevant.
  leaf->SetCrlDistributionPointUrl(
      CreateAndServeCrl(intermediate.get(), {intermediate->GetSerialNumber()}));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate().get());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with hard-fail revocation checking for local anchors.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), kHostname, flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);

  // Should pass, leaf and intermediate were covered by CRLs and were not
  // revoked.
  EXPECT_THAT(error, IsOk());
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationHardFailLeafRevokedByCrl) {
  if (!SupportsRevCheckingRequiredLocalAnchors()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS";
    return;
  }

  const char kHostname[] = "www.example.com";
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CertBuilder::CreateSimpleChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);

  // Root-issued CRL which does not revoke intermediate.
  intermediate->SetCrlDistributionPointUrl(CreateAndServeCrl(root.get(), {}));

  // Leaf is revoked by intermediate issued CRL.
  leaf->SetCrlDistributionPointUrl(
      CreateAndServeCrl(intermediate.get(), {leaf->GetSerialNumber()}));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate().get());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with hard-fail revocation checking for local anchors.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), kHostname, flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);

  // Should fail, leaf is revoked.
  EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationHardFailIntermediateRevokedByCrl) {
  if (!SupportsRevCheckingRequiredLocalAnchors()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS";
    return;
  }

  const char kHostname[] = "www.example.com";
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CertBuilder::CreateSimpleChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);

  // Intermediate is revoked by root issued CRL.
  intermediate->SetCrlDistributionPointUrl(
      CreateAndServeCrl(root.get(), {intermediate->GetSerialNumber()}));

  // Intermediate-issued CRL which does not revoke leaf.
  leaf->SetCrlDistributionPointUrl(CreateAndServeCrl(intermediate.get(), {}));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate().get());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with hard-fail revocation checking for local anchors.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), kHostname, flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);

  // Should fail, intermediate is revoked.
  EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

// CRL hard fail test where the intermediate certificate has a good CRL, but
// the leaf's distribution point returns an http error. Verification should
// fail.
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationHardFailLeafCrlDpHttpError) {
  if (!SupportsRevCheckingRequiredLocalAnchors()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS";
    return;
  }

  const char kHostname[] = "www.example.com";
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CertBuilder::CreateSimpleChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);

  // Serve a root-issued CRL which does not revoke intermediate.
  intermediate->SetCrlDistributionPointUrl(CreateAndServeCrl(root.get(), {}));

  // Serve a 404 for the intermediate-issued CRL distribution point url.
  leaf->SetCrlDistributionPointUrl(RegisterSimpleTestServerHandler(
      MakeRandomPath(".crl"), HTTP_NOT_FOUND, "text/plain", "Not Found"));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate().get());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with hard-fail revocation checking for local anchors.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), kHostname, flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);

  // Should fail since no revocation information was available for the leaf.
  EXPECT_THAT(error, IsError(ERR_CERT_UNABLE_TO_CHECK_REVOCATION));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

// CRL hard fail test where the leaf certificate has a good CRL, but
// the intermediate's distribution point returns an http error. Verification
// should fail.
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationHardFailIntermediateCrlDpHttpError) {
  if (!SupportsRevCheckingRequiredLocalAnchors()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS";
    return;
  }

  const char kHostname[] = "www.example.com";
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CertBuilder::CreateSimpleChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);

  // Serve a 404 for the root-issued CRL distribution point url.
  intermediate->SetCrlDistributionPointUrl(RegisterSimpleTestServerHandler(
      MakeRandomPath(".crl"), HTTP_NOT_FOUND, "text/plain", "Not Found"));

  // Serve an intermediate-issued CRL which does not revoke leaf.
  leaf->SetCrlDistributionPointUrl(CreateAndServeCrl(intermediate.get(), {}));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate().get());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with hard-fail revocation checking for local anchors.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), kHostname, flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);

  // Should fail since no revocation information was available for the
  // intermediate.
  EXPECT_THAT(error, IsError(ERR_CERT_UNABLE_TO_CHECK_REVOCATION));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_P(CertVerifyProcInternalWithNetFetchingTest, RevocationSoftFailNoCrls) {
  if (!SupportsSoftFailRevChecking()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_ENABLED";
    return;
  }

  // Create certs which have no AIA or CRL distribution points.
  const char kHostname[] = "www.example.com";
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CertBuilder::CreateSimpleChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate().get());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with soft-fail revocation checking.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_ENABLED;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), kHostname, flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);

  EXPECT_THAT(error, IsOk());
  EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_NO_REVOCATION_MECHANISM);
  EXPECT_FALSE(verify_result.cert_status &
               CERT_STATUS_UNABLE_TO_CHECK_REVOCATION);
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

// CRL soft fail test where both leaf and intermediate are covered by valid
// CRLs which have empty (non-present) revokedCertificates list. Verification
// should succeed.
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationSoftFailCrlGoodNoRevokedCertificates) {
  if (!SupportsSoftFailRevChecking()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_ENABLED";
    return;
  }

  const char kHostname[] = "www.example.com";
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CertBuilder::CreateSimpleChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);

  // Serve a root-issued CRL which does not revoke intermediate.
  intermediate->SetCrlDistributionPointUrl(CreateAndServeCrl(root.get(), {}));

  // Serve an intermediate-issued CRL which does not revoke leaf.
  leaf->SetCrlDistributionPointUrl(CreateAndServeCrl(intermediate.get(), {}));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate().get());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with soft-fail revocation checking.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_ENABLED;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), kHostname, flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);

  EXPECT_THAT(error, IsOk());
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

// CRL soft fail test where both leaf and intermediate are covered by valid
// CRLs which have revokedCertificates lists that revoke other irrelevant
// serial numbers. Verification should succeed.
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationSoftFailCrlGoodIrrelevantSerialsRevoked) {
  if (!SupportsSoftFailRevChecking()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_ENABLED";
    return;
  }

  const char kHostname[] = "www.example.com";
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CertBuilder::CreateSimpleChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);

  // Root-issued CRL revokes leaf's serial number. This is irrelevant.
  intermediate->SetCrlDistributionPointUrl(
      CreateAndServeCrl(root.get(), {leaf->GetSerialNumber()}));

  // Intermediate-issued CRL revokes intermediates's serial number. This is
  // irrelevant.
  leaf->SetCrlDistributionPointUrl(
      CreateAndServeCrl(intermediate.get(), {intermediate->GetSerialNumber()}));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate().get());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with soft-fail revocation checking.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_ENABLED;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), kHostname, flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);

  EXPECT_THAT(error, IsOk());
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationSoftFailLeafRevokedByCrl) {
  if (!SupportsSoftFailRevChecking()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_ENABLED";
    return;
  }

  const char kHostname[] = "www.example.com";
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CertBuilder::CreateSimpleChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);

  // Root-issued CRL which does not revoke intermediate.
  intermediate->SetCrlDistributionPointUrl(CreateAndServeCrl(root.get(), {}));

  // Leaf is revoked by intermediate issued CRL.
  leaf->SetCrlDistributionPointUrl(
      CreateAndServeCrl(intermediate.get(), {leaf->GetSerialNumber()}));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate().get());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with soft-fail revocation checking.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_ENABLED;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), kHostname, flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);

  if (verify_proc_type() == CERT_VERIFY_PROC_MAC && IsMacAtLeastOS10_12()) {
    // CRL handling seems broken on macOS >= 10.12.
    // TODO(mattm): followup on this.
    EXPECT_THAT(error, IsOk());
  } else {
    // Should fail, leaf is revoked.
    EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));
  }
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationSoftFailIntermediateRevokedByCrl) {
  if (!SupportsSoftFailRevChecking()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_ENABLED";
    return;
  }

  const char kHostname[] = "www.example.com";
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CertBuilder::CreateSimpleChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);

  // Intermediate is revoked by root issued CRL.
  intermediate->SetCrlDistributionPointUrl(
      CreateAndServeCrl(root.get(), {intermediate->GetSerialNumber()}));

  // Intermediate-issued CRL which does not revoke leaf.
  leaf->SetCrlDistributionPointUrl(CreateAndServeCrl(intermediate.get(), {}));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate().get());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with soft-fail revocation checking.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_ENABLED;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), kHostname, flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);

  if (verify_proc_type() == CERT_VERIFY_PROC_MAC && IsMacAtLeastOS10_12()) {
    // CRL handling seems broken on macOS >= 10.12.
    // TODO(mattm): followup on this.
    EXPECT_THAT(error, IsOk());
  } else {
    // Should fail, intermediate is revoked.
    EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));
  }
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationSoftFailLeafRevokedBySha1Crl) {
  if (!SupportsSoftFailRevChecking()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_ENABLED";
    return;
  }

  const char kHostname[] = "www.example.com";
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CertBuilder::CreateSimpleChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);

  // Root-issued CRL which does not revoke intermediate.
  intermediate->SetCrlDistributionPointUrl(CreateAndServeCrl(root.get(), {}));

  // Leaf is revoked by intermediate issued CRL which is signed with
  // sha1WithRSAEncryption.
  leaf->SetCrlDistributionPointUrl(CreateAndServeCrl(
      intermediate.get(), {leaf->GetSerialNumber()}, DigestAlgorithm::Sha1));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate().get());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with soft-fail revocation checking.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_ENABLED;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), kHostname, flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);

  if (verify_proc_type() == CERT_VERIFY_PROC_MAC && IsMacAtLeastOS10_12()) {
    // CRL handling seems broken on macOS >= 10.12.
    // TODO(mattm): followup on this.
    EXPECT_THAT(error, IsOk());
  } else {
    // Should fail, leaf is revoked.
    EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));
  }
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationSoftFailLeafRevokedByMd5Crl) {
  if (!SupportsSoftFailRevChecking()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_ENABLED";
    return;
  }

  const char kHostname[] = "www.example.com";
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CertBuilder::CreateSimpleChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);

  // Root-issued CRL which does not revoke intermediate.
  intermediate->SetCrlDistributionPointUrl(CreateAndServeCrl(root.get(), {}));

  // Leaf is revoked by intermediate issued CRL which is signed with
  // md5WithRSAEncryption.
  leaf->SetCrlDistributionPointUrl(CreateAndServeCrl(
      intermediate.get(), {leaf->GetSerialNumber()}, DigestAlgorithm::Md5));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate().get());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with soft-fail revocation checking.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_ENABLED;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), kHostname, flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);

  if (verify_proc_type() == CERT_VERIFY_PROC_WIN ||
      (verify_proc_type() == CERT_VERIFY_PROC_MAC && !IsMacAtLeastOS10_12())) {
    // Windows and Mac <= 10.11 honor MD5 CRLs. ¯\_(ツ)_/¯
    EXPECT_THAT(error, IsError(ERR_CERT_REVOKED));
  } else {
    // Verification should succeed: MD5 signature algorithm is not supported
    // and soft-fail checking will ignore the inability to get revocation
    // status.
    EXPECT_THAT(error, IsOk());
  }
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

// CRL soft fail test where the intermediate certificate has a good CRL, but
// the leaf's distribution point returns an http error. Verification should
// succeed.
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationSoftFailLeafCrlDpHttpError) {
  if (!SupportsSoftFailRevChecking()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_ENABLED";
    return;
  }

  const char kHostname[] = "www.example.com";
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CertBuilder::CreateSimpleChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);

  // Serve a root-issued CRL which does not revoke intermediate.
  intermediate->SetCrlDistributionPointUrl(CreateAndServeCrl(root.get(), {}));

  // Serve a 404 for the intermediate-issued CRL distribution point url.
  leaf->SetCrlDistributionPointUrl(RegisterSimpleTestServerHandler(
      MakeRandomPath(".crl"), HTTP_NOT_FOUND, "text/plain", "Not Found"));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate().get());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with soft-fail revocation checking.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_ENABLED;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), kHostname, flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);

  // Should succeed due to soft-fail revocation checking.
  EXPECT_THAT(error, IsOk());
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

// CRL soft fail test where the leaf certificate has a good CRL, but
// the intermediate's distribution point returns an http error. Verification
// should succeed.
TEST_P(CertVerifyProcInternalWithNetFetchingTest,
       RevocationSoftFailIntermediateCrlDpHttpError) {
  if (!SupportsSoftFailRevChecking()) {
    LOG(INFO) << "Skipping test as verifier doesn't support "
                 "VERIFY_REV_CHECKING_ENABLED";
    return;
  }

  const char kHostname[] = "www.example.com";
  std::unique_ptr<CertBuilder> leaf, intermediate, root;
  CertBuilder::CreateSimpleChain(&leaf, &intermediate, &root);
  ASSERT_TRUE(leaf && intermediate && root);

  // Serve a 404 for the root-issued CRL distribution point url.
  intermediate->SetCrlDistributionPointUrl(RegisterSimpleTestServerHandler(
      MakeRandomPath(".crl"), HTTP_NOT_FOUND, "text/plain", "Not Found"));

  // Serve an intermediate-issued CRL which does not revoke leaf.
  leaf->SetCrlDistributionPointUrl(CreateAndServeCrl(intermediate.get(), {}));

  // Trust the root and build a chain to verify that includes the intermediate.
  ScopedTestRoot scoped_root(root->GetX509Certificate().get());
  scoped_refptr<X509Certificate> chain = leaf->GetX509CertificateChain();
  ASSERT_TRUE(chain.get());

  // Verify with soft-fail revocation checking.
  const int flags = CertVerifyProc::VERIFY_REV_CHECKING_ENABLED;
  CertVerifyResult verify_result;
  int error =
      Verify(chain.get(), kHostname, flags, CRLSet::BuiltinCRLSet().get(),
             CertificateList(), &verify_result);

  // Should succeed due to soft-fail revocation checking.
  EXPECT_THAT(error, IsOk());
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_REV_CHECKING_ENABLED);
}

TEST(CertVerifyProcTest, RejectsMD2) {
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(cert);

  CertVerifyResult result;
  result.has_md2 = true;
  scoped_refptr<CertVerifyProc> verify_proc = new MockCertVerifyProc(result);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(
      cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
      CertificateList(), &verify_result, NetLogWithSource());
  EXPECT_THAT(error, IsError(ERR_CERT_INVALID));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_INVALID);
}

TEST(CertVerifyProcTest, RejectsMD4) {
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(cert);

  CertVerifyResult result;
  result.has_md4 = true;
  scoped_refptr<CertVerifyProc> verify_proc = new MockCertVerifyProc(result);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(
      cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
      CertificateList(), &verify_result, NetLogWithSource());
  EXPECT_THAT(error, IsError(ERR_CERT_INVALID));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_INVALID);
}

TEST(CertVerifyProcTest, RejectsMD5) {
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(cert);

  CertVerifyResult result;
  result.has_md5 = true;
  scoped_refptr<CertVerifyProc> verify_proc = new MockCertVerifyProc(result);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(
      cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
      CertificateList(), &verify_result, NetLogWithSource());
  EXPECT_THAT(error, IsError(ERR_CERT_WEAK_SIGNATURE_ALGORITHM));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_WEAK_SIGNATURE_ALGORITHM);
}

TEST(CertVerifyProcTest, RejectsPublicSHA1Leaves) {
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(cert);

  CertVerifyResult result;
  result.has_sha1 = true;
  result.has_sha1_leaf = true;
  result.is_issued_by_known_root = true;
  scoped_refptr<CertVerifyProc> verify_proc = new MockCertVerifyProc(result);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(
      cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
      CertificateList(), &verify_result, NetLogWithSource());
  EXPECT_THAT(error, IsError(ERR_CERT_WEAK_SIGNATURE_ALGORITHM));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_WEAK_SIGNATURE_ALGORITHM);
}

TEST(CertVerifyProcTest, RejectsPublicSHA1IntermediatesUnlessAllowed) {
  scoped_refptr<X509Certificate> cert(ImportCertFromFile(
      GetTestCertsDirectory(), "39_months_after_2015_04.pem"));
  ASSERT_TRUE(cert);

  CertVerifyResult result;
  result.has_sha1 = true;
  result.has_sha1_leaf = false;
  result.is_issued_by_known_root = true;
  scoped_refptr<CertVerifyProc> verify_proc = new MockCertVerifyProc(result);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(
      cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
      CertificateList(), &verify_result, NetLogWithSource());
  if (AreSHA1IntermediatesAllowed()) {
    EXPECT_THAT(error, IsOk());
    EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_SHA1_SIGNATURE_PRESENT);
  } else {
    EXPECT_THAT(error, IsError(ERR_CERT_WEAK_SIGNATURE_ALGORITHM));
    EXPECT_TRUE(verify_result.cert_status &
                CERT_STATUS_WEAK_SIGNATURE_ALGORITHM);
  }
}

TEST(CertVerifyProcTest, RejectsPrivateSHA1UnlessFlag) {
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(cert);

  CertVerifyResult result;
  result.has_sha1 = true;
  result.has_sha1_leaf = true;
  result.is_issued_by_known_root = false;
  scoped_refptr<CertVerifyProc> verify_proc = new MockCertVerifyProc(result);

  // SHA-1 should be rejected by default for private roots...
  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(
      cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
      CertificateList(), &verify_result, NetLogWithSource());
  EXPECT_THAT(error, IsError(ERR_CERT_WEAK_SIGNATURE_ALGORITHM));
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_SHA1_SIGNATURE_PRESENT);

  // ... unless VERIFY_ENABLE_SHA1_LOCAL_ANCHORS was supplied.
  flags = CertVerifyProc::VERIFY_ENABLE_SHA1_LOCAL_ANCHORS;
  verify_result.Reset();
  error = verify_proc->Verify(
      cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
      CertificateList(), &verify_result, NetLogWithSource());
  EXPECT_THAT(error, IsOk());
  EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_SHA1_SIGNATURE_PRESENT);
}

enum ExpectedAlgorithms {
  EXPECT_MD2 = 1 << 0,
  EXPECT_MD4 = 1 << 1,
  EXPECT_MD5 = 1 << 2,
  EXPECT_SHA1 = 1 << 3,
  EXPECT_SHA1_LEAF = 1 << 4,
};

struct WeakDigestTestData {
  const char* root_cert_filename;
  const char* intermediate_cert_filename;
  const char* ee_cert_filename;
  int expected_algorithms;
};

const char* StringOrDefault(const char* str, const char* default_value) {
  if (!str)
    return default_value;
  return str;
}

// GTest 'magic' pretty-printer, so that if/when a test fails, it knows how
// to output the parameter that was passed. Without this, it will simply
// attempt to print out the first twenty bytes of the object, which depending
// on platform and alignment, may result in an invalid read.
void PrintTo(const WeakDigestTestData& data, std::ostream* os) {
  *os << "root: " << StringOrDefault(data.root_cert_filename, "none")
      << "; intermediate: "
      << StringOrDefault(data.intermediate_cert_filename, "none")
      << "; end-entity: " << data.ee_cert_filename;
}

class CertVerifyProcWeakDigestTest
    : public testing::TestWithParam<WeakDigestTestData> {
 public:
  CertVerifyProcWeakDigestTest() = default;
  virtual ~CertVerifyProcWeakDigestTest() = default;
};

// Tests that the CertVerifyProc::Verify() properly surfaces the (weak) hash
// algorithms used in the chain.
TEST_P(CertVerifyProcWeakDigestTest, VerifyDetectsAlgorithm) {
  WeakDigestTestData data = GetParam();
  base::FilePath certs_dir = GetTestCertsDirectory();

  // Build |intermediates| as the full chain (including trust anchor).
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;

  if (data.intermediate_cert_filename) {
    scoped_refptr<X509Certificate> intermediate_cert =
        ImportCertFromFile(certs_dir, data.intermediate_cert_filename);
    ASSERT_TRUE(intermediate_cert);
    intermediates.push_back(bssl::UpRef(intermediate_cert->cert_buffer()));
  }

  if (data.root_cert_filename) {
    scoped_refptr<X509Certificate> root_cert =
        ImportCertFromFile(certs_dir, data.root_cert_filename);
    ASSERT_TRUE(root_cert);
    intermediates.push_back(bssl::UpRef(root_cert->cert_buffer()));
  }

  scoped_refptr<X509Certificate> ee_cert =
      ImportCertFromFile(certs_dir, data.ee_cert_filename);
  ASSERT_TRUE(ee_cert);

  scoped_refptr<X509Certificate> ee_chain = X509Certificate::CreateFromBuffer(
      bssl::UpRef(ee_cert->cert_buffer()), std::move(intermediates));
  ASSERT_TRUE(ee_chain);

  int flags = 0;
  CertVerifyResult verify_result;

  // Use a mock CertVerifyProc that returns success with a verified_cert of
  // |ee_chain|.
  //
  // This is sufficient for the purposes of this test, as the checking for weak
  // hash algorithms is done by CertVerifyProc::Verify().
  scoped_refptr<CertVerifyProc> proc =
      new MockCertVerifyProc(CertVerifyResult());
  proc->Verify(ee_chain.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
               /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
               CertificateList(), &verify_result, NetLogWithSource());
  EXPECT_EQ(!!(data.expected_algorithms & EXPECT_MD2), verify_result.has_md2);
  EXPECT_EQ(!!(data.expected_algorithms & EXPECT_MD4), verify_result.has_md4);
  EXPECT_EQ(!!(data.expected_algorithms & EXPECT_MD5), verify_result.has_md5);
  EXPECT_EQ(!!(data.expected_algorithms & EXPECT_SHA1), verify_result.has_sha1);
  EXPECT_EQ(!!(data.expected_algorithms & EXPECT_SHA1_LEAF),
            verify_result.has_sha1_leaf);
}

// The signature algorithm of the root CA should not matter.
const WeakDigestTestData kVerifyRootCATestData[] = {
    {"weak_digest_md5_root.pem", "weak_digest_sha1_intermediate.pem",
     "weak_digest_sha1_ee.pem", EXPECT_SHA1 | EXPECT_SHA1_LEAF},
    {"weak_digest_md4_root.pem", "weak_digest_sha1_intermediate.pem",
     "weak_digest_sha1_ee.pem", EXPECT_SHA1 | EXPECT_SHA1_LEAF},
    {"weak_digest_md2_root.pem", "weak_digest_sha1_intermediate.pem",
     "weak_digest_sha1_ee.pem", EXPECT_SHA1 | EXPECT_SHA1_LEAF},
};
INSTANTIATE_TEST_SUITE_P(VerifyRoot,
                         CertVerifyProcWeakDigestTest,
                         testing::ValuesIn(kVerifyRootCATestData));

// The signature algorithm of intermediates should be properly detected.
const WeakDigestTestData kVerifyIntermediateCATestData[] = {
    {"weak_digest_sha1_root.pem", "weak_digest_md5_intermediate.pem",
     "weak_digest_sha1_ee.pem", EXPECT_MD5 | EXPECT_SHA1 | EXPECT_SHA1_LEAF},
    {"weak_digest_sha1_root.pem", "weak_digest_md4_intermediate.pem",
     "weak_digest_sha1_ee.pem", EXPECT_MD4 | EXPECT_SHA1 | EXPECT_SHA1_LEAF},
    {"weak_digest_sha1_root.pem", "weak_digest_md2_intermediate.pem",
     "weak_digest_sha1_ee.pem", EXPECT_MD2 | EXPECT_SHA1 | EXPECT_SHA1_LEAF},
};

INSTANTIATE_TEST_SUITE_P(VerifyIntermediate,
                         CertVerifyProcWeakDigestTest,
                         testing::ValuesIn(kVerifyIntermediateCATestData));

// The signature algorithm of end-entity should be properly detected.
const WeakDigestTestData kVerifyEndEntityTestData[] = {
    {"weak_digest_sha1_root.pem", "weak_digest_sha1_intermediate.pem",
     "weak_digest_md5_ee.pem", EXPECT_MD5 | EXPECT_SHA1},
    {"weak_digest_sha1_root.pem", "weak_digest_sha1_intermediate.pem",
     "weak_digest_md4_ee.pem", EXPECT_MD4 | EXPECT_SHA1},
    {"weak_digest_sha1_root.pem", "weak_digest_sha1_intermediate.pem",
     "weak_digest_md2_ee.pem", EXPECT_MD2 | EXPECT_SHA1},
};

INSTANTIATE_TEST_SUITE_P(VerifyEndEntity,
                         CertVerifyProcWeakDigestTest,
                         testing::ValuesIn(kVerifyEndEntityTestData));

// Incomplete chains do not report the status of the intermediate.
// Note: really each of these tests should also expect the digest algorithm of
// the intermediate (included as a comment). However CertVerifyProc::Verify() is
// unable to distinguish that this is an intermediate and not a trust anchor, so
// this intermediate is treated like a trust anchor.
const WeakDigestTestData kVerifyIncompleteIntermediateTestData[] = {
    {nullptr, "weak_digest_md5_intermediate.pem", "weak_digest_sha1_ee.pem",
     /*EXPECT_MD5 |*/ EXPECT_SHA1 | EXPECT_SHA1_LEAF},
    {nullptr, "weak_digest_md4_intermediate.pem", "weak_digest_sha1_ee.pem",
     /*EXPECT_MD4 |*/ EXPECT_SHA1 | EXPECT_SHA1_LEAF},
    {nullptr, "weak_digest_md2_intermediate.pem", "weak_digest_sha1_ee.pem",
     /*EXPECT_MD2 |*/ EXPECT_SHA1 | EXPECT_SHA1_LEAF},
};

INSTANTIATE_TEST_SUITE_P(
    MAYBE_VerifyIncompleteIntermediate,
    CertVerifyProcWeakDigestTest,
    testing::ValuesIn(kVerifyIncompleteIntermediateTestData));

// Incomplete chains should report the status of the end-entity.
// Note: really each of these tests should also expect EXPECT_SHA1 (included as
// a comment). However CertVerifyProc::Verify() is unable to distinguish that
// this is an intermediate and not a trust anchor, so this intermediate is
// treated like a trust anchor.
const WeakDigestTestData kVerifyIncompleteEETestData[] = {
    {nullptr, "weak_digest_sha1_intermediate.pem", "weak_digest_md5_ee.pem",
     /*EXPECT_SHA1 |*/ EXPECT_MD5},
    {nullptr, "weak_digest_sha1_intermediate.pem", "weak_digest_md4_ee.pem",
     /*EXPECT_SHA1 |*/ EXPECT_MD4},
    {nullptr, "weak_digest_sha1_intermediate.pem", "weak_digest_md2_ee.pem",
     /*EXPECT_SHA1 |*/ EXPECT_MD2},
};

INSTANTIATE_TEST_SUITE_P(VerifyIncompleteEndEntity,
                         CertVerifyProcWeakDigestTest,
                         testing::ValuesIn(kVerifyIncompleteEETestData));

// Differing algorithms between the intermediate and the EE should still be
// reported.
const WeakDigestTestData kVerifyMixedTestData[] = {
    {"weak_digest_sha1_root.pem", "weak_digest_md5_intermediate.pem",
     "weak_digest_md2_ee.pem", EXPECT_MD2 | EXPECT_MD5},
    {"weak_digest_sha1_root.pem", "weak_digest_md2_intermediate.pem",
     "weak_digest_md5_ee.pem", EXPECT_MD2 | EXPECT_MD5},
    {"weak_digest_sha1_root.pem", "weak_digest_md4_intermediate.pem",
     "weak_digest_md2_ee.pem", EXPECT_MD2 | EXPECT_MD4},
};

INSTANTIATE_TEST_SUITE_P(VerifyMixed,
                         CertVerifyProcWeakDigestTest,
                         testing::ValuesIn(kVerifyMixedTestData));

// The EE is a trusted certificate. Even though it uses weak hashes, these
// should not be reported.
const WeakDigestTestData kVerifyTrustedEETestData[] = {
    {nullptr, nullptr, "weak_digest_md5_ee.pem", 0},
    {nullptr, nullptr, "weak_digest_md4_ee.pem", 0},
    {nullptr, nullptr, "weak_digest_md2_ee.pem", 0},
    {nullptr, nullptr, "weak_digest_sha1_ee.pem", 0},
};

INSTANTIATE_TEST_SUITE_P(VerifyTrustedEE,
                         CertVerifyProcWeakDigestTest,
                         testing::ValuesIn(kVerifyTrustedEETestData));

// Test fixture for verifying certificate names.
class CertVerifyProcNameTest : public ::testing::Test {
 protected:
  void VerifyCertName(const char* hostname, bool valid) {
    scoped_refptr<X509Certificate> cert(ImportCertFromFile(
        GetTestCertsDirectory(), "subjectAltName_sanity_check.pem"));
    ASSERT_TRUE(cert);
    CertVerifyResult result;
    result.is_issued_by_known_root = false;
    scoped_refptr<CertVerifyProc> verify_proc = new MockCertVerifyProc(result);

    CertVerifyResult verify_result;
    int error = verify_proc->Verify(
        cert.get(), hostname, /*ocsp_response=*/std::string(),
        /*sct_list=*/std::string(), 0, CRLSet::BuiltinCRLSet().get(),
        CertificateList(), &verify_result, NetLogWithSource());
    if (valid) {
      EXPECT_THAT(error, IsOk());
      EXPECT_FALSE(verify_result.cert_status & CERT_STATUS_COMMON_NAME_INVALID);
    } else {
      EXPECT_THAT(error, IsError(ERR_CERT_COMMON_NAME_INVALID));
      EXPECT_TRUE(verify_result.cert_status & CERT_STATUS_COMMON_NAME_INVALID);
    }
  }
};

// Don't match the common name
TEST_F(CertVerifyProcNameTest, DontMatchCommonName) {
  VerifyCertName("127.0.0.1", false);
}

// Matches the iPAddress SAN (IPv4)
TEST_F(CertVerifyProcNameTest, MatchesIpSanIpv4) {
  VerifyCertName("127.0.0.2", true);
}

// Matches the iPAddress SAN (IPv6)
TEST_F(CertVerifyProcNameTest, MatchesIpSanIpv6) {
  VerifyCertName("FE80:0:0:0:0:0:0:1", true);
}

// Should not match the iPAddress SAN
TEST_F(CertVerifyProcNameTest, DoesntMatchIpSanIpv6) {
  VerifyCertName("[FE80:0:0:0:0:0:0:1]", false);
}

// Compressed form matches the iPAddress SAN (IPv6)
TEST_F(CertVerifyProcNameTest, MatchesIpSanCompressedIpv6) {
  VerifyCertName("FE80::1", true);
}

// IPv6 mapped form should NOT match iPAddress SAN
TEST_F(CertVerifyProcNameTest, DoesntMatchIpSanIPv6Mapped) {
  VerifyCertName("::127.0.0.2", false);
}

// Matches the dNSName SAN
TEST_F(CertVerifyProcNameTest, MatchesDnsSan) {
  VerifyCertName("test.example", true);
}

// Matches the dNSName SAN (trailing . ignored)
TEST_F(CertVerifyProcNameTest, MatchesDnsSanTrailingDot) {
  VerifyCertName("test.example.", true);
}

// Should not match the dNSName SAN
TEST_F(CertVerifyProcNameTest, DoesntMatchDnsSan) {
  VerifyCertName("www.test.example", false);
}

// Should not match the dNSName SAN
TEST_F(CertVerifyProcNameTest, DoesntMatchDnsSanInvalid) {
  VerifyCertName("test..example", false);
}

// Should not match the dNSName SAN
TEST_F(CertVerifyProcNameTest, DoesntMatchDnsSanTwoTrailingDots) {
  VerifyCertName("test.example..", false);
}

// Should not match the dNSName SAN
TEST_F(CertVerifyProcNameTest, DoesntMatchDnsSanLeadingAndTrailingDot) {
  VerifyCertName(".test.example.", false);
}

// Should not match the dNSName SAN
TEST_F(CertVerifyProcNameTest, DoesntMatchDnsSanTrailingDot) {
  VerifyCertName(".test.example", false);
}

// Tests that CertVerifyProc records a histogram correctly when a
// certificate chaining to a private root contains the TLS feature
// extension and does not have a stapled OCSP response.
TEST(CertVerifyProcTest, HasTLSFeatureExtensionUMA) {
  base::HistogramTester histograms;
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "tls_feature_extension.pem"));
  ASSERT_TRUE(cert);
  CertVerifyResult result;
  result.is_issued_by_known_root = false;
  scoped_refptr<CertVerifyProc> verify_proc = new MockCertVerifyProc(result);

  histograms.ExpectTotalCount(kTLSFeatureExtensionHistogram, 0);
  histograms.ExpectTotalCount(kTLSFeatureExtensionOCSPHistogram, 0);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(
      cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
      CertificateList(), &verify_result, NetLogWithSource());
  EXPECT_EQ(OK, error);
  histograms.ExpectTotalCount(kTLSFeatureExtensionHistogram, 1);
  histograms.ExpectBucketCount(kTLSFeatureExtensionHistogram, true, 1);
  histograms.ExpectTotalCount(kTLSFeatureExtensionOCSPHistogram, 1);
  histograms.ExpectBucketCount(kTLSFeatureExtensionOCSPHistogram, false, 1);
}

// Tests that CertVerifyProc records a histogram correctly when a
// certificate chaining to a private root contains the TLS feature
// extension and does have a stapled OCSP response.
TEST(CertVerifyProcTest, HasTLSFeatureExtensionWithStapleUMA) {
  base::HistogramTester histograms;
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "tls_feature_extension.pem"));
  ASSERT_TRUE(cert);
  CertVerifyResult result;
  result.is_issued_by_known_root = false;
  scoped_refptr<CertVerifyProc> verify_proc = new MockCertVerifyProc(result);

  histograms.ExpectTotalCount(kTLSFeatureExtensionHistogram, 0);
  histograms.ExpectTotalCount(kTLSFeatureExtensionOCSPHistogram, 0);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(
      cert.get(), "127.0.0.1", "dummy response", /*sct_list=*/std::string(),
      flags, CRLSet::BuiltinCRLSet().get(), CertificateList(), &verify_result,
      NetLogWithSource());
  EXPECT_EQ(OK, error);
  histograms.ExpectTotalCount(kTLSFeatureExtensionHistogram, 1);
  histograms.ExpectBucketCount(kTLSFeatureExtensionHistogram, true, 1);
  histograms.ExpectTotalCount(kTLSFeatureExtensionOCSPHistogram, 1);
  histograms.ExpectBucketCount(kTLSFeatureExtensionOCSPHistogram, true, 1);
}

// Tests that CertVerifyProc records a histogram correctly when a
// certificate chaining to a private root does not contain the TLS feature
// extension.
TEST(CertVerifyProcTest, DoesNotHaveTLSFeatureExtensionUMA) {
  base::HistogramTester histograms;
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(cert);
  CertVerifyResult result;
  result.is_issued_by_known_root = false;
  scoped_refptr<CertVerifyProc> verify_proc = new MockCertVerifyProc(result);

  histograms.ExpectTotalCount(kTLSFeatureExtensionHistogram, 0);
  histograms.ExpectTotalCount(kTLSFeatureExtensionOCSPHistogram, 0);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(
      cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
      CertificateList(), &verify_result, NetLogWithSource());
  EXPECT_EQ(OK, error);
  histograms.ExpectTotalCount(kTLSFeatureExtensionHistogram, 1);
  histograms.ExpectBucketCount(kTLSFeatureExtensionHistogram, false, 1);
  histograms.ExpectTotalCount(kTLSFeatureExtensionOCSPHistogram, 0);
}

// Tests that CertVerifyProc does not record a histogram when a
// certificate contains the TLS feature extension but chains to a public
// root.
TEST(CertVerifyProcTest, HasTLSFeatureExtensionWithPublicRootUMA) {
  base::HistogramTester histograms;
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "tls_feature_extension.pem"));
  ASSERT_TRUE(cert);
  CertVerifyResult result;
  result.is_issued_by_known_root = true;
  scoped_refptr<CertVerifyProc> verify_proc = new MockCertVerifyProc(result);

  histograms.ExpectTotalCount(kTLSFeatureExtensionHistogram, 0);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(
      cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
      CertificateList(), &verify_result, NetLogWithSource());
  EXPECT_EQ(OK, error);
  histograms.ExpectTotalCount(kTLSFeatureExtensionHistogram, 0);
  histograms.ExpectTotalCount(kTLSFeatureExtensionOCSPHistogram, 0);
}

// Test that trust anchors are appropriately recorded via UMA.
TEST(CertVerifyProcTest, HasTrustAnchorVerifyUMA) {
  base::HistogramTester histograms;
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(cert);

  CertVerifyResult result;

  // Simulate a certificate chain issued by "C=US, O=Google Trust Services LLC,
  // CN=GTS Root R4". This publicly-trusted root was chosen as it was included
  // in 2017 and is not anticipated to be removed from all supported platforms
  // for a few decades.
  // Note: The actual cert in |cert| does not matter for this testing, so long
  // as it's not violating any CertVerifyProc::Verify() policies.
  SHA256HashValue leaf_hash = {{0}};
  SHA256HashValue intermediate_hash = {{1}};
  SHA256HashValue root_hash = {
      {0x98, 0x47, 0xe5, 0x65, 0x3e, 0x5e, 0x9e, 0x84, 0x75, 0x16, 0xe5,
       0xcb, 0x81, 0x86, 0x06, 0xaa, 0x75, 0x44, 0xa1, 0x9b, 0xe6, 0x7f,
       0xd7, 0x36, 0x6d, 0x50, 0x69, 0x88, 0xe8, 0xd8, 0x43, 0x47}};
  result.public_key_hashes.push_back(HashValue(leaf_hash));
  result.public_key_hashes.push_back(HashValue(intermediate_hash));
  result.public_key_hashes.push_back(HashValue(root_hash));

  const base::HistogramBase::Sample kGTSRootR4HistogramID = 486;

  scoped_refptr<CertVerifyProc> verify_proc = new MockCertVerifyProc(result);

  histograms.ExpectTotalCount(kTrustAnchorVerifyHistogram, 0);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(
      cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
      CertificateList(), &verify_result, NetLogWithSource());
  EXPECT_EQ(OK, error);
  histograms.ExpectUniqueSample(kTrustAnchorVerifyHistogram,
                                kGTSRootR4HistogramID, 1);
}

// Test that certificates with multiple trust anchors present result in
// only a single trust anchor being recorded, and that being the most specific
// trust anchor.
TEST(CertVerifyProcTest, LogsOnlyMostSpecificTrustAnchorUMA) {
  base::HistogramTester histograms;
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "ok_cert.pem"));
  ASSERT_TRUE(cert);

  CertVerifyResult result;

  // Simulate a chain of "C=US, O=Google Trust Services LLC, CN=GTS Root R4"
  // signing "C=US, O=Google Trust Services LLC, CN=GTS Root R3" signing an
  // intermediate and a leaf.
  // Note: The actual cert in |cert| does not matter for this testing, so long
  // as it's not violating any CertVerifyProc::Verify() policies.
  SHA256HashValue leaf_hash = {{0}};
  SHA256HashValue intermediate_hash = {{1}};
  SHA256HashValue gts_root_r3_hash = {
      {0x41, 0x79, 0xed, 0xd9, 0x81, 0xef, 0x74, 0x74, 0x77, 0xb4, 0x96,
       0x26, 0x40, 0x8a, 0xf4, 0x3d, 0xaa, 0x2c, 0xa7, 0xab, 0x7f, 0x9e,
       0x08, 0x2c, 0x10, 0x60, 0xf8, 0x40, 0x96, 0x77, 0x43, 0x48}};
  SHA256HashValue gts_root_r4_hash = {
      {0x98, 0x47, 0xe5, 0x65, 0x3e, 0x5e, 0x9e, 0x84, 0x75, 0x16, 0xe5,
       0xcb, 0x81, 0x86, 0x06, 0xaa, 0x75, 0x44, 0xa1, 0x9b, 0xe6, 0x7f,
       0xd7, 0x36, 0x6d, 0x50, 0x69, 0x88, 0xe8, 0xd8, 0x43, 0x47}};
  result.public_key_hashes.push_back(HashValue(leaf_hash));
  result.public_key_hashes.push_back(HashValue(intermediate_hash));
  result.public_key_hashes.push_back(HashValue(gts_root_r3_hash));
  result.public_key_hashes.push_back(HashValue(gts_root_r4_hash));

  const base::HistogramBase::Sample kGTSRootR3HistogramID = 485;

  scoped_refptr<CertVerifyProc> verify_proc = new MockCertVerifyProc(result);

  histograms.ExpectTotalCount(kTrustAnchorVerifyHistogram, 0);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(
      cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
      CertificateList(), &verify_result, NetLogWithSource());
  EXPECT_EQ(OK, error);

  // Only GTS Root R3 should be recorded.
  histograms.ExpectUniqueSample(kTrustAnchorVerifyHistogram,
                                kGTSRootR3HistogramID, 1);
}

// Test that trust anchors histograms record whether or not
// is_issued_by_known_root was derived from the OS.
TEST(CertVerifyProcTest, HasTrustAnchorVerifyOutOfDateUMA) {
  base::HistogramTester histograms;
  scoped_refptr<X509Certificate> cert(ImportCertFromFile(
      GetTestCertsDirectory(), "39_months_based_on_last_day.pem"));
  ASSERT_TRUE(cert);

  CertVerifyResult result;

  // Simulate a certificate chain that is recognized as trusted (from a known
  // root), but no certificates in the chain are tracked as known trust
  // anchors.
  SHA256HashValue leaf_hash = {{0}};
  SHA256HashValue intermediate_hash = {{1}};
  SHA256HashValue root_hash = {{2}};
  result.public_key_hashes.push_back(HashValue(leaf_hash));
  result.public_key_hashes.push_back(HashValue(intermediate_hash));
  result.public_key_hashes.push_back(HashValue(root_hash));
  result.is_issued_by_known_root = true;

  scoped_refptr<CertVerifyProc> verify_proc = new MockCertVerifyProc(result);

  histograms.ExpectTotalCount(kTrustAnchorVerifyHistogram, 0);
  histograms.ExpectTotalCount(kTrustAnchorVerifyOutOfDateHistogram, 0);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(
      cert.get(), "127.0.0.1", /*ocsp_response=*/std::string(),
      /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
      CertificateList(), &verify_result, NetLogWithSource());
  EXPECT_EQ(OK, error);
  const base::HistogramBase::Sample kUnknownRootHistogramID = 0;
  histograms.ExpectUniqueSample(kTrustAnchorVerifyHistogram,
                                kUnknownRootHistogramID, 1);
  histograms.ExpectUniqueSample(kTrustAnchorVerifyOutOfDateHistogram, true, 1);
}

// If the CertVerifyProc::VerifyInternal implementation calculated the stapled
// OCSP results in the CertVerifyResult, CertVerifyProc::Verify should not
// re-calculate them.
TEST(CertVerifyProcTest, DoesNotRecalculateStapledOCSPResult) {
  scoped_refptr<X509Certificate> cert = CreateCertificateChainFromFile(
      GetTestCertsDirectory(), "ok_cert_by_intermediate.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert);
  ASSERT_EQ(1U, cert->intermediate_buffers().size());

  CertVerifyResult result;

  result.ocsp_result.response_status = OCSPVerifyResult::PROVIDED;
  result.ocsp_result.revocation_status = OCSPRevocationStatus::GOOD;

  scoped_refptr<CertVerifyProc> verify_proc = new MockCertVerifyProc(result);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(
      cert.get(), "127.0.0.1",
      /*ocsp_response=*/"invalid OCSP data",
      /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
      CertificateList(), &verify_result, NetLogWithSource());
  EXPECT_EQ(OK, error);

  EXPECT_EQ(OCSPVerifyResult::PROVIDED,
            verify_result.ocsp_result.response_status);
  EXPECT_EQ(OCSPRevocationStatus::GOOD,
            verify_result.ocsp_result.revocation_status);
}

TEST(CertVerifyProcTest, CalculateStapledOCSPResultIfNotAlreadyDone) {
  scoped_refptr<X509Certificate> cert = CreateCertificateChainFromFile(
      GetTestCertsDirectory(), "ok_cert_by_intermediate.pem",
      X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert);
  ASSERT_EQ(1U, cert->intermediate_buffers().size());

  CertVerifyResult result;

  // Confirm the default-constructed values are as expected.
  EXPECT_EQ(OCSPVerifyResult::NOT_CHECKED, result.ocsp_result.response_status);
  EXPECT_EQ(OCSPRevocationStatus::UNKNOWN,
            result.ocsp_result.revocation_status);

  scoped_refptr<CertVerifyProc> verify_proc = new MockCertVerifyProc(result);

  int flags = 0;
  CertVerifyResult verify_result;
  int error = verify_proc->Verify(
      cert.get(), "127.0.0.1", /*ocsp_response=*/"invalid OCSP data",
      /*sct_list=*/std::string(), flags, CRLSet::BuiltinCRLSet().get(),
      CertificateList(), &verify_result, NetLogWithSource());
  EXPECT_EQ(OK, error);

  EXPECT_EQ(OCSPVerifyResult::PARSE_RESPONSE_ERROR,
            verify_result.ocsp_result.response_status);
  EXPECT_EQ(OCSPRevocationStatus::UNKNOWN,
            verify_result.ocsp_result.revocation_status);
}

}  // namespace net
