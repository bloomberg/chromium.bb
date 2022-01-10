// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_CELLULAR_ESIM_PROFILE_HANDLER_IMPL_H_
#define CHROMEOS_NETWORK_CELLULAR_ESIM_PROFILE_HANDLER_IMPL_H_

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/network/cellular_esim_profile_handler.h"
#include "chromeos/network/network_state_handler_observer.h"

class PrefService;
class PrefRegistrySimple;

namespace chromeos {

namespace network_ui {

class NetworkConfigMessageHandler;

}  // namespace network_ui

// CellularESimProfileHandler implementation which utilizes the local state
// PrefService to track eSIM profiles.
//
// eSIM profiles can only be retrieved from the device hardware when an EUICC is
// the "active" slot on the device, and only one slot can be active at a time.
// This means that if the physical SIM slot is active, we cannot fetch an
// updated list of profiles without switching slots, which can be disruptive if
// the user is utilizing a cellular connection from the physical SIM slot. To
// ensure that clients can access eSIM metadata regardless of the active slot,
// this class stores all known eSIM profiles persistently in prefs.
//
// Additionally, this class tracks all known EUICC paths. If it detects a new
// EUICC which it previously had not known about, it automatically refreshes
// profile metadata from that slot. This ensures that after a powerwash, we
// still expose information about installed profiles.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularESimProfileHandlerImpl
    : public CellularESimProfileHandler,
      public NetworkStateHandlerObserver {
 public:
  CellularESimProfileHandlerImpl();
  CellularESimProfileHandlerImpl(const CellularESimProfileHandlerImpl&) =
      delete;
  CellularESimProfileHandlerImpl& operator=(
      const CellularESimProfileHandlerImpl&) = delete;
  ~CellularESimProfileHandlerImpl() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // NetworkStateHandlerObserver:
  void DeviceListChanged() override;

 private:
  friend class CellularESimProfileHandlerImplTest;
  friend class network_ui::NetworkConfigMessageHandler;

  // CellularESimProfileHandler:
  void InitInternal() override;
  std::vector<CellularESimProfile> GetESimProfiles() override;
  bool HasRefreshedProfilesForEuicc(const std::string& eid) override;
  void SetDevicePrefs(PrefService* device_prefs) override;
  void OnHermesPropertiesUpdated() override;

  void AutoRefreshEuiccsIfNecessary();
  void StartAutoRefresh(const base::flat_set<std::string>& euicc_paths);
  base::flat_set<std::string> GetAutoRefreshedEuiccPaths() const;
  base::flat_set<std::string> GetAutoRefreshedEuiccPathsFromPrefs() const;
  void OnAutoRefreshEuiccComplete(
      const std::string& path,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);
  void AddNewlyRefreshedEuiccPathToPrefs(const std::string& path);

  void UpdateProfilesFromHermes();

  bool CellularDeviceExists() const;

  // Used by chrome://network debug page; not meant to be called during normal
  // usage.
  void ResetESimProfileCache();

  // Initialized to null and set once SetDevicePrefs() is called.
  PrefService* device_prefs_ = nullptr;

  base::flat_set<std::string> paths_pending_auto_refresh_;

  base::WeakPtrFactory<CellularESimProfileHandlerImpl> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_CELLULAR_ESIM_PROFILE_HANDLER_IMPL_H_
