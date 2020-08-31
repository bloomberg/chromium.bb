// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_VIEW_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_UI_SIGNIN_VIEW_CONTROLLER_DELEGATE_H_

#include "base/callback_forward.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

class Browser;
struct CoreAccountId;

namespace content {
class WebContents;
}

namespace signin {
enum class ReauthResult;
}

// Interface to the platform-specific managers of the Signin and Sync
// confirmation tab-modal dialogs. This and its platform-specific
// implementations are responsible for actually creating and owning the dialogs,
// as well as managing the navigation inside them.
// Subclasses are responsible for deleting themselves when the window they're
// managing closes.
class SigninViewControllerDelegate {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when a dialog controlled by this SigninViewControllerDelegate is
    // closed.
    virtual void OnModalSigninClosed() = 0;
  };

  SigninViewControllerDelegate(const SigninViewControllerDelegate&) = delete;
  SigninViewControllerDelegate& operator=(const SigninViewControllerDelegate&) =
      delete;

  // Returns a platform-specific SigninViewControllerDelegate instance that
  // displays the sync confirmation dialog. The returned object should delete
  // itself when the window it's managing is closed.
  static SigninViewControllerDelegate* CreateSyncConfirmationDelegate(
      Browser* browser);

  // Returns a platform-specific SigninViewControllerDelegate instance that
  // displays the modal sign in error dialog. The returned object should delete
  // itself when the window it's managing is closed.
  static SigninViewControllerDelegate* CreateSigninErrorDelegate(
      Browser* browser);

  // Returns a platform-specific SigninViewContolllerDelegate instance that
  // displays the reauth modal dialog. The returned object should delete itself
  // when the window it's managing is closed.
  static SigninViewControllerDelegate* CreateReauthDelegate(
      Browser* browser,
      const CoreAccountId& account_id,
      base::OnceCallback<void(signin::ReauthResult)> reauth_callback);

  // Returns a platform-specific SigninViewContolllerDelegate instance that
  // displays the fake reauth modal dialog. The returned object should delete
  // itself when the window it's managing is closed.
  // WARNING: This dialog is for development use only and should not be used in
  // production.
  static SigninViewControllerDelegate* CreateFakeReauthDelegate(
      Browser* browser,
      const CoreAccountId& account_id,
      base::OnceCallback<void(signin::ReauthResult)> reauth_callback);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Closes the sign-in dialog. Note that this method may destroy this object,
  // so the caller should no longer use this object after calling this method.
  virtual void CloseModalSignin() = 0;

  // This will be called by the base class to request a resize of the native
  // view hosting the content to |height|. |height| is the total height of the
  // content, in pixels.
  virtual void ResizeNativeView(int height) = 0;

  // Returns the web contents of the modal dialog.
  virtual content::WebContents* GetWebContents() = 0;

 protected:
  SigninViewControllerDelegate();
  virtual ~SigninViewControllerDelegate();

  void NotifyModalSigninClosed();

 private:
  base::ObserverList<Observer, true> observer_list_;
};

#endif  // CHROME_BROWSER_UI_SIGNIN_VIEW_CONTROLLER_DELEGATE_H_
