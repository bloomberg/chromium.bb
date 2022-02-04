// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "net/cookies/canonical_cookie.h"

namespace base {
class DictionaryValue;
}

namespace signin_metrics {
enum class AccessPoint;
}

extern const char kSignInPromoQueryKeyShowAccountManagement[];

// The base class handler for the inline login WebUI.
class InlineLoginHandler : public content::WebUIMessageHandler {
 public:
  InlineLoginHandler();

  InlineLoginHandler(const InlineLoginHandler&) = delete;
  InlineLoginHandler& operator=(const InlineLoginHandler&) = delete;

  ~InlineLoginHandler() override;

  // content::WebUIMessageHandler overrides:
  void RegisterMessages() override;
  void OnJavascriptDisallowed() override;

 protected:
  // Enum for gaia auth mode, must match AuthMode defined in
  // chrome/browser/resources/gaia_auth_host/authenticator.js.
  enum AuthMode {
    kDefaultAuthMode = 0,
    kOfflineAuthMode = 1,
    kDesktopAuthMode = 2
  };

  // Parameters passed to `CompleteLogin` method.
  struct CompleteLoginParams {
    CompleteLoginParams();
    CompleteLoginParams(const CompleteLoginParams&);
    CompleteLoginParams& operator=(const CompleteLoginParams&);
    ~CompleteLoginParams();

    std::string email;
    std::string password;
    std::string gaia_id;
    std::string auth_code;
    bool skip_for_now = false;
    bool trusted_value = false;
    bool trusted_found = false;
    bool choose_what_to_sync = false;
    // Whether the account should be available in ARC after addition. Used only
    // on Chrome OS.
    bool is_available_in_arc = false;
  };

  // Closes the dialog by calling the |inline.login.closeDialog| Javascript
  // function.
  // Does nothing if calling Javascript functions is not allowed.
  void CloseDialogFromJavascript();

 private:
  // JS callback to prepare for starting auth.
  void HandleInitializeMessage(const base::ListValue* args);

  // Continue to initialize the gaia auth extension. It calls
  // |SetExtraInitParams| to set extra init params.
  void ContinueHandleInitializeMessage();

  // JS callback to handle tasks after auth extension loads.
  virtual void HandleAuthExtensionReadyMessage(const base::ListValue* args) {}

  // JS callback to complete login. It calls |CompleteLogin| to do the real
  // work.
  void HandleCompleteLoginMessage(const base::ListValue* args);

  // Called by HandleCompleteLoginMessage after it gets the GAIA URL's cookies
  // from the CookieManager.
  void HandleCompleteLoginMessageWithCookies(
      const base::ListValue& args,
      const net::CookieAccessResultList& cookies,
      const net::CookieAccessResultList& excluded_cookies);

  // JS callback to switch the UI from a constrainted dialog to a full tab.
  void HandleSwitchToFullTabMessage(const base::ListValue* args);

  // Handles the web ui message sent when the window is closed from javascript.
  virtual void HandleDialogClose(const base::ListValue* args);

  virtual void SetExtraInitParams(base::DictionaryValue& params) {}
  virtual void CompleteLogin(const CompleteLoginParams& params) = 0;

  base::WeakPtrFactory<InlineLoginHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_HANDLER_H_
