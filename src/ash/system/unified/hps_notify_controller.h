// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_HPS_NOTIFY_CONTROLLER_H_
#define ASH_SYSTEM_UNIFIED_HPS_NOTIFY_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "chromeos/dbus/hps/hps_dbus_client.h"
#include "components/session_manager/session_manager_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;

namespace ash {

// Pushes visibility changes to the snooping protection icon based on DBus
// state, preferences and session type.
class ASH_EXPORT HpsNotifyController
    : public SessionObserver,
      public chromeos::HpsDBusClient::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when an observing icon should show or hide itself because the
    // snooping status has changed. Argument is true if an icon should now be
    // visible.
    virtual void ShouldUpdateVisibility(bool visible) = 0;
  };

  HpsNotifyController();
  HpsNotifyController(const HpsNotifyController&) = delete;
  HpsNotifyController& operator=(const HpsNotifyController&) = delete;
  ~HpsNotifyController() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // chromeos::HpsDBusClient::Observer:
  void OnHpsNotifyChanged(bool state) override;
  void OnRestart() override;
  void OnShutdown() override;

  // Adds/removes views that are listening for visibility updates.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // The current visibility state. Polled by views on construction.
  bool IsIconVisible() const;

 private:
  // Updates visibility state as appropriate given the signal, session and
  // preference state. If changed, notifies observers.
  void UpdateIconVisibility(bool logged_in, bool hps_state, bool is_enabled);

  // Configures the daemon and opts in to signals from it.
  void StartHpsObservation(bool service_is_available);

  // Re-configures the daemon and polls its initial state (used in case of
  // daemon restart).
  void RestartHpsObservation();

  // Performs the state update from the daemon response.
  void UpdateHpsState(absl::optional<bool> result);

  // A callback to update visibility when the user enables or disables the
  // feature.
  void UpdatePrefState();

  // Used to track whether a signal should actually trigger a visibility change:
  bool hps_state_ = false;       // The state last reported by the daemon.
  bool session_active_ = false;  // Whether or not there is an active user
                                 // session ongoing.
  bool is_enabled_ = false;      // Whether or not the user has enabled the
                                 // feature via preferences.

  base::ScopedObservation<SessionController, SessionObserver>
      session_observation_{this};
  base::ScopedObservation<chromeos::HpsDBusClient,
                          chromeos::HpsDBusClient::Observer>
      hps_dbus_observation_{this};

  // Used to notify ourselves of changes to the pref that enables / disables
  // this feature.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // The views that are listening for visibililty instructions.
  base::ObserverList<Observer> observers_;

  // Must be last.
  base::WeakPtrFactory<HpsNotifyController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_HPS_NOTIFY_CONTROLLER_H_
