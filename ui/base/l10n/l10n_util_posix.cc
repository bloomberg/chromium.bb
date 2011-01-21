// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

namespace l10n_util {

// Return true blindly for now.
bool IsLocaleSupportedByOS(const std::string& locale) {
  return true;
}

}  // namespace l10n_util
