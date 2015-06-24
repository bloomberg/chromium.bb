// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_RESOURCE_RESOURCE_BUNDLE_ANDROID_H_
#define UI_BASE_RESOURCE_RESOURCE_BUNDLE_ANDROID_H_

#include <jni.h>
#include <string>

#include "base/basictypes.h"
#include "base/files/memory_mapped_file.h"
#include "ui/base/ui_base_export.h"

namespace ui {

// Loads "resources.apk" from the .apk. Falls back to loading from disk, which
// is necessary for tests.
UI_BASE_EXPORT void LoadMainAndroidPackFile(
    const char* path_within_apk,
    const base::FilePath& disk_file_path);

// Returns the file descriptor and region for resources.pak.
UI_BASE_EXPORT int GetMainAndroidPackFd(
    base::MemoryMappedFile::Region* out_region);

// Returns the file descriptor and region for chrome_100_percent.pak
UI_BASE_EXPORT int GetCommonResourcesPackFd(
    base::MemoryMappedFile::Region* out_region);

// Checks whether the locale data pak is contained within the .apk.
bool AssetContainedInApk(const std::string& filename);

bool RegisterResourceBundleAndroid(JNIEnv* env);

}  // namespace ui

#endif  // UI_BASE_RESOURCE_RESOURCE_BUNDLE_ANDROID_H_
