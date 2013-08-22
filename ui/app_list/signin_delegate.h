// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_SIGNIN_DELEGATE_H_
#define UI_APP_LIST_SIGNIN_DELEGATE_H_

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "ui/app_list/app_list_export.h"

namespace app_list {

class APP_LIST_EXPORT SigninDelegate {
 public:
  SigninDelegate();
  virtual ~SigninDelegate();

  virtual bool NeedSignin() = 0;
  virtual void ShowSignin() = 0;
  virtual void OpenLearnMore() = 0;
  virtual void OpenSettings() = 0;

  virtual base::string16 GetSigninHeading() = 0;
  virtual base::string16 GetSigninText() = 0;
  virtual base::string16 GetSigninButtonText() = 0;
  virtual base::string16 GetLearnMoreLinkText() = 0;
  virtual base::string16 GetSettingsLinkText() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SigninDelegate);
};

}  // namespace app_list

#endif  // UI_APP_LIST_SIGNIN_DELEGATE_H_
