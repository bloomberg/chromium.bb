// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/display_rotation_default_handler.h"

#include <stddef.h>

#include "ash/public/interfaces/constants.mojom.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/settings/cros_settings_names.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace policy {

DisplayRotationDefaultHandler::DisplayRotationDefaultHandler() {
  settings_observer_ = chromeos::CrosSettings::Get()->AddSettingsObserver(
      chromeos::kDisplayRotationDefault,
      base::Bind(&DisplayRotationDefaultHandler::OnCrosSettingsChanged,
                 base::Unretained(this)));

  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &cros_display_config_);

  // Make the initial display unit info request. This will be queued until the
  // Ash service is ready.
  cros_display_config_->GetDisplayUnitInfoList(
      false /* single_unified */,
      base::BindOnce(&DisplayRotationDefaultHandler::OnGetInitialDisplayInfo,
                     weak_ptr_factory_.GetWeakPtr()));
}

DisplayRotationDefaultHandler::~DisplayRotationDefaultHandler() = default;

void DisplayRotationDefaultHandler::OnDisplayConfigChanged() {
  RequestAndRotateDisplays();
}

void DisplayRotationDefaultHandler::OnGetInitialDisplayInfo(
    std::vector<ash::mojom::DisplayUnitInfoPtr> info_list) {
  // Add this as an observer to the mojo service now that it is ready.
  // (We only care about changes that occur after we set any policy
  // rotation below).
  ash::mojom::CrosDisplayConfigObserverAssociatedPtrInfo ptr_info;
  cros_display_config_observer_binding_.Bind(mojo::MakeRequest(&ptr_info));
  cros_display_config_->AddObserver(std::move(ptr_info));

  // Get the initial policy values from CrosSettings and apply any rotation.
  UpdateFromCrosSettings();
  RotateDisplays(std::move(info_list));
}

void DisplayRotationDefaultHandler::RequestAndRotateDisplays() {
  cros_display_config_->GetDisplayUnitInfoList(
      false /* single_unified */,
      base::BindOnce(&DisplayRotationDefaultHandler::RotateDisplays,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DisplayRotationDefaultHandler::OnCrosSettingsChanged() {
  if (!UpdateFromCrosSettings())
    return;
  // Policy changed, so reset all displays.
  rotated_display_ids_.clear();
  RequestAndRotateDisplays();
}

void DisplayRotationDefaultHandler::RotateDisplays(
    std::vector<ash::mojom::DisplayUnitInfoPtr> info_list) {
  if (!policy_enabled_)
    return;

  for (const ash::mojom::DisplayUnitInfoPtr& display_unit_info : info_list) {
    std::string display_id = display_unit_info->id;
    if (rotated_display_ids_.find(display_id) != rotated_display_ids_.end())
      continue;

    rotated_display_ids_.insert(display_id);
    display::Display::Rotation rotation(display_unit_info->rotation);
    if (rotation == display_rotation_default_)
      continue;

    // The following sets only the |rotation| property of the display
    // configuration; no other properties will be affected.
    auto config_properties = ash::mojom::DisplayConfigProperties::New();
    config_properties->rotation =
        ash::mojom::DisplayRotation::New(display_rotation_default_);
    cros_display_config_->SetDisplayProperties(
        display_unit_info->id, std::move(config_properties), base::DoNothing());
  }
}

bool DisplayRotationDefaultHandler::UpdateFromCrosSettings() {
  int new_rotation;
  bool new_policy_enabled = chromeos::CrosSettings::Get()->GetInteger(
      chromeos::kDisplayRotationDefault, &new_rotation);
  display::Display::Rotation new_display_rotation_default =
      display::Display::ROTATE_0;
  if (new_policy_enabled) {
    if (new_rotation >= display::Display::ROTATE_0 &&
        new_rotation <= display::Display::ROTATE_270) {
      new_display_rotation_default =
          static_cast<display::Display::Rotation>(new_rotation);
    } else {
      LOG(ERROR) << "CrosSettings contains invalid value " << new_rotation
                 << " for DisplayRotationDefault. Ignoring setting.";
      new_policy_enabled = false;
    }
  }
  if (new_policy_enabled != policy_enabled_ ||
      (new_policy_enabled &&
       new_display_rotation_default != display_rotation_default_)) {
    policy_enabled_ = new_policy_enabled;
    display_rotation_default_ = new_display_rotation_default;
    return true;
  }
  return false;
}

}  // namespace policy
