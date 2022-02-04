// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_api.h"

#include "base/values.h"
#include "chrome/browser/extensions/api/platform_keys/platform_keys_api.h"
#include "chrome/browser/platform_keys/extension_platform_keys_service.h"
#include "chrome/browser/platform_keys/extension_platform_keys_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/enterprise_platform_keys.h"
#include "chrome/common/extensions/api/enterprise_platform_keys_internal.h"
#include "chrome/common/pref_names.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/keystore_service_ash.h"
#include "chrome/browser/ash/crosapi/keystore_service_factory_ash.h"
#endif  // #if BUILDFLAG(IS_CHROMEOS_ASH)

namespace extensions {

namespace {

namespace api_epk = api::enterprise_platform_keys;
namespace api_epki = api::enterprise_platform_keys_internal;
using crosapi::mojom::KeystoreService;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
const char kUnsupportedByAsh[] = "Not implemented.";
const char kUnsupportedProfile[] = "Not available.";
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
const char kExtensionDoesNotHavePermission[] =
    "The extension does not have permission to call this function.";

crosapi::mojom::KeystoreService* GetKeystoreService(
    content::BrowserContext* browser_context) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(b/191958380): Lift the restriction when *.platformKeys.* APIs are
  // implemented for secondary profiles in Lacros.
  CHECK(Profile::FromBrowserContext(browser_context)->IsMainProfile())
      << "Attempted to use an incorrect profile. Please file a bug at "
         "https://bugs.chromium.org/ if this happens.";
  return chromeos::LacrosService::Get()->GetRemote<KeystoreService>().get();
#endif  // #if BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  return crosapi::KeystoreServiceFactoryAsh::GetForBrowserContext(
      browser_context);
#endif  // #if BUILDFLAG(IS_CHROMEOS_ASH)
}

// Performs common crosapi validation. These errors are not caused by the
// extension so they are considered recoverable. Returns an error message on
// error, or empty string on success. |min_version| is the minimum version of
// the ash implementation of KeystoreService necessary to support this
// extension. |context| is the browser context in which the extension is hosted.
std::string ValidateCrosapi(int min_version, content::BrowserContext* context) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service || !service->IsAvailable<crosapi::mojom::KeystoreService>())
    return kUnsupportedByAsh;

  int version = service->GetInterfaceVersion(KeystoreService::Uuid_);
  if (version < min_version)
    return kUnsupportedByAsh;

  // These APIs are used in security-sensitive contexts. We need to ensure that
  // the user for ash is the same as the user for lacros. We do this by
  // restricting the API to the default profile, which is guaranteed to be the
  // same user.
  if (!Profile::FromBrowserContext(context)->IsMainProfile())
    return kUnsupportedProfile;
#endif  // #if BUILDFLAG(IS_CHROMEOS_LACROS)

  return "";
}

absl::optional<crosapi::mojom::KeystoreType> KeystoreTypeFromString(
    const std::string& input) {
  if (input == "user")
    return crosapi::mojom::KeystoreType::kUser;
  if (input == "system")
    return crosapi::mojom::KeystoreType::kDevice;
  return absl::nullopt;
}

// Validates that |token_id| is well-formed. Converts |token_id| into the output
// parameter |keystore|. Only populated on success. Returns an empty string on
// success and an error message on error. A validation error should result in
// extension termination.
std::string ValidateInput(const std::string& token_id,
                          crosapi::mojom::KeystoreType* keystore) {
  absl::optional<crosapi::mojom::KeystoreType> keystore_type =
      KeystoreTypeFromString(token_id);
  if (!keystore_type)
    return platform_keys::kErrorInvalidToken;

  *keystore = keystore_type.value();
  return "";
}

std::vector<uint8_t> VectorFromString(const std::string& s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

std::string StringFromVector(const std::vector<uint8_t>& v) {
  return std::string(v.begin(), v.end());
}

}  // namespace

namespace platform_keys {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kAttestationExtensionAllowlist);
}

bool IsExtensionAllowed(Profile* profile, const Extension* extension) {
  if (Manifest::IsComponentLocation(extension->location())) {
    // Note: For this to even be called, the component extension must also be
    // allowed in chrome/common/extensions/api/_permission_features.json
    return true;
  }
  const base::Value* list =
      profile->GetPrefs()->GetList(prefs::kAttestationExtensionAllowlist);
  DCHECK_NE(list, nullptr);
  base::Value value(extension->id());
  return std::find(list->GetList().begin(), list->GetList().end(), value) !=
         list->GetList().end();
}

}  // namespace platform_keys

//------------------------------------------------------------------------------

EnterprisePlatformKeysInternalGenerateKeyFunction::
    ~EnterprisePlatformKeysInternalGenerateKeyFunction() = default;

ExtensionFunction::ResponseAction
EnterprisePlatformKeysInternalGenerateKeyFunction::Run() {
  std::unique_ptr<api_epki::GenerateKey::Params> params(
      api_epki::GenerateKey::Params::Create(args()));

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(b/191958380): Lift the restriction when *.platformKeys.* APIs are
  // implemented for secondary profiles in Lacros.
  if (!Profile::FromBrowserContext(browser_context())->IsMainProfile())
    return RespondNow(Error(kUnsupportedProfile));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  EXTENSION_FUNCTION_VALIDATE(params);
  absl::optional<chromeos::platform_keys::TokenId> platform_keys_token_id =
      platform_keys::ApiIdToPlatformKeysTokenId(params->token_id);
  if (!platform_keys_token_id)
    return RespondNow(Error(platform_keys::kErrorInvalidToken));

  chromeos::ExtensionPlatformKeysService* service =
      chromeos::ExtensionPlatformKeysServiceFactory::GetForBrowserContext(
          browser_context());
  DCHECK(service);

  if (params->algorithm.name == "RSASSA-PKCS1-v1_5") {
    // TODO(pneubeck): Add support for unsigned integers to IDL.
    EXTENSION_FUNCTION_VALIDATE(params->algorithm.modulus_length &&
                                *(params->algorithm.modulus_length) >= 0);
    service->GenerateRSAKey(
        platform_keys_token_id.value(), *(params->algorithm.modulus_length),
        params->software_backed, extension_id(),
        base::BindOnce(
            &EnterprisePlatformKeysInternalGenerateKeyFunction::OnGeneratedKey,
            this));
  } else if (params->algorithm.name == "ECDSA") {
    EXTENSION_FUNCTION_VALIDATE(params->algorithm.named_curve);
    service->GenerateECKey(
        platform_keys_token_id.value(), *(params->algorithm.named_curve),
        extension_id(),
        base::BindOnce(
            &EnterprisePlatformKeysInternalGenerateKeyFunction::OnGeneratedKey,
            this));
  } else {
    NOTREACHED();
    EXTENSION_FUNCTION_VALIDATE(false);
  }
  return RespondLater();
}

void EnterprisePlatformKeysInternalGenerateKeyFunction::OnGeneratedKey(
    const std::string& public_key_der,
    absl::optional<crosapi::mojom::KeystoreError> error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!error) {
    Respond(ArgumentList(api_epki::GenerateKey::Results::Create(
        std::vector<uint8_t>(public_key_der.begin(), public_key_der.end()))));
  } else {
    Respond(
        Error(chromeos::platform_keys::KeystoreErrorToString(error.value())));
  }
}

//------------------------------------------------------------------------------

ExtensionFunction::ResponseAction
EnterprisePlatformKeysGetCertificatesFunction::Run() {
  std::unique_ptr<api_epk::GetCertificates::Params> params(
      api_epk::GetCertificates::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error =
      ValidateCrosapi(KeystoreService::kDEPRECATED_GetCertificatesMinVersion,
                      browser_context());
  if (!error.empty()) {
    return RespondNow(Error(error));
  }

  crosapi::mojom::KeystoreType keystore;
  error = ValidateInput(params->token_id, &keystore);
  if (!error.empty()) {
    return RespondNow(Error(error));
  }

  auto c = base::BindOnce(
      &EnterprisePlatformKeysGetCertificatesFunction::OnGetCertificates, this);
  GetKeystoreService(browser_context())
      ->DEPRECATED_GetCertificates(keystore, std::move(c));
  return RespondLater();
}

void EnterprisePlatformKeysGetCertificatesFunction::OnGetCertificates(
    crosapi::mojom::DEPRECATED_GetCertificatesResultPtr result) {
  if (result->is_error_message()) {
    Respond(Error(result->get_error_message()));
    return;
  }
  DCHECK(result->is_certificates());

  auto client_certs = std::make_unique<base::ListValue>();
  for (std::vector<uint8_t>& cert : result->get_certificates()) {
    client_certs->Append(std::make_unique<base::Value>(std::move(cert)));
  }

  auto results = std::make_unique<base::ListValue>();
  results->Append(std::move(client_certs));
  Respond(ArgumentList(std::move(results)));
}

//------------------------------------------------------------------------------

ExtensionFunction::ResponseAction
EnterprisePlatformKeysImportCertificateFunction::Run() {
  std::unique_ptr<api_epk::ImportCertificate::Params> params(
      api_epk::ImportCertificate::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error = ValidateCrosapi(
      KeystoreService::kDEPRECATED_AddCertificateMinVersion, browser_context());
  if (!error.empty()) {
    return RespondNow(Error(error));
  }

  crosapi::mojom::KeystoreType keystore;
  error = ValidateInput(params->token_id, &keystore);
  EXTENSION_FUNCTION_VALIDATE(error.empty());

  auto c = base::BindOnce(
      &EnterprisePlatformKeysImportCertificateFunction::OnAddCertificate, this);
  GetKeystoreService(browser_context())
      ->DEPRECATED_AddCertificate(keystore, params->certificate, std::move(c));
  return RespondLater();
}

void EnterprisePlatformKeysImportCertificateFunction::OnAddCertificate(
    const std::string& error) {
  if (error.empty()) {
    Respond(NoArguments());
  } else {
    Respond(Error(error));
  }
}

//------------------------------------------------------------------------------

ExtensionFunction::ResponseAction
EnterprisePlatformKeysRemoveCertificateFunction::Run() {
  std::unique_ptr<api_epk::RemoveCertificate::Params> params(
      api_epk::RemoveCertificate::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error =
      ValidateCrosapi(KeystoreService::kDEPRECATED_RemoveCertificateMinVersion,
                      browser_context());
  if (!error.empty()) {
    return RespondNow(Error(error));
  }

  crosapi::mojom::KeystoreType keystore;
  error = ValidateInput(params->token_id, &keystore);
  EXTENSION_FUNCTION_VALIDATE(error.empty());

  auto c = base::BindOnce(
      &EnterprisePlatformKeysRemoveCertificateFunction::OnRemoveCertificate,
      this);
  GetKeystoreService(browser_context())
      ->DEPRECATED_RemoveCertificate(keystore, params->certificate,
                                     std::move(c));
  return RespondLater();
}

void EnterprisePlatformKeysRemoveCertificateFunction::OnRemoveCertificate(
    const std::string& error) {
  if (error.empty()) {
    Respond(NoArguments());
  } else {
    Respond(Error(error));
  }
}

//------------------------------------------------------------------------------

ExtensionFunction::ResponseAction
EnterprisePlatformKeysInternalGetTokensFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().empty());

  std::string error = ValidateCrosapi(
      KeystoreService::kDEPRECATED_GetKeyStoresMinVersion, browser_context());
  if (!error.empty()) {
    return RespondNow(Error(error));
  }

  auto c = base::BindOnce(
      &EnterprisePlatformKeysInternalGetTokensFunction::OnGetKeyStores, this);
  GetKeystoreService(browser_context())->DEPRECATED_GetKeyStores(std::move(c));
  return RespondLater();
}

void EnterprisePlatformKeysInternalGetTokensFunction::OnGetKeyStores(
    crosapi::mojom::DEPRECATED_GetKeyStoresResultPtr result) {
  if (result->is_error_message()) {
    Respond(Error(result->get_error_message()));
    return;
  }
  DCHECK(result->is_key_stores());

  std::vector<std::string> key_stores;
  using KeystoreType = crosapi::mojom::KeystoreType;
  for (KeystoreType keystore_type : result->get_key_stores()) {
    if (!crosapi::mojom::IsKnownEnumValue(keystore_type)) {
      continue;
    }

    switch (keystore_type) {
      case KeystoreType::kUser:
        key_stores.push_back("user");
        break;
      case KeystoreType::kDevice:
        key_stores.push_back("system");
        break;
    }
  }
  Respond(ArgumentList(api_epki::GetTokens::Results::Create(key_stores)));
}

//------------------------------------------------------------------------------

ExtensionFunction::ResponseAction
EnterprisePlatformKeysChallengeMachineKeyFunction::Run() {
  std::unique_ptr<api_epk::ChallengeMachineKey::Params> params(
      api_epk::ChallengeMachineKey::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::string error = ValidateCrosapi(
      KeystoreService::kDEPRECATED_ChallengeAttestationOnlyKeystoreMinVersion,
      browser_context());
  if (!error.empty())
    return RespondNow(Error(error));

  if (!platform_keys::IsExtensionAllowed(
          Profile::FromBrowserContext(browser_context()), extension())) {
    return RespondNow(Error(kExtensionDoesNotHavePermission));
  }

  auto c = base::BindOnce(&EnterprisePlatformKeysChallengeMachineKeyFunction::
                              OnChallengeAttestationOnlyKeystore,
                          this);
  GetKeystoreService(browser_context())
      ->DEPRECATED_ChallengeAttestationOnlyKeystore(
          StringFromVector(params->challenge),
          crosapi::mojom::KeystoreType::kDevice,
          /*migrate=*/params->register_key ? *params->register_key : false,
          std::move(c));
  return RespondLater();
}

void EnterprisePlatformKeysChallengeMachineKeyFunction::
    OnChallengeAttestationOnlyKeystore(
        crosapi::mojom::DEPRECATED_KeystoreStringResultPtr result) {
  if (result->is_error_message()) {
    Respond(Error(result->get_error_message()));
    return;
  }
  DCHECK(result->is_challenge_response());

  Respond(ArgumentList(api_epk::ChallengeMachineKey::Results::Create(
      VectorFromString(result->get_challenge_response()))));
}

//------------------------------------------------------------------------------

ExtensionFunction::ResponseAction
EnterprisePlatformKeysChallengeUserKeyFunction::Run() {
  std::unique_ptr<api_epk::ChallengeUserKey::Params> params(
      api_epk::ChallengeUserKey::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::string error = ValidateCrosapi(
      KeystoreService::kDEPRECATED_ChallengeAttestationOnlyKeystoreMinVersion,
      browser_context());
  if (!error.empty())
    return RespondNow(Error(error));

  if (!platform_keys::IsExtensionAllowed(
          Profile::FromBrowserContext(browser_context()), extension())) {
    return RespondNow(Error(kExtensionDoesNotHavePermission));
  }

  auto c = base::BindOnce(&EnterprisePlatformKeysChallengeUserKeyFunction::
                              OnChallengeAttestationOnlyKeystore,
                          this);
  GetKeystoreService(browser_context())
      ->DEPRECATED_ChallengeAttestationOnlyKeystore(
          StringFromVector(params->challenge),
          crosapi::mojom::KeystoreType::kUser,
          /*migrate=*/params->register_key, std::move(c));
  return RespondLater();
}

void EnterprisePlatformKeysChallengeUserKeyFunction::
    OnChallengeAttestationOnlyKeystore(
        crosapi::mojom::DEPRECATED_KeystoreStringResultPtr result) {
  if (result->is_error_message()) {
    Respond(Error(result->get_error_message()));
    return;
  }
  DCHECK(result->is_challenge_response());
  Respond(ArgumentList(api_epk::ChallengeUserKey::Results::Create(
      VectorFromString(result->get_challenge_response()))));
}

}  // namespace extensions
