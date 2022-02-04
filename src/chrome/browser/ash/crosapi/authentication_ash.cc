// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/authentication_ash.h"

#include <utility>

#include "ash/components/login/auth/extended_authenticator.h"
#include "base/check.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils_chromeos.h"
#include "chrome/browser/extensions/api/quick_unlock_private/quick_unlock_private_ash_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/quick_unlock_private.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace crosapi {

using TokenInfo = extensions::api::quick_unlock_private::TokenInfo;

AuthenticationAsh::AuthenticationAsh() = default;
AuthenticationAsh::~AuthenticationAsh() = default;

void AuthenticationAsh::BindReceiver(
    mojo::PendingReceiver<mojom::Authentication> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void AuthenticationAsh::CreateQuickUnlockPrivateTokenInfo(
    const std::string& password,
    CreateQuickUnlockPrivateTokenInfoCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto helper =
      base::MakeRefCounted<extensions::QuickUnlockPrivateGetAuthTokenHelper>(
          ProfileManager::GetActiveUserProfile());
  // |extended_authenticator| is kept alive by |on_result_callback| binding.
  scoped_refptr<ash::ExtendedAuthenticator> extended_authenticator =
      ash::ExtendedAuthenticator::Create(helper.get());
  auto on_result_callback = base::BindOnce(
      &AuthenticationAsh::OnCreateQuickUnlockPrivateTokenInfoResults,
      weak_factory_.GetWeakPtr(), std::move(callback), extended_authenticator);
  // |helper| manages its own lifetime in Run(); can fire and forget.
  helper->Run(extended_authenticator.get(), password,
              std::move(on_result_callback));
}

void AuthenticationAsh::IsOsReauthAllowedForActiveUserProfile(
    base::TimeDelta auth_token_lifetime,
    IsOsReauthAllowedForActiveUserProfileCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool allowed = extensions::IsOsReauthAllowedAsh(
      ProfileManager::GetActiveUserProfile(), auth_token_lifetime);
  std::move(callback).Run(allowed);
}

void AuthenticationAsh::OnCreateQuickUnlockPrivateTokenInfoResults(
    CreateQuickUnlockPrivateTokenInfoCallback callback,
    scoped_refptr<ash::ExtendedAuthenticator> extended_authenticator,
    bool success,
    std::unique_ptr<TokenInfo> token_info,
    const std::string& error_message) {
  mojom::CreateQuickUnlockPrivateTokenInfoResultPtr result_ptr =
      mojom::CreateQuickUnlockPrivateTokenInfoResult::New();
  if (success) {
    DCHECK(token_info);
    crosapi::mojom::QuickUnlockPrivateTokenInfoPtr out_token_info =
        crosapi::mojom::QuickUnlockPrivateTokenInfo::New();
    out_token_info->token = token_info->token;
    out_token_info->lifetime_seconds = token_info->lifetime_seconds;
    result_ptr->set_token_info(std::move(out_token_info));
  } else {
    DCHECK(!error_message.empty());
    result_ptr->set_error_message(error_message);
  }
  std::move(callback).Run(std::move(result_ptr));

  extended_authenticator->SetConsumer(nullptr);
}

}  // namespace crosapi
