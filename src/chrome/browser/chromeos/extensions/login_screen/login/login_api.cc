// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/login_api.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/common/extensions/api/login.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#else
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/login_ash.h"
#endif

namespace extensions {

namespace {

crosapi::mojom::Login* GetLoginApi() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::Login>()
      .get();
#else
  return crosapi::CrosapiManager::Get()->crosapi_ash()->login_ash();
#endif
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Performs common crosapi validation. These errors are not caused by the
// extension so they are considered recoverable. Returns an error message on
// error, or nullopt on success.
absl::optional<std::string> ValidateCrosapi() {
  if (!chromeos::LacrosService::Get()->IsAvailable<crosapi::mojom::Login>()) {
    return "Unsupported by ash";
  }
  return absl::nullopt;
}

const char kCannotBeCalledFromLacros[] = "API cannot be called from Lacros";

#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace

ExtensionFunctionWithOptionalErrorResult::
    ~ExtensionFunctionWithOptionalErrorResult() = default;

void ExtensionFunctionWithOptionalErrorResult::OnResult(
    const absl::optional<std::string>& error) {
  if (error) {
    Respond(Error(*error));
    return;
  }

  return Respond(NoArguments());
}

ExtensionFunctionWithStringResult::~ExtensionFunctionWithStringResult() =
    default;

void ExtensionFunctionWithStringResult::OnResult(const std::string& result) {
  Respond(OneArgument(base::Value(result)));
}

ExtensionFunctionWithVoidResult::~ExtensionFunctionWithVoidResult() = default;

void ExtensionFunctionWithVoidResult::OnResult() {
  Respond(NoArguments());
}

LoginLaunchManagedGuestSessionFunction::
    LoginLaunchManagedGuestSessionFunction() = default;
LoginLaunchManagedGuestSessionFunction::
    ~LoginLaunchManagedGuestSessionFunction() = default;

ExtensionFunction::ResponseAction
LoginLaunchManagedGuestSessionFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return RespondNow(Error(kCannotBeCalledFromLacros));
#else
  auto parameters =
      api::login::LaunchManagedGuestSession::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  auto callback =
      base::BindOnce(&LoginLaunchManagedGuestSessionFunction::OnResult, this);

  absl::optional<std::string> password;
  if (parameters->password) {
    password = std::move(*parameters->password);
  }
  GetLoginApi()->LaunchManagedGuestSession(password, std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
#endif
}

LoginExitCurrentSessionFunction::LoginExitCurrentSessionFunction() = default;
LoginExitCurrentSessionFunction::~LoginExitCurrentSessionFunction() = default;

ExtensionFunction::ResponseAction LoginExitCurrentSessionFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  absl::optional<std::string> error = ValidateCrosapi();
  if (error.has_value()) {
    return RespondNow(Error(error.value()));
  }
#endif

  auto parameters = api::login::ExitCurrentSession::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  auto callback =
      base::BindOnce(&LoginExitCurrentSessionFunction::OnResult, this);

  absl::optional<std::string> data_for_next_login_attempt;
  if (parameters->data_for_next_login_attempt) {
    data_for_next_login_attempt =
        std::move(*parameters->data_for_next_login_attempt);
  }
  GetLoginApi()->ExitCurrentSession(data_for_next_login_attempt,
                                    std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

LoginFetchDataForNextLoginAttemptFunction::
    LoginFetchDataForNextLoginAttemptFunction() = default;
LoginFetchDataForNextLoginAttemptFunction::
    ~LoginFetchDataForNextLoginAttemptFunction() = default;

ExtensionFunction::ResponseAction
LoginFetchDataForNextLoginAttemptFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  absl::optional<std::string> error = ValidateCrosapi();
  if (error.has_value()) {
    return RespondNow(Error(error.value()));
  }
#endif

  auto callback = base::BindOnce(
      &LoginFetchDataForNextLoginAttemptFunction::OnResult, this);

  GetLoginApi()->FetchDataForNextLoginAttempt(std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

LoginLockManagedGuestSessionFunction::LoginLockManagedGuestSessionFunction() =
    default;
LoginLockManagedGuestSessionFunction::~LoginLockManagedGuestSessionFunction() =
    default;

ExtensionFunction::ResponseAction LoginLockManagedGuestSessionFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  absl::optional<std::string> error = ValidateCrosapi();
  if (error.has_value()) {
    return RespondNow(Error(error.value()));
  }
#endif

  auto callback =
      base::BindOnce(&LoginLockManagedGuestSessionFunction::OnResult, this);

  GetLoginApi()->LockManagedGuestSession(std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

LoginUnlockManagedGuestSessionFunction::
    LoginUnlockManagedGuestSessionFunction() = default;
LoginUnlockManagedGuestSessionFunction::
    ~LoginUnlockManagedGuestSessionFunction() = default;

ExtensionFunction::ResponseAction
LoginUnlockManagedGuestSessionFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return RespondNow(Error(kCannotBeCalledFromLacros));
#else
  auto parameters =
      api::login::UnlockManagedGuestSession::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  auto callback =
      base::BindOnce(&LoginUnlockManagedGuestSessionFunction::OnResult, this);

  GetLoginApi()->UnlockManagedGuestSession(parameters->password,
                                           std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
#endif
}

LoginLaunchSharedManagedGuestSessionFunction::
    LoginLaunchSharedManagedGuestSessionFunction() = default;
LoginLaunchSharedManagedGuestSessionFunction::
    ~LoginLaunchSharedManagedGuestSessionFunction() = default;

ExtensionFunction::ResponseAction
LoginLaunchSharedManagedGuestSessionFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return RespondNow(Error(kCannotBeCalledFromLacros));
#else
  auto parameters =
      api::login::LaunchSharedManagedGuestSession::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  auto callback = base::BindOnce(
      &LoginLaunchSharedManagedGuestSessionFunction::OnResult, this);

  GetLoginApi()->LaunchSharedManagedGuestSession(parameters->password,
                                                 std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
#endif
}

LoginEnterSharedSessionFunction::LoginEnterSharedSessionFunction() = default;
LoginEnterSharedSessionFunction::~LoginEnterSharedSessionFunction() = default;

ExtensionFunction::ResponseAction LoginEnterSharedSessionFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return RespondNow(Error(kCannotBeCalledFromLacros));
#else
  auto parameters = api::login::EnterSharedSession::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  auto callback =
      base::BindOnce(&LoginEnterSharedSessionFunction::OnResult, this);

  GetLoginApi()->EnterSharedSession(parameters->password, std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
#endif
}

LoginUnlockSharedSessionFunction::LoginUnlockSharedSessionFunction() = default;
LoginUnlockSharedSessionFunction::~LoginUnlockSharedSessionFunction() = default;

ExtensionFunction::ResponseAction LoginUnlockSharedSessionFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return RespondNow(Error(kCannotBeCalledFromLacros));
#else
  auto parameters = api::login::EnterSharedSession::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  auto callback =
      base::BindOnce(&LoginUnlockSharedSessionFunction::OnResult, this);

  GetLoginApi()->UnlockSharedSession(parameters->password, std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
#endif
}

LoginEndSharedSessionFunction::LoginEndSharedSessionFunction() = default;
LoginEndSharedSessionFunction::~LoginEndSharedSessionFunction() = default;

ExtensionFunction::ResponseAction LoginEndSharedSessionFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(jityao, b:217155485): Enable for Lacros after cleanup handlers are
  // added.
  return RespondNow(Error(kCannotBeCalledFromLacros));
#else
  auto callback =
      base::BindOnce(&LoginEndSharedSessionFunction::OnResult, this);

  GetLoginApi()->EndSharedSession(std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
#endif
}

LoginSetDataForNextLoginAttemptFunction::
    LoginSetDataForNextLoginAttemptFunction() = default;
LoginSetDataForNextLoginAttemptFunction::
    ~LoginSetDataForNextLoginAttemptFunction() = default;

ExtensionFunction::ResponseAction
LoginSetDataForNextLoginAttemptFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  absl::optional<std::string> error = ValidateCrosapi();
  if (error.has_value()) {
    return RespondNow(Error(error.value()));
  }
#endif

  auto parameters =
      api::login::SetDataForNextLoginAttempt::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  auto callback =
      base::BindOnce(&LoginSetDataForNextLoginAttemptFunction::OnResult, this);

  GetLoginApi()->SetDataForNextLoginAttempt(
      parameters->data_for_next_login_attempt, std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

}  // namespace extensions
