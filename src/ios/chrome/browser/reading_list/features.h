// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_FEATURES_H_
#define IOS_CHROME_BROWSER_READING_LIST_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace reading_list {
// Used to implement Reading List's Offline Version without relying on
// CRWNativeContent.
extern const base::Feature kOfflineVersionWithoutNativeContent;

// Whether offline page without relying on CRWNativeContent should be used.
bool IsOfflinePageWithoutNativeContentEnabled();
}  // namespace reading_list

#endif  // IOS_CHROME_BROWSER_READING_LIST_FEATURES_H_
