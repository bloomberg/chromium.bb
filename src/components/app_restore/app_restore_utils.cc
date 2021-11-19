// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/app_restore_utils.h"

#include "ash/constants/app_types.h"
#include "base/bind.h"
#include "components/app_restore/desk_template_read_handler.h"
#include "components/app_restore/features.h"
#include "components/app_restore/full_restore_info.h"
#include "components/app_restore/full_restore_read_handler.h"
#include "components/app_restore/window_info.h"
#include "components/app_restore/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/views/widget/widget_delegate.h"

namespace app_restore {

void ApplyProperties(app_restore::WindowInfo* window_info,
                     ui::PropertyHandler* property_handler) {
  DCHECK(window_info);
  DCHECK(property_handler);

  // Create a clone so `property_handler` can have complete ownership of a copy
  // of WindowInfo.
  app_restore::WindowInfo* window_info_clone = window_info->Clone();
  property_handler->SetProperty(app_restore::kWindowInfoKey, window_info_clone);

  if (window_info->activation_index) {
    const int32_t index = *window_info->activation_index;
    // kActivationIndexKey is owned, which allows for passing in this raw
    // pointer.
    property_handler->SetProperty(app_restore::kActivationIndexKey,
                                  std::make_unique<int32_t>(index));
    // Windows opened from full restore should not be activated. Widgets that
    // are shown are activated by default. Force the widget to not be
    // activatable; the activation will be restored in ash once the window is
    // launched.
    property_handler->SetProperty(app_restore::kLaunchedFromFullRestoreKey,
                                  true);
  }
  if (window_info->pre_minimized_show_state_type) {
    property_handler->SetProperty(aura::client::kPreMinimizedShowStateKey,
                                  *window_info->pre_minimized_show_state_type);
  }
}

void ModifyWidgetParams(int32_t restore_window_id,
                        views::Widget::InitParams* out_params) {
  if (!full_restore::features::IsFullRestoreEnabled())
    return;

  DCHECK(out_params);

  const bool is_arc_app =
      out_params->init_properties_container.GetProperty(
          aura::client::kAppType) == static_cast<int>(ash::AppType::ARC_APP);
  std::unique_ptr<app_restore::WindowInfo> window_info;
  full_restore::FullRestoreReadHandler* full_restore_read_handler =
      full_restore::FullRestoreReadHandler::GetInstance();
  if (is_arc_app) {
    ArcReadHandler* arc_read_handler =
        full_restore_read_handler->arc_read_handler();
    window_info = arc_read_handler
                      ? arc_read_handler->GetWindowInfo(restore_window_id)
                      : nullptr;
  } else {
    // If full restore is not running, try and get `window_info` from desk
    // templates. Otherwise, default to getting `window_info` from full restore.
    // TODO(sammiequon): Separate full restore and desk templates logic.
    if (!full_restore_read_handler->IsFullRestoreRunning()) {
      window_info =
          DeskTemplateReadHandler::Get()->GetWindowInfo(restore_window_id);
    }
    if (!window_info) {
      window_info = full_restore_read_handler->GetWindowInfoForActiveProfile(
          restore_window_id);
    }
  }
  if (!window_info)
    return;

  ApplyProperties(window_info.get(), &out_params->init_properties_container);

  if (window_info->desk_id)
    out_params->workspace = base::NumberToString(*window_info->desk_id);
  if (window_info->current_bounds)
    out_params->bounds = *window_info->current_bounds;
  if (window_info->window_state_type) {
    // ToWindowShowState will make us lose some ash-specific types (left/right
    // snap). Ash is responsible for restoring these states by checking
    // GetWindowInfo.
    out_params->show_state =
        chromeos::ToWindowShowState(*window_info->window_state_type);
  }

  // Register to track when the widget has initialized. If a delegate is not
  // set, then the widget creator is responsible for calling
  // OnWidgetInitialized.
  views::WidgetDelegate* delegate = out_params->delegate;
  if (delegate) {
    delegate->RegisterWidgetInitializedCallback(base::BindOnce(
        [](views::WidgetDelegate* delegate) {
          full_restore::FullRestoreInfo::GetInstance()->OnWidgetInitialized(
              delegate->GetWidget());
        },
        delegate));
  }
}

int32_t FetchRestoreWindowId(const std::string& app_id) {
  if (!full_restore::features::IsFullRestoreEnabled())
    return 0;

  // If full restore is not running, check if desk templates can get a viable
  // window id, otherwise default to checking full restore.
  auto* full_restore_read_handler =
      full_restore::FullRestoreReadHandler::GetInstance();
  if (!full_restore_read_handler->IsFullRestoreRunning()) {
    const int32_t desk_template_restore_window_id =
        DeskTemplateReadHandler::Get()->FetchRestoreWindowId(app_id);
    if (desk_template_restore_window_id > 0)
      return desk_template_restore_window_id;
  }

  return full_restore::FullRestoreReadHandler::GetInstance()
      ->FetchRestoreWindowId(app_id);
}

int32_t GetArcSessionId() {
  return full_restore::FullRestoreReadHandler::GetInstance()->GetArcSessionId();
}

void SetArcSessionIdForWindowId(int32_t arc_session_id, int32_t window_id) {
  return full_restore::FullRestoreReadHandler::GetInstance()
      ->SetArcSessionIdForWindowId(arc_session_id, window_id);
}

int32_t GetArcRestoreWindowIdForTaskId(int32_t task_id) {
  if (!full_restore::features::IsFullRestoreEnabled())
    return 0;

  return full_restore::FullRestoreReadHandler::GetInstance()
      ->GetArcRestoreWindowIdForTaskId(task_id);
}

int32_t GetArcRestoreWindowIdForSessionId(int32_t session_id) {
  if (!full_restore::features::IsFullRestoreEnabled())
    return 0;

  return full_restore::FullRestoreReadHandler::GetInstance()
      ->GetArcRestoreWindowIdForSessionId(session_id);
}

}  // namespace app_restore
