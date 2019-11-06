// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IMPORTER_EDGE_IMPORTER_UTILS_WIN_H_
#define CHROME_COMMON_IMPORTER_EDGE_IMPORTER_UTILS_WIN_H_

#include "base/files/file_path.h"
#include "base/strings/string16.h"

namespace importer {

// Returns the key to be used in HKCU to look for Edge's settings.
// Overridable by tests via ImporterTestRegistryOverrider.
base::string16 GetEdgeSettingsKey();

// Returns the data path for the Edge browser. Returns an empty path on error.
base::FilePath GetEdgeDataFilePath();

// Returns true if Edge favorites is currently in legacy (pre-Edge 13) mode.
bool IsEdgeFavoritesLegacyMode();

// Returns true if the Edge browser is installed and available for import.
bool EdgeImporterCanImport();

}  // namespace importer

#endif  // CHROME_COMMON_IMPORTER_EDGE_IMPORTER_UTILS_WIN_H_
