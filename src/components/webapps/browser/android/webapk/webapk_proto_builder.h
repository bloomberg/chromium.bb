// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPK_WEBAPK_PROTO_BUILDER_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPK_WEBAPK_PROTO_BUILDER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "components/webapk/webapk.pb.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/android/webapk/webapk_icon_hasher.h"
#include "components/webapps/browser/android/webapk/webapk_types.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace webapps {

// Populates webapk::WebApk and returns it.
// Must be called on a worker thread because it encodes an SkBitmap.
// The splash icon can be passed either via |icon_url_to_murmur2_hash| or via
// |splash_icon| parameter. |splash_icon| parameter is only used when the
// splash icon URL is unknown.
std::unique_ptr<std::string> BuildProtoInBackground(
    const webapps::ShortcutInfo& shortcut_info,
    const SkBitmap& primary_icon,
    bool is_primary_icon_maskable,
    const SkBitmap& splash_icon,
    const std::string& package_name,
    const std::string& version,
    std::map<std::string, WebApkIconHasher::Icon> icon_url_to_murmur2_hash,
    bool is_manifest_stale,
    bool is_app_identity_update_supported,
    std::vector<WebApkUpdateReason> update_reasons);

// Builds the WebAPK proto for an update request and stores it to
// |update_request_path|. Returns whether the proto was successfully written to
// disk.
bool StoreUpdateRequestToFileInBackground(
    const base::FilePath& update_request_path,
    const webapps::ShortcutInfo& shortcut_info,
    const SkBitmap& primary_icon,
    bool is_primary_icon_maskable,
    const SkBitmap& splash_icon,
    const std::string& package_name,
    const std::string& version,
    std::map<std::string, WebApkIconHasher::Icon> icon_url_to_murmur2_hash,
    bool is_manifest_stale,
    bool is_app_identity_update_supported,
    std::vector<WebApkUpdateReason> update_reasons);

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_WEBAPK_WEBAPK_PROTO_BUILDER_H_
