// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IMPORTER_IE_IMPORTER_UTILS_WIN_H_
#define CHROME_COMMON_IMPORTER_IE_IMPORTER_UTILS_WIN_H_

#include "base/strings/string16.h"

namespace importer {

// Returns the key to be used in HKCU to look for IE's favorites order blob.
// Overridable by tests via ImporterTestRegistryOverrider.
base::string16 GetIEFavoritesOrderKey();

// Returns the key to be used in HKCU to look for IE7 passwords.
// Overridable by tests via ImporterTestRegistryOverrider.
base::string16 GetIE7PasswordsKey();

// Returns the key to be used in HKCU to look for IE settings.
// Overridable by tests via ImporterTestRegistryOverrider.
base::string16 GetIESettingsKey();

}  // namespace importer

#endif  // CHROME_COMMON_IMPORTER_IE_IMPORTER_UTILS_WIN_H_
