// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/supervision/onboarding_controller_impl.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/supervision/onboarding_constants.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/prefs/pref_service.h"
#include "services/identity/public/cpp/access_token_fetcher.h"
#include "url/gurl.h"

namespace chromeos {
namespace supervision {
namespace {

// OAuth scope necessary to access the Supervision server.
const char kSupervisionScope[] =
    "https://www.googleapis.com/auth/kid.family.readonly";

GURL SupervisionServerBaseUrl() {
  GURL command_line_prefix(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kSupervisionOnboardingUrlPrefix));
  if (command_line_prefix.is_valid())
    return command_line_prefix;

  return GURL(kSupervisionServerUrlPrefix);
}

}  // namespace

OnboardingControllerImpl::OnboardingControllerImpl(Profile* profile)
    : profile_(profile) {
  DCHECK(profile);
}

OnboardingControllerImpl::~OnboardingControllerImpl() = default;

void OnboardingControllerImpl::BindRequest(
    mojom::OnboardingControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void OnboardingControllerImpl::BindWebviewHost(
    mojom::OnboardingWebviewHostPtr webview_host) {
  webview_host_ = std::move(webview_host);

  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);

  std::string account_id = identity_manager->GetPrimaryAccountId();

  OAuth2TokenService::ScopeSet scopes{kSupervisionScope};

  // base::Unretained is safe here since |access_token_fetcher_| is owned by
  // |this|.
  access_token_fetcher_ = identity_manager->CreateAccessTokenFetcherForAccount(
      account_id, "supervision_onboarding_controller", scopes,
      base::BindOnce(&OnboardingControllerImpl::AccessTokenCallback,
                     base::Unretained(this)),
      identity::AccessTokenFetcher::Mode::kImmediate);
}

void OnboardingControllerImpl::HandleAction(mojom::OnboardingAction action) {
  DCHECK(webview_host_);
  switch (action) {
    // TODO(958985): Implement the full flow state machine.
    case mojom::OnboardingAction::kSkipFlow:
    case mojom::OnboardingAction::kShowNextPage:
    case mojom::OnboardingAction::kShowPreviousPage:
      webview_host_->ExitFlow();
  }
}

void OnboardingControllerImpl::AccessTokenCallback(
    GoogleServiceAuthError error,
    identity::AccessTokenInfo access_token_info) {
  DCHECK(webview_host_);
  if (error.state() != GoogleServiceAuthError::NONE) {
    webview_host_->ExitFlow();
    return;
  }

  mojom::OnboardingPage page;
  page.access_token = access_token_info.token;
  page.custom_header_name = kExperimentHeaderName;

  page.url_filter_pattern = SupervisionServerBaseUrl().Resolve("/*").spec();
  page.url =
      SupervisionServerBaseUrl().Resolve(kOnboardingStartPageRelativeUrl);

  webview_host_->LoadPage(
      page.Clone(), base::BindOnce(&OnboardingControllerImpl::LoadPageCallback,
                                   base::Unretained(this)));
}

void OnboardingControllerImpl::LoadPageCallback(
    const base::Optional<std::string>& custom_header_value) {
  DCHECK(webview_host_);

  bool eligible =
      custom_header_value.has_value() &&
      base::EqualsCaseInsensitiveASCII(custom_header_value.value(),
                                       kDeviceOnboardingExperimentName);
  if (!eligible ||
      !base::FeatureList::IsEnabled(features::kSupervisionOnboardingScreens)) {
    webview_host_->ExitFlow();
  }

  profile_->GetPrefs()->SetBoolean(ash::prefs::kKioskNextShellEligible,
                                   eligible);
}

}  // namespace supervision
}  // namespace chromeos
