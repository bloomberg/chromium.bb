// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_SIGNIN_DELEGATE_H_
#define UI_APP_LIST_SIGNIN_DELEGATE_H_

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "ui/app_list/app_list_export.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace app_list {

class SigninDelegateObserver;

class APP_LIST_EXPORT SigninDelegate {
 public:
  SigninDelegate();
  virtual ~SigninDelegate();

  virtual bool NeedSignin() = 0;
  virtual content::WebContents* PrepareForSignin() = 0;

  void AddObserver(SigninDelegateObserver* observer);
  void RemoveObserver(SigninDelegateObserver* observer);

 protected:
  void NotifySigninSuccess();

 private:
  ObserverList<SigninDelegateObserver, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(SigninDelegate);
};

}  // namespace app_list

#endif  // UI_APP_LIST_SIGNIN_DELEGATE_H_
