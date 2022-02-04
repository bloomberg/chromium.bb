// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_TAB_HELPER_H_

#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@class CommandDispatcher;
@protocol FormSuggestionProvider;
@class PasswordController;
@protocol PasswordControllerDelegate;
@protocol PasswordGenerationProvider;
@protocol PasswordsUiDelegate;
@class UIViewController;

namespace password_manager {
class PasswordGenerationFrameHelper;
class PasswordManager;
class PasswordManagerClient;
class PasswordManagerDriver;
}

// Class binding a PasswordController to a WebState.
class PasswordTabHelper : public web::WebStateObserver,
                          public web::WebStateUserData<PasswordTabHelper> {
 public:
  PasswordTabHelper(const PasswordTabHelper&) = delete;
  PasswordTabHelper& operator=(const PasswordTabHelper&) = delete;

  ~PasswordTabHelper() override;

  // Creates a PasswordTabHelper and attaches it to the given |web_state|.
  static void CreateForWebState(web::WebState* web_state);

  // Sets the BaseViewController from which to present UI.
  void SetBaseViewController(UIViewController* baseViewController);

  // Sets the PasswordController delegate.
  void SetPasswordControllerDelegate(id<PasswordControllerDelegate> delegate);

  // Sets the CommandDispatcher.
  void SetDispatcher(CommandDispatcher* dispatcher);

  // Returns an object that can provide suggestions from the PasswordController.
  // May return nil.
  id<FormSuggestionProvider> GetSuggestionProvider();

  // Returns the PasswordGenerationFrameHelper owned by the PasswordController.
  password_manager::PasswordGenerationFrameHelper* GetGenerationHelper();

  // Returns the PasswordManager owned by the PasswordController.
  password_manager::PasswordManager* GetPasswordManager();

  // Returns the PasswordManagerClient owned by the PasswordController.
  password_manager::PasswordManagerClient* GetPasswordManagerClient();

  // Returns the PasswordManagerDriver owned by the PasswordController.
  password_manager::PasswordManagerDriver* GetPasswordManagerDriver();

  // Returns an object that can provide password generation from the
  // PasswordController. May return nil.
  id<PasswordGenerationProvider> GetPasswordGenerationProvider();

 private:
  friend class web::WebStateUserData<PasswordTabHelper>;

  explicit PasswordTabHelper(web::WebState* web_state);

  // web::WebStateObserver implementation.
  void WebStateDestroyed(web::WebState* web_state) override;

  // The Objective-C password controller instance.
  __strong PasswordController* controller_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_TAB_HELPER_H_
