// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/token_validator_base.h"

#include <vector>

#include "base/atomic_sequence_num.h"
#include "crypto/rsa_private_key.h"
#include "net/cert/x509_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTokenUrl[] = "https://example.com/token";
const char kTokenValidationUrl[] = "https://example.com/validate";
const char kTokenValidationCertIssuer[] = "*";

base::StaticAtomicSequenceNumber g_serial_number;

scoped_refptr<net::X509Certificate> CreateFakeCert(base::Time valid_start,
                                                   base::Time valid_expiry) {
  std::unique_ptr<crypto::RSAPrivateKey> unused_key;
  std::string cert_der;
  net::x509_util::CreateKeyAndSelfSignedCert(
      "CN=subject", g_serial_number.GetNext(), valid_start, valid_expiry,
      &unused_key, &cert_der);
  return net::X509Certificate::CreateFromBytes(cert_der.data(),
                                               cert_der.size());
}

}  // namespace

namespace remoting {

class TestTokenValidator : TokenValidatorBase {
 public:
  explicit TestTokenValidator(const ThirdPartyAuthConfig& config);
  ~TestTokenValidator() override;

  void SelectCertificates(net::CertificateList* selected_certs);

  void ExpectContinueWithCertificate(net::X509Certificate* client_cert);

 protected:
  void ContinueWithCertificate(net::X509Certificate* client_cert,
                               net::SSLPrivateKey* client_private_key) override;

 private:
  void StartValidateRequest(const std::string& token) override {}

  net::X509Certificate* expected_client_cert_ = nullptr;
};

TestTokenValidator::TestTokenValidator(const ThirdPartyAuthConfig& config) :
    TokenValidatorBase(config, "", nullptr) {
}

TestTokenValidator::~TestTokenValidator() {}

void TestTokenValidator::SelectCertificates(
    net::CertificateList* selected_certs) {
  OnCertificatesSelected(selected_certs, nullptr);
}

void TestTokenValidator::ExpectContinueWithCertificate(
    net::X509Certificate* client_cert) {
  expected_client_cert_ = client_cert;
}

void TestTokenValidator::ContinueWithCertificate(
    net::X509Certificate* client_cert,
    net::SSLPrivateKey* client_private_key) {
  EXPECT_EQ(expected_client_cert_, client_cert);
}

class TokenValidatorBaseTest : public testing::Test {
 public:
  void SetUp() override;
 protected:
  std::unique_ptr<TestTokenValidator> token_validator_;
};

void TokenValidatorBaseTest::SetUp() {
  ThirdPartyAuthConfig config;
  config.token_url = GURL(kTokenUrl);
  config.token_validation_url = GURL(kTokenValidationUrl);
  config.token_validation_cert_issuer = kTokenValidationCertIssuer;
  token_validator_.reset(new TestTokenValidator(config));
}

TEST_F(TokenValidatorBaseTest, TestSelectCertificate) {
  base::Time now = base::Time::Now();

  scoped_refptr<net::X509Certificate> cert_expired_5_minutes_ago =
      CreateFakeCert(now - base::TimeDelta::FromMinutes(10),
                                        now - base::TimeDelta::FromMinutes(5));

  scoped_refptr<net::X509Certificate> cert_start_5min_expire_5min =
      CreateFakeCert(now - base::TimeDelta::FromMinutes(5),
                                        now + base::TimeDelta::FromMinutes(5));

  scoped_refptr<net::X509Certificate> cert_start_10min_expire_5min =
      CreateFakeCert(now - base::TimeDelta::FromMinutes(10),
                                        now + base::TimeDelta::FromMinutes(5));

  scoped_refptr<net::X509Certificate> cert_start_5min_expire_10min =
      CreateFakeCert(now - base::TimeDelta::FromMinutes(5),
                                        now + base::TimeDelta::FromMinutes(10));

  // No certificate.
  net::CertificateList certificates {};
  token_validator_->ExpectContinueWithCertificate(nullptr);
  token_validator_->SelectCertificates(&certificates);

  // One invalid certificate.
  certificates = { cert_expired_5_minutes_ago };
  token_validator_->ExpectContinueWithCertificate(nullptr);
  token_validator_->SelectCertificates(&certificates);

  // One valid certificate.
  certificates = { cert_start_5min_expire_5min };
  token_validator_->ExpectContinueWithCertificate(
      cert_start_5min_expire_5min.get());
  token_validator_->SelectCertificates(&certificates);

  // One valid one invalid.
  certificates = { cert_expired_5_minutes_ago, cert_start_5min_expire_5min };
  token_validator_->ExpectContinueWithCertificate(
      cert_start_5min_expire_5min.get());
  token_validator_->SelectCertificates(&certificates);

  // Two valid certs. Choose latest created.
  certificates = { cert_start_10min_expire_5min, cert_start_5min_expire_5min };
  token_validator_->ExpectContinueWithCertificate(
      cert_start_5min_expire_5min.get());
  token_validator_->SelectCertificates(&certificates);

  // Two valid certs. Choose latest expires.
  certificates = { cert_start_5min_expire_5min, cert_start_5min_expire_10min };
  token_validator_->ExpectContinueWithCertificate(
      cert_start_5min_expire_10min.get());
  token_validator_->SelectCertificates(&certificates);

  // Pick the best given all certificates.
  certificates = { cert_expired_5_minutes_ago, cert_start_5min_expire_5min,
      cert_start_5min_expire_10min, cert_start_10min_expire_5min };
    token_validator_->ExpectContinueWithCertificate(
        cert_start_5min_expire_10min.get());
    token_validator_->SelectCertificates(&certificates);
}

}  // namespace remoting
