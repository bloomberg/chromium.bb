// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_CHILD_ACCOUNT_SERVICE_ANDROID_H_
#define CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_CHILD_ACCOUNT_SERVICE_ANDROID_H_

#include <string>

#include "base/callback_forward.h"

namespace content {
class WebContents;
}

void ReauthenticateChildAccount(content::WebContents* web_contents,
                                const std::string& email,
                                const base::Callback<void(bool)>& callback);

#endif  // CHROME_BROWSER_SUPERVISED_USER_CHILD_ACCOUNTS_CHILD_ACCOUNT_SERVICE_ANDROID_H_
