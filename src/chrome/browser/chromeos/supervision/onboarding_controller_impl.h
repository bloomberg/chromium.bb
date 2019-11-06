// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SUPERVISION_ONBOARDING_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_SUPERVISION_ONBOARDING_CONTROLLER_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/supervision/mojom/onboarding_controller.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "url/gurl.h"

class Profile;

namespace identity {
class AccessTokenFetcher;
}

namespace chromeos {
namespace supervision {

class OnboardingControllerImpl : public mojom::OnboardingController {
 public:
  explicit OnboardingControllerImpl(Profile* profile);
  ~OnboardingControllerImpl() override;

  void BindRequest(mojom::OnboardingControllerRequest request);

 private:
  // mojom::OnboardingController:
  void BindWebviewHost(mojom::OnboardingWebviewHostPtr webview_host) override;
  void HandleAction(mojom::OnboardingAction action) override;

  // Callback to get the access token from the IdentityManager.
  void AccessTokenCallback(GoogleServiceAuthError error,
                           identity::AccessTokenInfo access_token_info);

  // Callback to OnboardingWebviewHost::LoadPage.
  void LoadPageCallback(const base::Optional<std::string>& custom_header_value);

  Profile* profile_;
  mojom::OnboardingWebviewHostPtr webview_host_;
  mojo::BindingSet<mojom::OnboardingController> bindings_;
  std::unique_ptr<identity::AccessTokenFetcher> access_token_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(OnboardingControllerImpl);
};

}  // namespace supervision
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SUPERVISION_ONBOARDING_CONTROLLER_IMPL_H_
