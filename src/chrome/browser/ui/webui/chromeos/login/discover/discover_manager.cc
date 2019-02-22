// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/discover/discover_manager.h"

#include <algorithm>

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/discover_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/modules/discover_module_launch_help_app.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/modules/discover_module_pin_setup.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/modules/discover_module_redeem_offers.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/modules/discover_module_sync_files.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/modules/discover_module_welcome.h"

namespace chromeos {

DiscoverManager::DiscoverManager() {
  CreateModules();
}

DiscoverManager::~DiscoverManager() = default;

// static
DiscoverManager* DiscoverManager::Get() {
  return g_browser_process->platform_part()->GetDiscoverManager();
}

bool DiscoverManager::IsCompleted() const {
  // Returns true if all of the modules are completed.
  return std::all_of(modules_.begin(), modules_.end(),
                     [](const auto& module_pair) {
                       return module_pair.second->IsCompleted();
                     });
}

void DiscoverManager::CreateModules() {
  modules_[DiscoverModuleLaunchHelpApp::kModuleName] =
      std::make_unique<DiscoverModuleLaunchHelpApp>();
  modules_[DiscoverModuleRedeemOffers::kModuleName] =
      std::make_unique<DiscoverModuleRedeemOffers>();
  modules_[DiscoverModuleSyncFiles::kModuleName] =
      std::make_unique<DiscoverModuleSyncFiles>();
  modules_[DiscoverModuleWelcome::kModuleName] =
      std::make_unique<DiscoverModuleWelcome>();
  modules_[DiscoverModulePinSetup::kModuleName] =
      std::make_unique<DiscoverModulePinSetup>();
}

std::vector<std::unique_ptr<DiscoverHandler>>
DiscoverManager::CreateWebUIHandlers() const {
  std::vector<std::unique_ptr<DiscoverHandler>> handlers;
  for (const auto& module_pair : modules_) {
    handlers.emplace_back(module_pair.second->CreateWebUIHandler());
  }
  return handlers;
}

DiscoverModule* DiscoverManager::GetModuleByName(
    const std::string& module_name) const {
  const auto it = modules_.find(module_name);
  return it == modules_.end() ? nullptr : it->second.get();
}

}  // namespace chromeos
