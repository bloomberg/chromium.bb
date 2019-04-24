// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screen_manager.h"

#include <utility>

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

void ScreenManager::SetScreenForTesting(std::unique_ptr<BaseScreen> value) {
  // Capture screen id to avoid using `value` after moving it; = is not a
  // sequence point.
  auto id = value->screen_id();
  screens_[id] = std::move(value);
}

void ScreenManager::DeleteScreenForTesting(OobeScreen screen) {
  screens_[screen] = nullptr;
}

}  // namespace chromeos
