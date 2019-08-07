// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/fileapi/file_system_features.h"

namespace storage {

namespace features {

// Enables Filesystem API in incognito mode.
const base::Feature kEnableFilesystemInIncognito{
    "EnableFilesystemInIncognito", base::FEATURE_ENABLED_BY_DEFAULT};
}  // namespace features

}  // namespace storage
