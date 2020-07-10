// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_USERS_PRIVATE_USERS_PRIVATE_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_USERS_PRIVATE_USERS_PRIVATE_DELEGATE_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util.h"
#include "chrome/common/extensions/api/users_private.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_function.h"

class Profile;

namespace extensions {

// Provides prefs access for managing users.
// Use UsersPrivateDelegateFactory to create a UsersPrivateDelegate object.
class UsersPrivateDelegate : public KeyedService {
 public:
  explicit UsersPrivateDelegate(Profile* profile);
  ~UsersPrivateDelegate() override;

  // Gets a PrefsUtil object used for persisting settings.
  // The caller does not own the returned object.
  virtual PrefsUtil* GetPrefsUtil();

 protected:
  Profile* profile_;  // weak; not owned by us
  std::unique_ptr<PrefsUtil> prefs_util_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UsersPrivateDelegate);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_USERS_PRIVATE_USERS_PRIVATE_DELEGATE_H_
