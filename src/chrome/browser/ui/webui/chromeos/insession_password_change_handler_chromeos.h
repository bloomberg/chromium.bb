// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_INSESSION_PASSWORD_CHANGE_HANDLER_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_INSESSION_PASSWORD_CHANGE_HANDLER_CHROMEOS_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "chromeos/login/auth/cryptohome_authenticator.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace chromeos {

class InSessionPasswordChangeHandler : public content::WebUIMessageHandler,
                                       AuthStatusConsumer {
 public:
  explicit InSessionPasswordChangeHandler(
      const std::string& password_change_url);
  ~InSessionPasswordChangeHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  void HandleInitialize(const base::ListValue*);
  void HandleChangePassword(const base::ListValue* passwords);

  // AuthStatusConsumer:
  void OnAuthFailure(const AuthFailure& error) override;
  void OnAuthSuccess(const UserContext& user_context) override;

 private:
  const std::string password_change_url_;
  scoped_refptr<CryptohomeAuthenticator> authenticator_;
  base::WeakPtrFactory<InSessionPasswordChangeHandler> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(InSessionPasswordChangeHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_INSESSION_PASSWORD_CHANGE_HANDLER_CHROMEOS_H_
