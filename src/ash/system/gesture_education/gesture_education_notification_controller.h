// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_GESTURE_EDUCATION_GESTURE_EDUCATION_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_GESTURE_EDUCATION_GESTURE_EDUCATION_NOTIFICATION_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/session/session_observer.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"

class PrefRegistrySimple;

namespace ash {

// Controller class to manage gesture education notification. This notification
// shows up once to provide the user with information about new gestures added
// to chrome os for easier navigation.
class ASH_EXPORT GestureEducationNotificationController
    : public SessionObserver,
      public TabletModeObserver {
 public:
  GestureEducationNotificationController();
  ~GestureEducationNotificationController() override;

  // Shows the gesture education notification if it needs to be shown.
  void MaybeShowNotification();

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletControllerDestroyed() override;

  // See Shell::RegisterProfilePrefs().
  static void RegisterProfilePrefs(PrefRegistrySimple* registry, bool for_test);

 private:
  friend class GestureEducationNotificationControllerTest;

  void GenerateGestureEducationNotification();
  base::string16 GetNotificationTitle() const;
  base::string16 GetNotificationMessage() const;
  void HandleNotificationClick();

  void ResetPrefForTest();

  ScopedObserver<TabletModeController, TabletModeObserver>
      tablet_mode_observer_{this};

  PrefService* active_user_prefs_ = nullptr;  // Not owned.

  static const char kNotificationId[];

  base::WeakPtrFactory<GestureEducationNotificationController>
      weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_GESTURE_EDUCATION_GESTURE_EDUCATION_NOTIFICATION_CONTROLLER_H_
