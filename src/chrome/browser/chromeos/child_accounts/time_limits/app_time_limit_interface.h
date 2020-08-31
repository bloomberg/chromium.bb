// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_LIMIT_INTERFACE_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_LIMIT_INTERFACE_H_

#include <string>

#include "base/optional.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"

class Profile;

namespace base {
class TimeDelta;
}  // namespace base

namespace chromeos {
namespace app_time {

// Interface of the object controlling UI for app time limits feature.
class AppTimeLimitInterface {
 public:
  // Factory method that returns object controlling UI for app time limits
  // feature. Provided to reduce the dependencies between API consumer and child
  // user related code. AppTimeLimitInterface object has a lifetime of a
  // KeyedService.
  static AppTimeLimitInterface* Get(Profile* profile);

  virtual ~AppTimeLimitInterface();

  // Blocks access to Chrome and web apps. Should be called when the daily
  // time limit is reached. Calling it multiple times is safe.
  // |app_service_id| identifies web application active when limit was reached.
  // Currently the web time limit is shared between all PWAs and Chrome and all
  // of them will be paused regardless |app_service_id|.
  virtual void PauseWebActivity(const std::string& app_service_id) = 0;

  // Resumes access to Chrome and web apps. Should be called when the daily time
  // limit is lifted. Calling it multiple times is safe. Subsequent calls will
  // be ignored.
  // |app_service_id| identifies web application active when limit was reached.
  // Currently the web time limit is shared between all PWAs and Chrome and all
  // of them will be resumed regardless |app_service_id|.
  virtual void ResumeWebActivity(const std::string& app_service_id) = 0;

  // Returns current time limit for the app identified by |app_service_id| and
  // |app_type|.Will return nullopt if there is no limit set or app does not
  // exist.
  virtual base::Optional<base::TimeDelta> GetTimeLimitForApp(
      const std::string& app_service_id,
      apps::mojom::AppType app_type) = 0;
};

}  // namespace app_time
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_TIME_LIMITS_APP_TIME_LIMIT_INTERFACE_H_
