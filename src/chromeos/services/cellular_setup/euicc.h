// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CELLULAR_SETUP_EUICC_H_
#define CHROMEOS_SERVICES_CELLULAR_SETUP_EUICC_H_

#include "base/gtest_prod_util.h"
#include "chromeos/dbus/hermes/hermes_euicc_client.h"
#include "chromeos/dbus/hermes/hermes_profile_client.h"
#include "chromeos/network/cellular_inhibitor.h"
#include "chromeos/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace dbus {
class ObjectPath;
}  // namespace dbus

namespace chromeos {

class CellularESimProfile;

namespace cellular_setup {

class ESimProfile;
class ESimManager;

// Implementation of mojom::Euicc. This class represents an EUICC hardware
// available on the device. Euicc holds multiple ESimProfile instances.
class Euicc : public mojom::Euicc {
 public:
  Euicc(const dbus::ObjectPath& path, ESimManager* esim_manager);
  Euicc(const Euicc&) = delete;
  Euicc& operator=(const Euicc&) = delete;
  ~Euicc() override;

  // mojom::Euicc:
  void GetProperties(GetPropertiesCallback callback) override;
  void GetProfileList(GetProfileListCallback callback) override;
  void InstallProfileFromActivationCode(
      const std::string& activation_code,
      const std::string& confirmation_code,
      InstallProfileFromActivationCodeCallback callback) override;
  void RequestPendingProfiles(RequestPendingProfilesCallback callback) override;
  void GetEidQRCode(GetEidQRCodeCallback callback) override;

  // Updates list of eSIM profiles for this euicc from with the given
  // |esim_profile_states|.
  void UpdateProfileList(
      const std::vector<CellularESimProfile>& esim_profile_states);

  // Updates properties for this Euicc from D-Bus.
  void UpdateProperties();

  // Returns a new pending remote attached to this instance.
  mojo::PendingRemote<mojom::Euicc> CreateRemote();

  // Returns ESimProfile instance under this Euicc with given path.
  ESimProfile* GetProfileFromPath(const dbus::ObjectPath& path);

  const dbus::ObjectPath& path() { return path_; }
  const mojom::EuiccPropertiesPtr& properties() { return properties_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(EuiccTest, RequestPendingProfiles);

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class RequestPendingProfilesResult {
    kSuccess = 0,
    kInhibitFailed = 1,
    kHermesRequestFailed = 2,
    kMaxValue = kHermesRequestFailed
  };
  static void RecordRequestPendingProfilesResult(
      RequestPendingProfilesResult result);

  // Type of callback for profile installation methods.
  using ProfileInstallResultCallback =
      base::OnceCallback<void(mojom::ProfileInstallResult)>;

  void PerformInstallProfileFromActivationCode(
      const std::string& activation_code,
      const std::string& confirmation_code,
      InstallProfileFromActivationCodeCallback callback,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);
  void OnESimInstallProfileResult(
      InstallProfileFromActivationCodeCallback callback,
      HermesResponseStatus hermes_status,
      absl::optional<dbus::ObjectPath> profile_path,
      absl::optional<std::string> service_path);
  void OnNewProfileConnectSuccess(const dbus::ObjectPath& profile_path);
  void PerformRequestPendingProfiles(
      RequestPendingProfilesCallback callback,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);
  void OnRequestPendingProfilesResult(
      RequestPendingProfilesCallback callback,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
      HermesResponseStatus status);
  mojom::ProfileInstallResult GetPendingProfileInfoFromActivationCode(
      const std::string& activation_code,
      ESimProfile** profile_info);
  // Updates an ESimProfile in |esim_profiles_| with values from given
  // |esim_profile_state| or creates new one if it doesn't exist. Returns
  // pointer to ESimProfile object if one was created.
  ESimProfile* UpdateOrCreateESimProfile(
      const CellularESimProfile& esim_profile_state);
  // Removes any ESimProfile object in |esim_profiles_| that doesn't exists in
  // given |esim_profile_states|. Returns true if any profiles were removed.
  bool RemoveUntrackedProfiles(
      const std::vector<CellularESimProfile>& esim_profile_states);

  // Reference to ESimManager that owns this Euicc.
  ESimManager* esim_manager_;
  mojo::ReceiverSet<mojom::Euicc> receiver_set_;
  mojom::EuiccPropertiesPtr properties_;
  dbus::ObjectPath path_;
  std::vector<std::unique_ptr<ESimProfile>> esim_profiles_;

  // Maps profile dbus paths to InstallProfileFromActivation method callbacks
  // that are pending creation of a new ESimProfile object.
  std::map<dbus::ObjectPath, InstallProfileFromActivationCodeCallback>
      install_calls_pending_create_;

  base::WeakPtrFactory<Euicc> weak_ptr_factory_{this};
};

}  // namespace cellular_setup
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_CELLULAR_SETUP_EUICC_H_
