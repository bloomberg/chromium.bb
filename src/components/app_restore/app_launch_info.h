// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_RESTORE_APP_LAUNCH_INFO_H_
#define COMPONENTS_APP_RESTORE_APP_LAUNCH_INFO_H_

#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace app_restore {

// This class is the parameter for the interface SaveAppLaunchInfo, to save the
// app launch information.
struct COMPONENT_EXPORT(APP_RESTORE) AppLaunchInfo {
  AppLaunchInfo(const std::string& app_id,
                int32_t window_id,
                apps::mojom::LaunchContainer container,
                WindowOpenDisposition disposition,
                int64_t display_id,
                std::vector<base::FilePath> launch_files,
                apps::mojom::IntentPtr intent);

  AppLaunchInfo(const std::string& app_id, int32_t window_id);

  AppLaunchInfo(const std::string& app_id,
                apps::mojom::LaunchContainer container,
                WindowOpenDisposition disposition,
                int64_t display_id,
                std::vector<base::FilePath> launch_files,
                apps::mojom::IntentPtr intent);

  AppLaunchInfo(const std::string& app_id,
                int32_t event_flags,
                int32_t arc_session_id,
                int64_t display_id);

  AppLaunchInfo(const std::string& app_id,
                int32_t event_flags,
                apps::mojom::IntentPtr intent,
                int32_t arc_session_id,
                int64_t display_id);

  AppLaunchInfo(const std::string& app_id,
                const std::string& handler_id,
                std::vector<base::FilePath> launch_files);

  AppLaunchInfo(const AppLaunchInfo&) = delete;
  AppLaunchInfo& operator=(const AppLaunchInfo&) = delete;

  ~AppLaunchInfo();

  std::string app_id;
  absl::optional<int32_t> window_id;
  absl::optional<int32_t> event_flag;
  absl::optional<int32_t> container;
  absl::optional<int32_t> disposition;
  absl::optional<int32_t> arc_session_id;
  absl::optional<int64_t> display_id;
  absl::optional<std::string> handler_id;
  absl::optional<std::vector<GURL>> urls;
  absl::optional<int32_t> active_tab_index;
  absl::optional<std::vector<base::FilePath>> file_paths;
  absl::optional<apps::mojom::IntentPtr> intent;
  absl::optional<bool> app_type_browser;
  absl::optional<std::string> app_name;
};

}  // namespace app_restore

#endif  // COMPONENTS_APP_RESTORE_APP_LAUNCH_INFO_H_
