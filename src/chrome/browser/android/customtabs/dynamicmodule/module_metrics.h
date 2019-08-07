// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CUSTOMTABS_DYNAMICMODULE_MODULE_METRICS_H_
#define CHROME_BROWSER_ANDROID_CUSTOMTABS_DYNAMICMODULE_MODULE_METRICS_H_

#include <string>

namespace customtabs {

void RecordCodeMemoryFootprint(const std::string& package_name,
                               const std::string& suffix);
}  // namespace customtabs

#endif  // CHROME_BROWSER_ANDROID_CUSTOMTABS_DYNAMICMODULE_MODULE_METRICS_H_
