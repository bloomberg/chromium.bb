// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_GLOBAL_ERROR_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_GLOBAL_ERROR_H_

#include <set>
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/signin_error_controller.h"

class Profile;

// Shows auth errors on the wrench menu using a bubble view and a menu item.
class SigninGlobalError : public GlobalErrorWithStandardBubble,
                          public SigninErrorController::Observer,
                          public KeyedService {
 public:
  SigninGlobalError(SigninErrorController* error_controller,
                    Profile* profile);

  SigninGlobalError(const SigninGlobalError&) = delete;
  SigninGlobalError& operator=(const SigninGlobalError&) = delete;

  ~SigninGlobalError() override;

  // Returns true if there is an authentication error.
  bool HasError();

 private:
  FRIEND_TEST_ALL_PREFIXES(SigninGlobalErrorTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(SigninGlobalErrorTest, AuthStatusEnumerateAllErrors);

  // KeyedService:
  void Shutdown() override;

  // GlobalErrorWithStandardBubble:
  bool HasMenuItem() override;
  int MenuItemCommandID() override;
  std::u16string MenuItemLabel() override;
  void ExecuteMenuItem(Browser* browser) override;
  bool HasBubbleView() override;
  std::u16string GetBubbleViewTitle() override;
  std::vector<std::u16string> GetBubbleViewMessages() override;
  std::u16string GetBubbleViewAcceptButtonLabel() override;
  std::u16string GetBubbleViewCancelButtonLabel() override;
  void OnBubbleViewDidClose(Browser* browser) override;
  void BubbleViewAcceptButtonPressed(Browser* browser) override;
  void BubbleViewCancelButtonPressed(Browser* browser) override;

  // SigninErrorController::Observer:
  void OnErrorChanged() override;

  // The Profile this service belongs to.
  raw_ptr<Profile> profile_;

  // The SigninErrorController that provides auth status.
  raw_ptr<SigninErrorController> error_controller_;
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_GLOBAL_ERROR_H_
