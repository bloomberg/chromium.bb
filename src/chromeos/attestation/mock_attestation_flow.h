// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ATTESTATION_MOCK_ATTESTATION_FLOW_H_
#define CHROMEOS_ATTESTATION_MOCK_ATTESTATION_FLOW_H_

#include <string>

#include "chromeos/attestation/attestation_flow.h"

#include "base/callback.h"
#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"

class AccountId;

namespace chromeos {
namespace attestation {

// A fake server proxy which just appends "_response" to every request if no
// response specified.
class FakeServerProxy : public ServerProxy {
 public:
  FakeServerProxy();
  ~FakeServerProxy() override;

  void set_result(bool result) {
    result_ = result;
  }

  void SendEnrollRequest(const std::string& request,
                         DataCallback callback) override;

  void SendCertificateRequest(const std::string& request,
                              DataCallback callback) override;

  void CheckIfAnyProxyPresent(ProxyPresenceCallback callback) override;

  void set_enroll_response(const std::string& response) {
    enroll_response_ = response;
  }

  void set_cert_response(const std::string& response) {
    cert_response_ = response;
  }

 private:
  bool result_;

  std::string enroll_response_;
  std::string cert_response_;

  DISALLOW_COPY_AND_ASSIGN(FakeServerProxy);
};

class MockServerProxy : public FakeServerProxy {
 public:
  MockServerProxy();
  virtual ~MockServerProxy();

  void DeferToFake(bool result);
  MOCK_METHOD2(SendEnrollRequest, void(const std::string&, DataCallback));
  MOCK_METHOD2(SendCertificateRequest, void(const std::string&, DataCallback));
  MOCK_METHOD0(GetType, PrivacyCAType());
  MOCK_METHOD1(CheckIfAnyProxyPresent, void(ProxyPresenceCallback));

  FakeServerProxy* fake() { return &fake_; }

 private:
  FakeServerProxy fake_;
};

// This class can be used to mock AttestationFlow callbacks.
class MockObserver {
 public:
  MockObserver();
  virtual ~MockObserver();

  MOCK_METHOD2(MockCertificateCallback,
               void(AttestationStatus, const std::string&));
};

class MockAttestationFlow : public AttestationFlow {
 public:
  MockAttestationFlow();
  virtual ~MockAttestationFlow();

  MOCK_METHOD6(GetCertificate,
               void(AttestationCertificateProfile,
                    const AccountId& account_id,
                    const std::string&,
                    bool,
                    const std::string&, /* key_name */
                    CertificateCallback));
};

}  // namespace attestation
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when //chromeos/attestation
// moved to ash
namespace ash {
namespace attestation {
using ::chromeos::attestation::MockAttestationFlow;
}  // namespace attestation
}  // namespace ash

#endif  // CHROMEOS_ATTESTATION_MOCK_ATTESTATION_FLOW_H_
