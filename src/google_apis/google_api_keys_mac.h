// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_API_KEYS_MAC_H_
#define GOOGLE_API_KEYS_MAC_H_

#include <string>

namespace google_apis {

std::string GetAPIKeyFromInfoPlist(const std::string& key_name);

}  // namespace google_apis

#endif  // GOOGLE_API_KEYS_MAC_H_
