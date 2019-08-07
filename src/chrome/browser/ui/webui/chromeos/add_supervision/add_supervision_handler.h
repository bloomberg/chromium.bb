// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_ADD_SUPERVISION_ADD_SUPERVISION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_ADD_SUPERVISION_ADD_SUPERVISION_HANDLER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {
class WebUI;
}  // namespace content

namespace identity {
class AccessTokenFetcher;
struct AccessTokenInfo;
class IdentityManager;
}  // namespace identity

class GoogleServiceAuthError;

class AddSupervisionHandler
    : public add_supervision::mojom::AddSupervisionHandler {
 public:
  AddSupervisionHandler(
      add_supervision::mojom::AddSupervisionHandlerRequest request,
      content::WebUI* web_ui);
  ~AddSupervisionHandler() override;

  // add_supervision::mojom::AddSupervisionHandler overrides:
  void LogOut(LogOutCallback callback) override;
  void GetInstalledArcApps(GetInstalledArcAppsCallback callback) override;
  void GetOAuthToken(GetOAuthTokenCallback callback) override;

 private:
  void OnAccessTokenFetchComplete(GetOAuthTokenCallback callback,
                                  GoogleServiceAuthError error,
                                  identity::AccessTokenInfo access_token_info);

  mojo::Binding<add_supervision::mojom::AddSupervisionHandler> binding_;

  // The AddSupervisionUI that this AddSupervisionHandler belongs to.
  content::WebUI* web_ui_;

  // Used to fetch OAuth2 access tokens.
  identity::IdentityManager* identity_manager_;
  std::unique_ptr<identity::AccessTokenFetcher> oauth2_access_token_fetcher_;

  base::WeakPtrFactory<AddSupervisionHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AddSupervisionHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_ADD_SUPERVISION_ADD_SUPERVISION_HANDLER_H_
