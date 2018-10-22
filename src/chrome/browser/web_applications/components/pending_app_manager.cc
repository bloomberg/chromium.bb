// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/pending_app_manager.h"

#include <memory>
#include <utility>

namespace web_app {

// static
PendingAppManager::AppInfo PendingAppManager::AppInfo::Create(
    GURL url,
    LaunchContainer launch_container,
    bool create_shortcuts) {
  return AppInfo(url, launch_container, create_shortcuts,
                 InstallationFlag::kNone);
}

// static
PendingAppManager::AppInfo PendingAppManager::AppInfo::CreateForDefaultApp(
    GURL url,
    LaunchContainer launch_container,
    bool create_shortcuts) {
  return AppInfo(url, launch_container, create_shortcuts,
                 InstallationFlag::kDefaultApp);
}

// static
PendingAppManager::AppInfo PendingAppManager::AppInfo::CreateForPolicy(
    GURL url,
    LaunchContainer launch_container,
    bool create_shortcuts) {
  return AppInfo(url, launch_container, create_shortcuts,
                 InstallationFlag::kFromPolicy);
}

PendingAppManager::AppInfo::AppInfo(PendingAppManager::AppInfo&& other) =
    default;

PendingAppManager::AppInfo::~AppInfo() = default;

std::unique_ptr<PendingAppManager::AppInfo> PendingAppManager::AppInfo::Clone()
    const {
  std::unique_ptr<AppInfo> other(
      new AppInfo(url, launch_container, create_shortcuts, installation_flag));
  DCHECK_EQ(*this, *other);
  return other;
}

bool PendingAppManager::AppInfo::operator==(
    const PendingAppManager::AppInfo& other) const {
  return std::tie(url, launch_container, create_shortcuts, installation_flag) ==
         std::tie(other.url, other.launch_container, other.create_shortcuts,
                  other.installation_flag);
}

PendingAppManager::AppInfo::AppInfo(GURL url,
                                    LaunchContainer launch_container,
                                    bool create_shortcuts,
                                    InstallationFlag installation_flag)
    : url(std::move(url)),
      launch_container(launch_container),
      create_shortcuts(create_shortcuts),
      installation_flag(installation_flag) {}

PendingAppManager::PendingAppManager() = default;

PendingAppManager::~PendingAppManager() = default;

std::ostream& operator<<(std::ostream& out,
                         const PendingAppManager::AppInfo& app_info) {
  return out << "url: " << app_info.url << "\n launch_container: "
             << static_cast<int32_t>(app_info.launch_container)
             << "\n create_shortcuts: " << app_info.create_shortcuts
             << "\n installation_flag: "
             << static_cast<int64_t>(app_info.installation_flag);
}

}  // namespace web_app
