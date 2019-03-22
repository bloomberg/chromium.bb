// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screen_manager.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"

namespace chromeos {

ScreenManager::ScreenManager() = default;

ScreenManager::~ScreenManager() = default;

BaseScreen* ScreenManager::GetScreen(OobeScreen screen) {
  auto iter = screens_.find(screen);
  if (iter != screens_.end())
    return iter->second.get();

  std::unique_ptr<BaseScreen> result =
      WizardController::default_controller()->CreateScreen(screen);
  DCHECK(result) << "Can not create screen named " << GetOobeScreenName(screen);
  BaseScreen* unowned_result = result.get();
  screens_[screen] = std::move(result);
  return unowned_result;
}

bool ScreenManager::HasScreen(OobeScreen screen) {
  return screens_.count(screen) > 0;
}

}  // namespace chromeos
