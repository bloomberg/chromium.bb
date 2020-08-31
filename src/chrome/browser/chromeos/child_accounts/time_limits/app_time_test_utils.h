// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_TEST_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_TEST_UTILS_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/arc/mojom/app.mojom.h"

namespace extensions {
class Extension;
}

namespace chromeos {
namespace app_time {

arc::mojom::ArcPackageInfoPtr CreateArcAppPackage(
    const std::string& package_name);

arc::mojom::AppInfo CreateArcAppInfo(const std::string& package_name,
                                     const std::string& name);

scoped_refptr<extensions::Extension> CreateExtension(
    const std::string& extension_id,
    const std::string& name,
    const std::string& url,
    bool is_bookmark_app = false);

}  // namespace app_time
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_TEST_UTILS_H_
