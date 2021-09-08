// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_ARC_SHELF_SPINNER_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_ARC_SHELF_SPINNER_ITEM_CONTROLLER_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "chrome/browser/chromeos/full_restore/arc_window_handler.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_item_controller.h"
#include "components/arc/mojom/app.mojom.h"

// ArcShelfSpinnerItemController displays the icon of the ARC app that
// cannot be launched immediately (due to ARC not being ready) on Chrome OS'
// shelf, with an overlaid spinner to provide visual feedback.
class ArcShelfSpinnerItemController
    : public ShelfSpinnerItemController,
      public ArcAppListPrefs::Observer,
      public arc::ArcSessionManagerObserver,
      public chromeos::full_restore::ArcWindowHandler::Observer {
 public:
  ArcShelfSpinnerItemController(const std::string& arc_app_id,
                                int event_flags,
                                arc::UserInteractionType user_interaction_type,
                                arc::mojom::WindowInfoPtr window_info);

  ~ArcShelfSpinnerItemController() override;

  // ShelfSpinnerItemController:
  void SetHost(const base::WeakPtr<ShelfSpinnerController>& host) override;

  // ArcAppListPrefs::Observer:
  void OnAppStatesChanged(const std::string& app_id,
                          const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppRemoved(const std::string& removed_app_id) override;

  // arc::ArcSessionManagerObserver:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;

  // chromeos::full_restore::ArcWindowHandler::Observer:
  void OnWindowCloseRequested(int window_id) override;

 private:
  // The flags of the event that caused the ARC app to be activated. These will
  // be propagated to the launch event once the app is actually launched.
  const int event_flags_;

  // Stores how this action was initiated.
  const arc::UserInteractionType user_interaction_type_;

  arc::mojom::WindowInfoPtr window_info_;

  // Unowned
  Profile* observed_profile_ = nullptr;

  base::ScopedObservation<chromeos::full_restore::ArcWindowHandler,
                          chromeos::full_restore::ArcWindowHandler::Observer>
      arc_handler_observation_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcShelfSpinnerItemController);
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_ARC_SHELF_SPINNER_ITEM_CONTROLLER_H_
