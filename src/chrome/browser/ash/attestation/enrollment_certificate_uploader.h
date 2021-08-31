// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ATTESTATION_ENROLLMENT_CERTIFICATE_UPLOADER_H_
#define CHROME_BROWSER_ASH_ATTESTATION_ENROLLMENT_CERTIFICATE_UPLOADER_H_

#include "base/callback.h"

namespace ash {
namespace attestation {

// An abstract class for enrollment certificate uploaders.
class EnrollmentCertificateUploader {
 public:
  // Execution status reported by |ObtainAndUploadCertificate|.
  enum class Status {
    // Enrollment certificate is fetched and uploaded.
    kSuccess,
    // Cannot fetch enrollment certificate.
    kFailedToFetch,
    // Cannot upload fetched enrollment certificate.
    kFailedToUpload
  };

  using UploadCallback = base::OnceCallback<void(Status status)>;

  virtual ~EnrollmentCertificateUploader() = default;

  // Obtains a fresh enrollment certificate and uploads it.
  virtual void ObtainAndUploadCertificate(UploadCallback callback) = 0;
};

}  // namespace attestation
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ATTESTATION_ENROLLMENT_CERTIFICATE_UPLOADER_H_
