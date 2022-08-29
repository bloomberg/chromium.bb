// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ATTESTATION_SOFT_BIND_ATTESTATION_FLOW_H_
#define CHROME_BROWSER_ASH_ATTESTATION_SOFT_BIND_ATTESTATION_FLOW_H_

#include <string>

#include "ash/components/attestation/attestation_flow.h"
#include "base/callback.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/attestation/certificate_util.h"
#include "chromeos/dbus/attestation/attestation_client.h"
#include "components/account_id/account_id.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace ash {
namespace attestation {

// Generates certificates asynchronously for soft key binding.
class SoftBindAttestationFlow {
 public:
  using Callback =
      base::OnceCallback<void(const std::vector<std::string>& cert)>;

  SoftBindAttestationFlow();
  ~SoftBindAttestationFlow();

  SoftBindAttestationFlow(const SoftBindAttestationFlow&) = delete;
  SoftBindAttestationFlow(SoftBindAttestationFlow&&) = delete;
  SoftBindAttestationFlow& operator=(const SoftBindAttestationFlow&) = delete;
  SoftBindAttestationFlow& operator=(SoftBindAttestationFlow&&) = delete;

  // !!! WARNING !!! This API should only be called by the browser itself.
  // Any new usage of this API should undergo security review.
  // Must be invoked on the UI thread due to AttestationClient requirements.
  // If the call times out before request completion, the request will
  // continue in the background so long as this object is not freed.
  void GetCertificate(Callback callback,
                      const AccountId& account_id,
                      const std::string& user_key);

  void SetAttestationFlowForTesting(
      std::unique_ptr<AttestationFlow> attestation_flow);

 private:
  // Encapsulates data necessary to construct the certificate chain. This data
  // holder is passed along through the entire callback flow, ultimately
  // returning the completed cert chain via its own callback.
  class Session {
   public:
    Session(Callback callback,
            AccountId account_id,
            const std::string& user_key);
    ~Session();
    bool IsTimerRunning() const;
    void StopTimer();
    // Returns false if the maximum number of certificate fetch retries (timer
    // resets) has been reached.
    bool ResetTimer();
    const AccountId& GetAccountId() const;
    const std::string& GetUserKey() const;
    void ReportFailure(const std::string& error_message);
    void ReportSuccess(const std::vector<std::string>& certificate_chain);

   private:
    void OnTimeout();

    Callback callback_;
    base::RepeatingTimer timer_;
    const AccountId account_id_;
    std::string user_key_;
    int max_retries_ = 3;
  };

  void GetCertificateInternal(bool force_new_key,
                              std::unique_ptr<Session> session);
  void OnCertificateReady(std::unique_ptr<Session> session,
                          AttestationStatus operation_status,
                          const std::string& certificate_chain);
  EVP_PKEY* GetLeafCertSpki(const std::string& certificate_chain);
  void OnCertificateSigned(std::unique_ptr<Session> session,
                           const std::string& tbs_cert,
                           const std::string& certificate_chain,
                           bool should_renew,
                           const ::attestation::SignReply& reply);
  bool IsAttestationAllowedByPolicy() const;
  CertificateExpiryStatus CheckExpiry(const std::string& certificate_chain);
  EVP_PKEY* GetLeafSubjectPublicKeyInfo(const std::string& certificate_chain);
  void RenewCertificateCallback(const std::string& old_certificate_chain,
                                AttestationStatus operation_status,
                                const std::string& certificate_chain);
  // Returns true if the cert was successfully generated, and false otherwise
  bool GenerateLeafCert(EVP_PKEY* key,
                        base::Time not_valid_before,
                        base::Time not_valid_after,
                        std::string* pem_encoded_cert);

  AttestationClient* const attestation_client_;
  std::unique_ptr<AttestationFlow> attestation_flow_;
  std::set<std::string> renewals_in_progress_;

  base::WeakPtrFactory<SoftBindAttestationFlow> weak_ptr_factory_{this};
};

}  // namespace attestation
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ATTESTATION_SOFT_BIND_ATTESTATION_FLOW_H_
