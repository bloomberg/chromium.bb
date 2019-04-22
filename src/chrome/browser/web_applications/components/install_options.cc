// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/install_options.h"

#include <ostream>
#include <tuple>

namespace web_app {

InstallOptions::InstallOptions() = default;

InstallOptions::InstallOptions(const GURL& url,
                               LaunchContainer launch_container,
                               InstallSource install_source)
    : url(url),
      launch_container(launch_container),
      install_source(install_source) {}

InstallOptions::~InstallOptions() = default;

InstallOptions::InstallOptions(const InstallOptions& other) = default;

InstallOptions::InstallOptions(InstallOptions&& other) = default;

InstallOptions& InstallOptions::operator=(const InstallOptions& other) =
    default;

bool InstallOptions::operator==(const InstallOptions& other) const {
  return std::tie(url, launch_container, install_source,
                  add_to_applications_menu, add_to_desktop,
                  add_to_quick_launch_bar, override_previous_user_uninstall,
                  bypass_service_worker_check, require_manifest, always_update,
                  wait_for_windows_closed, install_placeholder,
                  reinstall_placeholder) ==
         std::tie(other.url, other.launch_container, other.install_source,
                  other.add_to_applications_menu, other.add_to_desktop,
                  other.add_to_quick_launch_bar,
                  other.override_previous_user_uninstall,
                  other.bypass_service_worker_check, other.require_manifest,
                  other.always_update, other.wait_for_windows_closed,
                  other.install_placeholder, other.reinstall_placeholder);
}

std::ostream& operator<<(std::ostream& out,
                         const InstallOptions& install_options) {
  return out << "url: " << install_options.url << "\n launch_container: "
             << static_cast<int32_t>(install_options.launch_container)
             << "\n install_source: "
             << static_cast<int32_t>(install_options.install_source)
             << "\n add_to_applications_menu: "
             << install_options.add_to_applications_menu
             << "\n add_to_desktop: " << install_options.add_to_desktop
             << "\n add_to_quick_launch_bar: "
             << install_options.add_to_quick_launch_bar
             << "\n override_previous_user_uninstall: "
             << install_options.override_previous_user_uninstall
             << "\n bypass_service_worker_check: "
             << install_options.bypass_service_worker_check
             << "\n require_manifest: " << install_options.require_manifest
             << "\n always_update: " << install_options.always_update
             << "\n wait_for_windows_closed: "
             << install_options.wait_for_windows_closed
             << "\n install_placeholder: "
             << install_options.install_placeholder
             << "\n reinstall_placeholder: "
             << install_options.reinstall_placeholder;
}

}  // namespace web_app
