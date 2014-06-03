// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_menu.h"

#include "grit/ui_resources.h"
#include "grit/ui_strings.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/resource/resource_bundle.h"

namespace app_list {

AppListMenu::AppListMenu(AppListViewDelegate* delegate)
    : menu_model_(this),
      delegate_(delegate),
      users_(delegate->GetUsers()) {
  InitMenu();
}

AppListMenu::~AppListMenu() {}

void AppListMenu::InitMenu() {
  // User selector menu section. We don't show the user selector if there is
  // only 1 user.
  if (users_.size() > 1) {
    for (size_t i = 0; i < users_.size(); ++i) {
#if defined(OS_MACOSX)
      menu_model_.AddRadioItem(SELECT_PROFILE + i,
                               users_[i].email.empty() ? users_[i].name
                                                       : users_[i].email,
                               0 /* group_id */);
#elif defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
      menu_model_.AddItem(SELECT_PROFILE + i, users_[i].name);
      int menu_index = menu_model_.GetIndexOfCommandId(SELECT_PROFILE + i);
      menu_model_.SetSublabel(menu_index, users_[i].email);
      // Use custom check mark.
      if (users_[i].active) {
        ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
        menu_model_.SetIcon(menu_index, gfx::Image(*rb.GetImageSkiaNamed(
            IDR_APP_LIST_USER_INDICATOR)));
      }
#endif
    }
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  }

  menu_model_.AddItem(SHOW_SETTINGS, l10n_util::GetStringUTF16(
      IDS_APP_LIST_OPEN_SETTINGS));

  menu_model_.AddItem(SHOW_HELP, l10n_util::GetStringUTF16(
      IDS_APP_LIST_HELP));

  menu_model_.AddItem(SHOW_FEEDBACK, l10n_util::GetStringUTF16(
      IDS_APP_LIST_OPEN_FEEDBACK));
}

bool AppListMenu::IsCommandIdChecked(int command_id) const {
#if defined(OS_MACOSX)
  DCHECK_LT(static_cast<unsigned>(command_id) - SELECT_PROFILE, users_.size());
  return users_[command_id - SELECT_PROFILE].active;
#else
  return false;
#endif
}

bool AppListMenu::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool AppListMenu::GetAcceleratorForCommandId(int command_id,
                                             ui::Accelerator* accelerator) {
  return false;
}

void AppListMenu::ExecuteCommand(int command_id, int event_flags) {
  if (command_id >= SELECT_PROFILE) {
    delegate_->ShowForProfileByPath(
        users_[command_id - SELECT_PROFILE].profile_path);
    return;
  }
  switch (command_id) {
    case SHOW_SETTINGS:
      delegate_->OpenSettings();
      break;
    case SHOW_HELP:
      delegate_->OpenHelp();
      break;
    case SHOW_FEEDBACK:
      delegate_->OpenFeedback();
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace app_list
