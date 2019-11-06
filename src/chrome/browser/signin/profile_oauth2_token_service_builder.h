// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_PROFILE_OAUTH2_TOKEN_SERVICE_BUILDER_H_
#define CHROME_BROWSER_SIGNIN_PROFILE_OAUTH2_TOKEN_SERVICE_BUILDER_H_

#include <memory>

namespace content {
class BrowserContext;
}

class AccountTrackerService;
class ProfileOAuth2TokenService;
class IdentityManagerFactory;

class ProfileOAuth2TokenServiceBuilder {
 private:
  // Builds a ProfileOAuth2TokenService instance for use by IdentityManager.
  static std::unique_ptr<ProfileOAuth2TokenService> BuildInstanceFor(
      content::BrowserContext* context,
      AccountTrackerService* account_tracker_service);

  friend IdentityManagerFactory;
};
#endif  // CHROME_BROWSER_SIGNIN_PROFILE_OAUTH2_TOKEN_SERVICE_BUILDER_H_
