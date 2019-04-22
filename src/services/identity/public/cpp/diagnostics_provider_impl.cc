// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/diagnostics_provider_impl.h"

#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "google_apis/gaia/oauth2_token_service_delegate.h"

namespace identity {

DiagnosticsProviderImpl::DiagnosticsProviderImpl(
    ProfileOAuth2TokenService* profile_oauth2_token_service,
    GaiaCookieManagerService* gaia_cookie_manager_service)
    : gaia_cookie_manager_service_(gaia_cookie_manager_service),
      profile_oauth2_token_service_(profile_oauth2_token_service) {
  DCHECK(gaia_cookie_manager_service_);
  DCHECK(profile_oauth2_token_service_);
}

DiagnosticsProviderImpl::~DiagnosticsProviderImpl() {}

OAuth2TokenServiceDelegate::LoadCredentialsState
DiagnosticsProviderImpl::GetDetailedStateOfLoadingOfRefreshTokens() const {
  DCHECK(profile_oauth2_token_service_->GetDelegate());
  return profile_oauth2_token_service_->GetDelegate()->load_credentials_state();
}

base::TimeDelta
DiagnosticsProviderImpl::GetDelayBeforeMakingAccessTokenRequests() const {
  const net::BackoffEntry* backoff_entry =
      profile_oauth2_token_service_->GetDelegateBackoffEntry();
  return backoff_entry ? backoff_entry->GetTimeUntilRelease()
                       : base::TimeDelta();
}

base::TimeDelta DiagnosticsProviderImpl::GetDelayBeforeMakingCookieRequests()
    const {
  DCHECK(gaia_cookie_manager_service_->GetBackoffEntry());
  return gaia_cookie_manager_service_->GetBackoffEntry()->GetTimeUntilRelease();
}

}  // namespace identity