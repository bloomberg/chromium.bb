// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_VERIFIER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_VERIFIER_H_

#include "base/macros.h"
#include "chromeos/services/multidevice_setup/host_verifier.h"

namespace chromeos {

namespace multidevice_setup {

// Test HostVerifier implementation.
class FakeHostVerifier : public HostVerifier {
 public:
  FakeHostVerifier();
  ~FakeHostVerifier() override;

  void set_is_host_verified(bool is_host_verified) {
    is_host_verified_ = is_host_verified;
  }

  size_t num_verify_now_attempts() { return num_verify_now_attempts_; }

  using HostVerifier::NotifyHostVerified;

 private:
  // HostVerifier:
  bool IsHostVerified() override;
  void PerformAttemptVerificationNow() override;

  bool is_host_verified_ = false;
  size_t num_verify_now_attempts_ = 0u;

  DISALLOW_COPY_AND_ASSIGN(FakeHostVerifier);
};

// Test HostVerifier::Observer implementation.
class FakeHostVerifierObserver : public HostVerifier::Observer {
 public:
  FakeHostVerifierObserver();
  ~FakeHostVerifierObserver() override;

  size_t num_host_verifications() { return num_host_verifications_; }

 private:
  // HostVerifier::Observer:
  void OnHostVerified() override;

  size_t num_host_verifications_ = 0u;

  DISALLOW_COPY_AND_ASSIGN(FakeHostVerifierObserver);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_VERIFIER_H_
