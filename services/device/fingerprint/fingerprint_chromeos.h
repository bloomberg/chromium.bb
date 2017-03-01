// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_FINGERPRINT_FINGERPRINT_CHROMEOS_H_
#define SERVICES_DEVICE_FINGERPRINT_FINGERPRINT_CHROMEOS_H_

#include <stdint.h>

#include "base/macros.h"
#include "services/device/fingerprint/fingerprint_export.h"
#include "services/device/public/interfaces/fingerprint.mojom.h"

namespace dbus {
class ObjectPath;
}

namespace device {

// Implementation of Fingerprint interface for ChromeOS platform.
// This is used to connect to biod(through dbus) and perform fingerprint related
// operations. It observes signals from biod.
class SERVICES_DEVICE_FINGERPRINT_EXPORT FingerprintChromeOS
    : public NON_EXPORTED_BASE(mojom::Fingerprint) {
 public:
  explicit FingerprintChromeOS();
  ~FingerprintChromeOS() override;

  // mojom::Fingerprint:
  // TODO(xiaoyinh@): Update the function once biod change has landed.
  void GetFingerprintsList(
      const GetFingerprintsListCallback& callback) override;
  void StartEnroll(const std::string& user_id,
                   const std::string& label) override;
  void CancelCurrentEnroll() override;
  void GetLabel(int32_t index, const GetLabelCallback& callback) override;
  void SetLabel(const std::string& label, int32_t index) override;
  void RemoveEnrollment(int32_t index) override;
  void StartAuthentication() override;
  void EndCurrentAuthentication() override;
  void DestroyAllEnrollments() override;
  void AddFingerprintObserver(mojom::FingerprintObserverPtr observer) override;

 private:
  friend class FingerprintChromeOSTest;

  void BiodBiometricClientRestarted();
  void BiometricsScanEventReceived(uint32_t scan_result, bool is_complete);
  // TODO(xiaoyinh@): Update the function once biod change has landed.
  void BiometricsAttemptEventReceived(
      uint32_t scan_result,
      const std::vector<std::string>& recognized_user_ids);
  void BiometricsFailureReceived();

  void OnFingerprintObserverDisconnected(mojom::FingerprintObserver* observer);
  void OnStartEnroll(const dbus::ObjectPath& enroll_path);
  void OnStartAuthentication(const dbus::ObjectPath& auth_path);
  void OnGetFingerprintsList(
      const GetFingerprintsListCallback& callback,
      const std::vector<dbus::ObjectPath>& enrollment_paths);
  void OnGetLabelFromEnrollmentPath(const GetFingerprintsListCallback& callback,
                                    size_t num_enrollments,
                                    std::vector<std::string>* out_labels,
                                    const std::string& label);

  void OnGetLabel(int index,
                  const GetLabelCallback& callback,
                  const std::vector<dbus::ObjectPath>& enrollment_paths);
  void OnSetLabel(const std::string& new_label,
                  int index,
                  const std::vector<dbus::ObjectPath>& enrollment_paths);
  void OnRemoveEnrollment(
      int index,
      const std::vector<dbus::ObjectPath>& enrollment_paths);

  std::vector<mojom::FingerprintObserverPtr> observers_;
  std::unique_ptr<dbus::ObjectPath> current_enroll_path_;
  std::unique_ptr<dbus::ObjectPath> current_auth_path_;

  base::WeakPtrFactory<FingerprintChromeOS> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FingerprintChromeOS);
};

}  // namespace device

#endif  // SERVICES_DEVICE_FINGERPRINT_FINGERPRINT_CHROMEOS_H_
