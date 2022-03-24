// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/default_ranking.h"
#include "build/build_config.h"

namespace sharing {

#if !defined(OS_ANDROID)
std::vector<std::string> DefaultRankingForLocaleAndType(
    const std::string& locale,
    const std::string& type) {
  return {};
}
#endif  // OS_ANDROID

}  // namespace sharing
