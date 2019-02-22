// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DISPLAY_ROTATION_DEFAULT_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DISPLAY_ROTATION_DEFAULT_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ash/public/interfaces/cros_display_config.mojom.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "ui/display/display.h"

namespace policy {

// Enforces the device policy DisplayRotationDefault.
// This class must be constructed after CrosSettings is initialized.
// On construction this class registers itself with
// ash::mojom::CrosDisplayConfigObserver for display changes and with
// CrosSettings for settings changes. Whenever there is a change in the display
// configuration, any new display with an id that is not already in
// |rotated_display_ids_| will be rotated according to the policy. When there is
// a change to CrosSettings, the new policy is applied to all displays.
// NOTE: This only rotates displays on startup and when the policy changes.
// i.e. this will not override subsequent rotations (e.g. via Settings or an
// extension with display configuration permissions).
class DisplayRotationDefaultHandler
    : public ash::mojom::CrosDisplayConfigObserver {
 public:
  DisplayRotationDefaultHandler();
  ~DisplayRotationDefaultHandler() override;

  // ash::mojom::CrosDisplayConfigObserver
  void OnDisplayConfigChanged() override;

 private:
  // Receives the initial display info list and initializes the class.
  void OnGetInitialDisplayInfo(
      std::vector<ash::mojom::DisplayUnitInfoPtr> info_list);

  // Requests the list of displays and calls RotateDisplays.
  void RequestAndRotateDisplays();

  // Callback function for settings_observer_.
  void OnCrosSettingsChanged();

  // Applies the policy to all connected displays as necessary.
  void RotateDisplays(std::vector<ash::mojom::DisplayUnitInfoPtr> info_list);

  // Reads |chromeos::kDisplayRotationDefault| from CrosSettings and stores
  // its value, and whether it has a value, in member variables
  // |display_rotation_default_| and |policy_enabled_|. Returns true if the
  // setting changed.
  bool UpdateFromCrosSettings();

  bool policy_enabled_ = false;
  display::Display::Rotation display_rotation_default_ =
      display::Display::ROTATE_0;
  std::set<std::string> rotated_display_ids_;
  ash::mojom::CrosDisplayConfigControllerPtr cros_display_config_;
  mojo::AssociatedBinding<ash::mojom::CrosDisplayConfigObserver>
      cros_display_config_observer_binding_{this};
  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      settings_observer_;
  base::WeakPtrFactory<DisplayRotationDefaultHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DisplayRotationDefaultHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DISPLAY_ROTATION_DEFAULT_HANDLER_H_
