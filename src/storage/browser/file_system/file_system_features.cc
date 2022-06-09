// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/file_system_features.h"

namespace storage {

namespace features {

// Enables persistent Filesystem API in incognito mode.
const base::Feature kEnablePersistentFilesystemInIncognito{
    "EnablePersistentFilesystemInIncognito", base::FEATURE_ENABLED_BY_DEFAULT};

// Creates FileSystemContexts in incognito mode. This is used to run web tests
// in incognito mode to ensure feature parity for FileSystemAccessAccessHandles.
const base::Feature kIncognitoFileSystemContextForTesting{
    "IncognitoFileSystemContextForTesting", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features

}  // namespace storage