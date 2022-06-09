// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/attestation/enrollment_certificate_uploader_impl.h"

#include <memory>
#include <utility>

#include "ash/components/attestation/attestation_flow.h"
#include "ash/components/attestation/attestation_flow_adaptive.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/browser/ash/attestation/attestation_ca_client.h"
#include "chrome/browser/ash/attestation/certificate_util.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {

// Constants for retrying certificate obtention and upload.
constexpr base::TimeDelta kRetryDelay = base::Seconds(5);
const int kRetryLimit = 100;

void DBusPrivacyCACallback(
    const base::RepeatingCallback<void(const std::string&)> on_success,
    const base::RepeatingCallback<
        void(chromeos::attestation::AttestationStatus)> on_failure,
    const base::Location& from_here,
    chromeos::attestation::AttestationStatus status,
    const std::string& data) {
  DCHECK(on_success);
  DCHECK(on_failure);

  if (status == chromeos::attestation::ATTESTATION_SUCCESS) {
    on_success.Run(data);
    return;
  }

  LOG(ERROR) << "Attestation DBus method or server called failed with status: "
             << status << ": " << from_here.ToString();
  on_failure.Run(status);
}

}  // namespace

namespace ash {
namespace attestation {

EnrollmentCertificateUploaderImpl::EnrollmentCertificateUploaderImpl(
    policy::CloudPolicyClient* policy_client)
    : policy_client_(policy_client),
      retry_limit_(kRetryLimit),
      retry_delay_(kRetryDelay) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

EnrollmentCertificateUploaderImpl::~EnrollmentCertificateUploaderImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void EnrollmentCertificateUploaderImpl::ObtainAndUploadCertificate(
    UploadCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool start = callbacks_.empty();
  callbacks_.push(std::move(callback));
  if (start) {
    num_retries_ = 0;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&EnrollmentCertificateUploaderImpl::Start,
                                  weak_factory_.GetWeakPtr()));
  }
}

void EnrollmentCertificateUploaderImpl::Start() {
  // We expect a registered CloudPolicyClient.
  if (!policy_client_->is_registered()) {
    LOG(ERROR) << "CloudPolicyClient not registered.";
    RunCallbacks(Status::kFailedToFetch);
    return;
  }

  if (!attestation_flow_) {
    std::unique_ptr<ServerProxy> attestation_ca_client(
        new AttestationCAClient());
    default_attestation_flow_ = std::make_unique<AttestationFlowAdaptive>(
        std::move(attestation_ca_client));
    attestation_flow_ = default_attestation_flow_.get();
  }

  GetCertificate(/*force_new_key=*/false);
}

void EnrollmentCertificateUploaderImpl::GetCertificate(bool force_new_key) {
  VLOG_IF(1, force_new_key) << "Fetching certificate with new key";
  attestation_flow_->GetCertificate(
      PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE,
      EmptyAccountId(),  // Not used.
      std::string(),     // Not used.
      force_new_key,
      std::string(),  // Leave key name empty to generate a default name.
      base::BindOnce(
          [](const base::RepeatingCallback<void(const std::string&)> on_success,
             const base::RepeatingCallback<void(AttestationStatus)> on_failure,
             const base::Location& from_here, AttestationStatus status,
             const std::string& data) {
            DBusPrivacyCACallback(on_success, on_failure, from_here, status,
                                  std::move(data));
          },
          base::BindRepeating(
              &EnrollmentCertificateUploaderImpl::CheckCertificateExpiry,
              weak_factory_.GetWeakPtr()),
          base::BindRepeating(
              &EnrollmentCertificateUploaderImpl::HandleGetCertificateFailure,
              weak_factory_.GetWeakPtr()),
          FROM_HERE));
}

void EnrollmentCertificateUploaderImpl::HandleGetCertificateFailure(
    AttestationStatus status) {
  if (status == ATTESTATION_SERVER_BAD_REQUEST_FAILURE) {
    LOG(ERROR)
        << "Failed to fetch Enterprise Enrollment Certificate: bad request.";
    RunCallbacks(Status::kFailedToFetch);
    return;
  }

  LOG(WARNING) << "Failed to fetch Enterprise Enrollment Certificate.";

  if (!Reschedule()) {
    RunCallbacks(Status::kFailedToFetch);
  }
}

void EnrollmentCertificateUploaderImpl::CheckCertificateExpiry(
    const std::string& pem_certificate_chain) {
  // Expiry threshold is 0 so no expiring soon certificates. The reason is that
  // enrollmen certificates expire in 1 day so there's no anyhow optimal
  // threshold to catch expiring certificates. Worst case scenario: upload
  // expring certificate on start-up and re-upload new one on demand.
  const CertificateExpiryStatus cert_status =
      ::ash::attestation::CheckCertificateExpiry(
          pem_certificate_chain,
          /*expiry_threshold=*/base::TimeDelta());
  switch (cert_status) {
    case CertificateExpiryStatus::kExpiringSoon:
    case CertificateExpiryStatus::kExpired:
      LOG(WARNING) << "Existing certificate has expired.";
      has_already_uploaded_ = false;
      GetCertificate(/*force_new_key=*/true);
      return;
    case CertificateExpiryStatus::kValid:
    case CertificateExpiryStatus::kInvalidPemChain:
    case CertificateExpiryStatus::kInvalidX509:
      // kInvalidPemChain and kInvalidX509 are not handled intentionally.
      // Renewal is expensive so we only renew certificates with good evidence
      // that they have expired or will soon expire; if we don't know, we don't
      // renew.
      LOG_IF(ERROR, cert_status != CertificateExpiryStatus::kValid)
          << "Failed to parse certificate, cannot check expiry: "
          << CertificateExpiryStatusToString(cert_status);
      UploadCertificateIfNeeded(pem_certificate_chain);
      return;
  }

  NOTREACHED() << "Unknown certificate status";
}

void EnrollmentCertificateUploaderImpl::UploadCertificateIfNeeded(
    const std::string& pem_certificate_chain) {
  if (has_already_uploaded_) {
    RunCallbacks(Status::kSuccess);
    return;
  }

  policy_client_->UploadEnterpriseEnrollmentCertificate(
      pem_certificate_chain,
      base::BindOnce(&EnrollmentCertificateUploaderImpl::OnUploadComplete,
                     weak_factory_.GetWeakPtr()));
}

void EnrollmentCertificateUploaderImpl::OnUploadComplete(bool status) {
  if (status) {
    has_already_uploaded_ = true;
    if (num_retries_ != 0) {
      LOG(WARNING) << "Enterprise Enrollment Certificate uploaded to DMServer "
                      "after retries: "
                   << num_retries_;
    } else {
      VLOG(1) << "Enterprise Enrollment Certificate uploaded to DMServer.";
    }
    RunCallbacks(Status::kSuccess);
    return;
  }

  LOG(WARNING)
      << "Failed to upload Enterprise Enrollment Certificate to DMServer.";

  if (!Reschedule()) {
    RunCallbacks(Status::kFailedToUpload);
  }
}

bool EnrollmentCertificateUploaderImpl::Reschedule() {
  if (num_retries_ >= retry_limit_) {
    LOG(ERROR) << "Retry limit exceeded to fetch enrollment certificate.";
    return false;
  }

  ++num_retries_;

  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&EnrollmentCertificateUploaderImpl::Start,
                     weak_factory_.GetWeakPtr()),
      retry_delay_);
  return true;
}

void EnrollmentCertificateUploaderImpl::RunCallbacks(Status status) {
  std::queue<UploadCallback> callbacks;
  callbacks.swap(callbacks_);
  for (; !callbacks.empty(); callbacks.pop())
    std::move(callbacks.front()).Run(status);
}

}  // namespace attestation
}  // namespace ash
