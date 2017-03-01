// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/fingerprint/fingerprint_chromeos.h"

#include <string.h>

#include "dbus/object_path.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/device/fingerprint/fingerprint.h"

namespace device {

FingerprintChromeOS::FingerprintChromeOS() : weak_ptr_factory_(this) {}

FingerprintChromeOS::~FingerprintChromeOS() {}

void FingerprintChromeOS::GetFingerprintsList(
    const GetFingerprintsListCallback& callback) {}

void FingerprintChromeOS::StartEnroll(const std::string& user_id,
                                      const std::string& label) {}

void FingerprintChromeOS::CancelCurrentEnroll() {
  // Call Dbus to canel current enroll and reset the current_enroll_path.
  current_enroll_path_.reset();
}

void FingerprintChromeOS::GetLabel(int32_t index,
                                   const GetLabelCallback& callback) {}

void FingerprintChromeOS::SetLabel(const std::string& label, int32_t index) {}

void FingerprintChromeOS::RemoveEnrollment(int32_t index) {}

void FingerprintChromeOS::StartAuthentication() {}

void FingerprintChromeOS::EndCurrentAuthentication() {
  // Call Dbus to canel current authentication and reset the current_auth_path.
  current_auth_path_.reset();
}

void FingerprintChromeOS::DestroyAllEnrollments() {}

void FingerprintChromeOS::AddFingerprintObserver(
    mojom::FingerprintObserverPtr observer) {
  observer.set_connection_error_handler(
      base::Bind(&FingerprintChromeOS::OnFingerprintObserverDisconnected,
                 base::Unretained(this), observer.get()));
  observers_.push_back(std::move(observer));
}

void FingerprintChromeOS::BiodBiometricClientRestarted() {
  current_enroll_path_.reset();
  current_auth_path_.reset();
  for (auto& observer : observers_)
    observer->OnRestarted();
}

void FingerprintChromeOS::BiometricsScanEventReceived(uint32_t scan_result,
                                                      bool is_complete) {
  if (is_complete)
    current_enroll_path_.reset();

  for (auto& observer : observers_)
    observer->OnScanned(scan_result, is_complete);
}

void FingerprintChromeOS::BiometricsAttemptEventReceived(
    uint32_t scan_result,
    const std::vector<std::string>& recognized_user_ids) {
  for (auto& observer : observers_)
    observer->OnAttempt(scan_result, recognized_user_ids);
}

void FingerprintChromeOS::BiometricsFailureReceived() {
  for (auto& observer : observers_)
    observer->OnFailure();
}

void FingerprintChromeOS::OnFingerprintObserverDisconnected(
    mojom::FingerprintObserver* observer) {
  for (auto item = observers_.begin(); item != observers_.end(); ++item) {
    if (item->get() == observer) {
      observers_.erase(item);
      break;
    }
  }
}

void FingerprintChromeOS::OnStartEnroll(const dbus::ObjectPath& enroll_path) {
  DCHECK(!current_enroll_path_);
  current_enroll_path_ = base::MakeUnique<dbus::ObjectPath>(enroll_path);
}

void FingerprintChromeOS::OnStartAuthentication(
    const dbus::ObjectPath& auth_path) {
  DCHECK(!current_auth_path_);
  current_auth_path_ = base::MakeUnique<dbus::ObjectPath>(auth_path);
}

void FingerprintChromeOS::OnGetFingerprintsList(
    const GetFingerprintsListCallback& callback,
    const std::vector<dbus::ObjectPath>& enrollment_paths) {}

void FingerprintChromeOS::OnGetLabel(
    int index,
    const GetLabelCallback& callback,
    const std::vector<dbus::ObjectPath>& enrollment_paths) {
  DCHECK(index >= 0 && index < int{enrollment_paths.size()});
}

void FingerprintChromeOS::OnSetLabel(
    const std::string& new_label,
    int index,
    const std::vector<dbus::ObjectPath>& enrollment_paths) {
  DCHECK(index >= 0 && index < int{enrollment_paths.size()});
}

void FingerprintChromeOS::OnRemoveEnrollment(
    int index,
    const std::vector<dbus::ObjectPath>& enrollment_paths) {
  DCHECK(index >= 0 && index < int{enrollment_paths.size()});
}

// static
void Fingerprint::Create(device::mojom::FingerprintRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<FingerprintChromeOS>(),
                          std::move(request));
}

}  // namespace device
