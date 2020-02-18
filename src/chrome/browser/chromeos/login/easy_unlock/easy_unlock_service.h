// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_SERVICE_H_

#include <memory>
#include <set>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/login/easy_unlock/chrome_proximity_auth_client.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_auth_attempt.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_metrics.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_screenlock_state_handler.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_types.h"
#include "chromeos/components/multidevice/remote_device_ref.h"
#include "chromeos/components/proximity_auth/screenlock_state.h"
#include "chromeos/components/proximity_auth/smart_lock_metrics_recorder.h"
#include "components/keyed_service/core/keyed_service.h"

class AccountId;

namespace base {
class ListValue;
}  // namespace base

namespace user_manager {
class User;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace proximity_auth {
class ProximityAuthPrefManager;
class ProximityAuthSystem;
}  // namespace proximity_auth

class Profile;
class PrefRegistrySimple;

namespace chromeos {

namespace secure_channel {
class SecureChannelClient;
}  // namespace secure_channel

class EasyUnlockService : public KeyedService {
 public:
  enum Type { TYPE_REGULAR, TYPE_SIGNIN };

  // Gets EasyUnlockService instance.
  static EasyUnlockService* Get(Profile* profile);

  // Gets EasyUnlockService instance associated with a user if the user is
  // logged in and their profile is initialized.
  static EasyUnlockService* GetForUser(const user_manager::User& user);

  // Registers Easy Unlock profile preferences.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Registers Easy Unlock local state entries.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Removes the hardlock state for the given user.
  static void ResetLocalStateForUser(const AccountId& account_id);

  // Returns the ProximityAuthPrefManager, responsible for managing all
  // EasyUnlock preferences.
  virtual proximity_auth::ProximityAuthPrefManager*
  GetProximityAuthPrefManager();

  // Returns the EasyUnlockService type.
  virtual Type GetType() const = 0;

  // Returns the user currently associated with the service.
  virtual AccountId GetAccountId() const = 0;

  // Retrieve the stored remote devices list:
  //   * If in regular context, device list is retrieved from prefs.
  //   * If in sign-in context, device list is retrieved from TPM.
  virtual const base::ListValue* GetRemoteDevices() const = 0;

  // Gets the challenge bytes for the user currently associated with the
  // service.
  virtual std::string GetChallenge() const = 0;

  // Retrieved wrapped secret that should be used to unlock cryptohome for the
  // user currently associated with the service. If the service does not support
  // signin (i.e. service for a regular profile) or there is no secret available
  // for the user, returns an empty string.
  virtual std::string GetWrappedSecret() const = 0;

  // Records metrics for Easy sign-in outcome for the given user.
  virtual void RecordEasySignInOutcome(const AccountId& account_id,
                                       bool success) const = 0;

  // Records metrics for password based flow for the given user.
  virtual void RecordPasswordLoginEvent(const AccountId& account_id) const = 0;

  // Sets the service up and schedules service initialization.
  void Initialize();

  // Whether easy unlock is allowed to be used. If the controlling preference
  // is set (from policy), this returns the preference value. Otherwise, it is
  // permitted if the flag is enabled. Virtual to allow override for testing.
  virtual bool IsAllowed() const;

  // Whether Easy Unlock is currently enabled for this user. Virtual to allow
  // override for testing.
  virtual bool IsEnabled() const;

  // Returns true if ChromeOS login is enabled by the user.
  virtual bool IsChromeOSLoginEnabled() const;

  // Sets the hardlock state for the associated user.
  void SetHardlockState(EasyUnlockScreenlockStateHandler::HardlockState state);

  // Returns the hardlock state for the associated user.
  EasyUnlockScreenlockStateHandler::HardlockState GetHardlockState() const;

  // Gets the persisted hardlock state. Return true if there is persisted
  // hardlock state and the value would be set to |state|. Otherwise,
  // returns false and |state| is unchanged.
  bool GetPersistedHardlockState(
      EasyUnlockScreenlockStateHandler::HardlockState* state) const;

  // Updates the user pod on the signin/lock screen for the user associated with
  // the service to reflect the provided screenlock state.
  bool UpdateScreenlockState(proximity_auth::ScreenlockState state);

  // Starts an auth attempt for the user associated with the service. The
  // attempt type (unlock vs. signin) will depend on the service type.
  void AttemptAuth(const AccountId& account_id);

  // Finalizes the previously started auth attempt for easy unlock. If called on
  // signin profile service, it will cancel the current auth attempt if one
  // exists.
  void FinalizeUnlock(bool success);

  // Finalizes previously started auth attempt for easy signin. If called on
  // regular profile service, it will cancel the current auth attempt if one
  // exists.
  void FinalizeSignin(const std::string& secret);

  // Handles Easy Unlock auth failure for the user.
  void HandleAuthFailure(const AccountId& account_id);

  // Checks the consistency between pairing data and cryptohome keys. Set
  // hardlock state if the two do not match.
  void CheckCryptohomeKeysAndMaybeHardlock();

  ChromeProximityAuthClient* proximity_auth_client() {
    return &proximity_auth_client_;
  }

 protected:
  EasyUnlockService(Profile* profile,
                    secure_channel::SecureChannelClient* secure_channel_client);
  ~EasyUnlockService() override;

  // Does a service type specific initialization.
  virtual void InitializeInternal() = 0;

  // Does a service type specific shutdown. Called from |Shutdown|.
  virtual void ShutdownInternal() = 0;

  // Service type specific tests for whether the service is allowed. Returns
  // false if service is not allowed. If true is returned, the service may still
  // not be allowed if common tests fail (e.g. if Bluetooth is not available).
  virtual bool IsAllowedInternal() const = 0;

  // Called when the local device resumes after a suspend.
  virtual void OnSuspendDoneInternal() = 0;

  // Called when the state of the Bluetooth adapter changes.
  virtual void OnBluetoothAdapterPresentChanged();

  // Called when the user enters password before easy unlock succeeds or fails
  // definitively.
  virtual void OnUserEnteredPassword();

  // KeyedService override:
  void Shutdown() override;

  // Exposes the profile to which the service is attached to subclasses.
  Profile* profile() const { return profile_; }

  // Checks whether Easy unlock should be running and updates app state.
  void UpdateAppState();

  // Resets the screenlock state set by this service.
  void ResetScreenlockState();

  // Updates |screenlock_state_handler_|'s hardlocked state.
  void SetScreenlockHardlockedState(
      EasyUnlockScreenlockStateHandler::HardlockState state);

  const EasyUnlockScreenlockStateHandler* screenlock_state_handler() const {
    return screenlock_state_handler_.get();
  }

  // Saves hardlock state for the given user. Update UI if the currently
  // associated user is the same.
  void SetHardlockStateForUser(
      const AccountId& account_id,
      EasyUnlockScreenlockStateHandler::HardlockState state);

  // Returns the authentication event for a recent password sign-in or unlock,
  // according to the current state of the service.
  EasyUnlockAuthEvent GetPasswordAuthEvent() const;

  // Returns the authentication event for a recent password sign-in or unlock,
  // according to the current state of the service.
  SmartLockMetricsRecorder::SmartLockAuthEventPasswordState
  GetSmartUnlockPasswordAuthEvent() const;

  // Called by subclasses when remote devices allowed to unlock the screen
  // are loaded for |account_id|.
  void SetProximityAuthDevices(
      const AccountId& account_id,
      const multidevice::RemoteDeviceRefList& remote_devices,
      base::Optional<multidevice::RemoteDeviceRef> local_device);

  bool will_authenticate_using_easy_unlock() const {
    return will_authenticate_using_easy_unlock_;
  }

  void set_will_authenticate_using_easy_unlock(
      bool will_authenticate_using_easy_unlock) {
    will_authenticate_using_easy_unlock_ = will_authenticate_using_easy_unlock;
  }

 private:
  // True if the user just authenticated using Easy Unlock. Reset once
  // the screen signs in/unlocks. Used to distinguish Easy Unlock-powered
  // signins/unlocks from password-based unlocks for metrics.
  bool will_authenticate_using_easy_unlock_ = false;

  // A class to detect whether a bluetooth adapter is present.
  class BluetoothDetector;

  // Gets |screenlock_state_handler_|. Returns NULL if Easy Unlock is not
  // allowed. Otherwise, if |screenlock_state_handler_| is not set, an instance
  // is created. Do not cache the returned value, as it may go away if Easy
  // Unlock gets disabled.
  EasyUnlockScreenlockStateHandler* GetScreenlockStateHandler();

  // Callback for get key operation from CheckCryptohomeKeysAndMaybeHardlock.
  void OnCryptohomeKeysFetchedForChecking(
      const AccountId& account_id,
      const std::set<std::string> paired_devices,
      bool success,
      const EasyUnlockDeviceKeyDataList& key_data_list);

  // Updates the service to state for handling system suspend.
  void PrepareForSuspend();

  // Called when the system resumes from a suspended state.
  void OnSuspendDone();

  void EnsureTpmKeyPresentIfNeeded();

  Profile* const profile_;
  secure_channel::SecureChannelClient* secure_channel_client_;

  ChromeProximityAuthClient proximity_auth_client_;

  // Created lazily in |GetScreenlockStateHandler|.
  std::unique_ptr<EasyUnlockScreenlockStateHandler> screenlock_state_handler_;

  // The handler for the current auth attempt. Set iff an auth attempt is in
  // progress.
  std::unique_ptr<EasyUnlockAuthAttempt> auth_attempt_;

  // Detects when the system Bluetooth adapter status changes.
  std::unique_ptr<BluetoothDetector> bluetooth_detector_;

  // Handles connecting, authenticating, and updating the UI on the lock/sign-in
  // screen. After a |RemoteDeviceRef| instance is provided, this object will
  // handle the rest.
  std::unique_ptr<proximity_auth::ProximityAuthSystem> proximity_auth_system_;

  // Monitors suspend and wake state of ChromeOS.
  class PowerMonitor;
  std::unique_ptr<PowerMonitor> power_monitor_;

  // Whether the service has been shut down.
  bool shut_down_;

  bool tpm_key_checked_;

  base::WeakPtrFactory<EasyUnlockService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_SERVICE_H_
