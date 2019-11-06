// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trial_comparison_cert_verifier_mojo.h"

#include <utility>

#include "net/cert/cert_verify_proc.h"
#include "net/cert/trial_comparison_cert_verifier.h"

namespace network {

TrialComparisonCertVerifierMojo::TrialComparisonCertVerifierMojo(
    bool initial_allowed,
    mojom::TrialComparisonCertVerifierConfigClientRequest config_client_request,
    mojom::TrialComparisonCertVerifierReportClientPtrInfo report_client,
    scoped_refptr<net::CertVerifyProc> primary_verify_proc,
    scoped_refptr<net::CertVerifyProc> trial_verify_proc)
    : binding_(this, std::move(config_client_request)),
      report_client_(std::move(report_client)) {
  trial_comparison_cert_verifier_ =
      std::make_unique<net::TrialComparisonCertVerifier>(
          initial_allowed, primary_verify_proc, trial_verify_proc,
          base::BindRepeating(
              &mojom::TrialComparisonCertVerifierReportClient::SendTrialReport,
              // Unretained safe because the report_callback will not be called
              // after trial_comparison_cert_verifier_ is destroyed.
              base::Unretained(report_client_.get())));
}

TrialComparisonCertVerifierMojo::~TrialComparisonCertVerifierMojo() = default;

int TrialComparisonCertVerifierMojo::Verify(
    const RequestParams& params,
    net::CertVerifyResult* verify_result,
    net::CompletionOnceCallback callback,
    std::unique_ptr<Request>* out_req,
    const net::NetLogWithSource& net_log) {
  return trial_comparison_cert_verifier_->Verify(
      params, verify_result, std::move(callback), out_req, net_log);
}

void TrialComparisonCertVerifierMojo::SetConfig(const Config& config) {
  trial_comparison_cert_verifier_->SetConfig(config);
}

void TrialComparisonCertVerifierMojo::OnTrialConfigUpdated(bool allowed) {
  trial_comparison_cert_verifier_->set_trial_allowed(allowed);
}

}  // namespace network
