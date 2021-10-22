// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MULTIDEVICE_SETUP_AUTH_TOKEN_VALIDATOR_FACTORY_H_
#define CHROME_BROWSER_ASH_MULTIDEVICE_SETUP_AUTH_TOKEN_VALIDATOR_FACTORY_H_

#include "base/memory/singleton.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chromeos/services/multidevice_setup/public/cpp/auth_token_validator.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace ash {
namespace multidevice_setup {

// Owns AuthTokenValidator instances and associates them with Profiles.
class AuthTokenValidatorFactory : public BrowserContextKeyedServiceFactory {
 public:
  static AuthTokenValidator* GetForProfile(Profile* profile);

  static AuthTokenValidatorFactory* GetInstance();

  AuthTokenValidatorFactory(const AuthTokenValidatorFactory&) = delete;
  AuthTokenValidatorFactory& operator=(const AuthTokenValidatorFactory&) =
      delete;

 private:
  friend struct base::DefaultSingletonTraits<AuthTokenValidatorFactory>;

  AuthTokenValidatorFactory();
  ~AuthTokenValidatorFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace multidevice_setup
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MULTIDEVICE_SETUP_AUTH_TOKEN_VALIDATOR_FACTORY_H_
