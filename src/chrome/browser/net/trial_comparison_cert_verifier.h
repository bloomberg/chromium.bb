// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_TRIAL_COMPARISON_CERT_VERIFIER_H_
#define CHROME_BROWSER_NET_TRIAL_COMPARISON_CERT_VERIFIER_H_

#include <stdint.h>

#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "net/base/net_export.h"
#include "net/cert/cert_verifier.h"

namespace net {
class CertVerifyProc;
}

class TrialComparisonCertVerifier : public net::CertVerifier {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum TrialComparisonResult {
    kInvalid = 0,
    kEqual = 1,
    kPrimaryValidSecondaryError = 2,
    kPrimaryErrorSecondaryValid = 3,
    kBothValidDifferentDetails = 4,
    kBothErrorDifferentDetails = 5,
    kIgnoredMacUndesiredRevocationChecking = 6,
    kIgnoredMultipleEVPoliciesAndOneMatchesRoot = 7,
    kIgnoredDifferentPathReVerifiesEquivalent = 8,
    kIgnoredLocallyTrustedLeaf = 9,
    kIgnoredConfigurationChanged = 10,
    kMaxValue = kIgnoredConfigurationChanged
  };

  TrialComparisonCertVerifier(
      void* profile_id,
      scoped_refptr<net::CertVerifyProc> primary_verify_proc,
      scoped_refptr<net::CertVerifyProc> trial_verify_proc);

  ~TrialComparisonCertVerifier() override;

  // This method can be called by tests to fake an official build (reports are
  // only sent from official builds).
  static void SetFakeOfficialBuildForTesting();

  // CertVerifier implementation
  int Verify(const RequestParams& params,
             net::CertVerifyResult* verify_result,
             net::CompletionOnceCallback callback,
             std::unique_ptr<Request>* out_req,
             const net::NetLogWithSource& net_log) override;
  void SetConfig(const Config& config) override;

  // Returns a CertVerifier using the primary CertVerifyProc, which will not
  // cause OnPrimaryVerifierComplete to be called. This can be used to
  // attempt to re-verify a cert with different chain or flags without
  // messing up the stats or potentially causing an infinite loop.
  net::CertVerifier* primary_reverifier() const {
    return primary_reverifier_.get();
  }
  net::CertVerifier* trial_verifier() const { return trial_verifier_.get(); }
  net::CertVerifier* revocation_trial_verifier() const {
    return revocation_trial_verifier_.get();
  }

 private:
  class TrialVerificationJob;

  void OnPrimaryVerifierComplete(const RequestParams& params,
                                 const net::NetLogWithSource& net_log,
                                 int primary_error,
                                 const net::CertVerifyResult& primary_result,
                                 base::TimeDelta primary_latency,
                                 bool is_first_job);
  void OnTrialVerifierComplete(const RequestParams& params,
                               const net::NetLogWithSource& net_log,
                               int trial_error,
                               const net::CertVerifyResult& trial_result,
                               base::TimeDelta latency,
                               bool is_first_job);
  void MaybeDoTrialVerification(const RequestParams& params,
                                const net::NetLogWithSource& net_log,
                                int primary_error,
                                const net::CertVerifyResult& primary_result,
                                base::TimeDelta primary_latency,
                                bool is_first_job,
                                uint32_t config_id,
                                void* profile_id,
                                bool trial_allowed);

  void RemoveJob(TrialVerificationJob* job_ptr);

  // The profile this verifier is associated with. Stored as a void* to avoid
  // accidentally using it on IO thread.
  void* profile_id_;

  // Unique identifier for the current configuration, to determine if a
  // configuration has changed in between primary and trial verifications.
  uint32_t config_id_;
  net::CertVerifier::Config config_;

  std::unique_ptr<net::CertVerifier> primary_verifier_;
  std::unique_ptr<net::CertVerifier> primary_reverifier_;
  std::unique_ptr<net::CertVerifier> trial_verifier_;
  // Similar to |trial_verifier_|, except configured to always check
  // revocation information.
  std::unique_ptr<net::CertVerifier> revocation_trial_verifier_;

  std::set<std::unique_ptr<TrialVerificationJob>, base::UniquePtrComparator>
      jobs_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<TrialComparisonCertVerifier> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TrialComparisonCertVerifier);
};

#endif  // CHROME_BROWSER_NET_TRIAL_COMPARISON_CERT_VERIFIER_H_
