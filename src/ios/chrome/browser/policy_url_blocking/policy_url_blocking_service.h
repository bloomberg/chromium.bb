// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_URL_BLOCKING_POLICY_URL_BLOCKING_SERVICE_H_
#define IOS_CHROME_BROWSER_POLICY_URL_BLOCKING_POLICY_URL_BLOCKING_SERVICE_H_

#include "base/no_destructor.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "components/policy/core/browser/url_blacklist_manager.h"

namespace web {
class BrowserState;
}

// Associates a policy::URLBlacklistManager instance with a BrowserState.
class PolicyBlocklistService : public KeyedService {
 public:
  explicit PolicyBlocklistService(
      web::BrowserState* browser_state,
      std::unique_ptr<policy::URLBlacklistManager> url_blacklist_manager);
  ~PolicyBlocklistService() override;

  // Returns the blocking state for |url|.
  policy::URLBlacklist::URLBlacklistState GetURLBlocklistState(
      const GURL& url) const;

 private:
  // The URLBlacklistManager associated with |browser_state|.
  std::unique_ptr<policy::URLBlacklistManager> url_blacklist_manager_;

  PolicyBlocklistService(const PolicyBlocklistService&) = delete;
  PolicyBlocklistService& operator=(const PolicyBlocklistService&) = delete;
};

class PolicyBlocklistServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static PolicyBlocklistServiceFactory* GetInstance();
  static PolicyBlocklistService* GetForBrowserState(
      web::BrowserState* browser_state);

 private:
  friend class base::NoDestructor<PolicyBlocklistServiceFactory>;

  PolicyBlocklistServiceFactory();
  ~PolicyBlocklistServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* browser_state) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* browser_state) const override;

  PolicyBlocklistServiceFactory(const PolicyBlocklistServiceFactory&) = delete;
  PolicyBlocklistServiceFactory& operator=(
      const PolicyBlocklistServiceFactory&) = delete;
};

#endif  // IOS_CHROME_BROWSER_POLICY_URL_BLOCKING_POLICY_URL_BLOCKING_SERVICE_H_
