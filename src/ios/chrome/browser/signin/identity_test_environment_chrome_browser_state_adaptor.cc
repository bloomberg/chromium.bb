// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/identity_test_environment_chrome_browser_state_adaptor.h"

#include <utility>

#include "base/bind.h"
#include "components/signin/core/browser/account_consistency_method.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/identity_manager_wrapper.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/signin/ios/browser/profile_oauth2_token_service_ios_delegate.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/account_tracker_service_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/signin/profile_oauth2_token_service_ios_provider_impl.h"
#include "ios/chrome/browser/signin/signin_client_factory.h"

namespace {

std::unique_ptr<KeyedService> BuildTestSigninClient(web::BrowserState* state) {
  return std::make_unique<TestSigninClient>(
      ios::ChromeBrowserState::FromBrowserState(state)->GetPrefs());
}

}  // namespace

// static
std::unique_ptr<TestChromeBrowserState>
IdentityTestEnvironmentChromeBrowserStateAdaptor::
    CreateChromeBrowserStateForIdentityTestEnvironment() {
  return CreateChromeBrowserStateForIdentityTestEnvironment(
      TestChromeBrowserState::TestingFactories());
}

// static
std::unique_ptr<TestChromeBrowserState>
IdentityTestEnvironmentChromeBrowserStateAdaptor::
    CreateChromeBrowserStateForIdentityTestEnvironment(
        const TestChromeBrowserState::TestingFactories& input_factories) {
  TestChromeBrowserState::Builder builder;

  for (auto& input_factory : input_factories) {
    builder.AddTestingFactory(input_factory.first, input_factory.second);
  }

  return CreateChromeBrowserStateForIdentityTestEnvironment(builder);
}

// static
std::unique_ptr<TestChromeBrowserState>
IdentityTestEnvironmentChromeBrowserStateAdaptor::
    CreateChromeBrowserStateForIdentityTestEnvironment(
        TestChromeBrowserState::Builder& builder,
        bool use_ios_token_service_delegate) {
  for (auto& identity_factory :
       GetIdentityTestEnvironmentFactories(use_ios_token_service_delegate)) {
    builder.AddTestingFactory(identity_factory.first, identity_factory.second);
  }

  return builder.Build();
}

// static
void IdentityTestEnvironmentChromeBrowserStateAdaptor::
    SetIdentityTestEnvironmentFactoriesOnBrowserContext(
        TestChromeBrowserState* chrome_browser_state) {
  for (const auto& factory_pair : GetIdentityTestEnvironmentFactories(
           /*use_ios_token_service_delegate=*/false)) {
    factory_pair.first->SetTestingFactory(chrome_browser_state,
                                          factory_pair.second);
  }
}

// static
void IdentityTestEnvironmentChromeBrowserStateAdaptor::
    AppendIdentityTestEnvironmentFactories(
        TestChromeBrowserState::TestingFactories* factories_to_append_to) {
  TestChromeBrowserState::TestingFactories identity_factories =
      GetIdentityTestEnvironmentFactories(
          /*use_ios_token_service_delegate=*/false);
  factories_to_append_to->insert(factories_to_append_to->end(),
                                 identity_factories.begin(),
                                 identity_factories.end());
}

// static
std::unique_ptr<KeyedService>
IdentityTestEnvironmentChromeBrowserStateAdaptor::BuildIdentityManagerForTests(
    web::BrowserState* browser_state) {
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(browser_state);

  auto fake_token_service = std::make_unique<FakeProfileOAuth2TokenService>(
      chrome_browser_state->GetPrefs());

  return identity::IdentityTestEnvironment::BuildIdentityManagerForTests(
      SigninClientFactory::GetForBrowserState(chrome_browser_state),
      chrome_browser_state->GetPrefs(), std::move(fake_token_service),
      ios::AccountTrackerServiceFactory::GetForBrowserState(
          chrome_browser_state),
      signin::AccountConsistencyMethod::kMirror);
}

// static
std::unique_ptr<KeyedService> IdentityTestEnvironmentChromeBrowserStateAdaptor::
    BuildIdentityManagerForTestWithIOSDelegate(
        web::BrowserState* browser_state) {
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(browser_state);

  std::unique_ptr<OAuth2TokenServiceDelegate> delegate =
      std::make_unique<ProfileOAuth2TokenServiceIOSDelegate>(
          SigninClientFactory::GetForBrowserState(chrome_browser_state),
          std::make_unique<ProfileOAuth2TokenServiceIOSProviderImpl>(),
          ios::AccountTrackerServiceFactory::GetForBrowserState(
              chrome_browser_state));

  auto fake_token_service = std::make_unique<FakeProfileOAuth2TokenService>(
      chrome_browser_state->GetPrefs(), std::move(delegate));

  return identity::IdentityTestEnvironment::BuildIdentityManagerForTests(
      SigninClientFactory::GetForBrowserState(chrome_browser_state),
      chrome_browser_state->GetPrefs(), std::move(fake_token_service),
      ios::AccountTrackerServiceFactory::GetForBrowserState(
          chrome_browser_state),
      signin::AccountConsistencyMethod::kMirror);
}

// static
TestChromeBrowserState::TestingFactories
IdentityTestEnvironmentChromeBrowserStateAdaptor::
    GetIdentityTestEnvironmentFactories(bool use_ios_token_service_delegate) {
  return {{SigninClientFactory::GetInstance(),
           base::BindRepeating(&BuildTestSigninClient)},
          {IdentityManagerFactory::GetInstance(),
           base::BindRepeating(use_ios_token_service_delegate
                                   ? &BuildIdentityManagerForTestWithIOSDelegate
                                   : &BuildIdentityManagerForTests)}};
}

IdentityTestEnvironmentChromeBrowserStateAdaptor::
    IdentityTestEnvironmentChromeBrowserStateAdaptor(
        ios::ChromeBrowserState* chrome_browser_state)
    : identity_test_env_(
          IdentityManagerFactory::GetForBrowserState(chrome_browser_state)) {}
