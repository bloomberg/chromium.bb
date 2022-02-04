// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_TRIGGERS_MOCK_TRIGGER_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_TRIGGERS_MOCK_TRIGGER_MANAGER_H_

#include "components/safe_browsing/content/browser/triggers/trigger_manager.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace safe_browsing {

class MockTriggerManager : public TriggerManager {
 public:
  MockTriggerManager();

  MockTriggerManager(const MockTriggerManager&) = delete;
  MockTriggerManager& operator=(const MockTriggerManager&) = delete;

  ~MockTriggerManager() override;

  MOCK_METHOD8(
      StartCollectingThreatDetails,
      bool(TriggerType trigger_type,
           content::WebContents* web_contents,
           const security_interstitials::UnsafeResource& resource,
           scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
           history::HistoryService* history_service,
           base::RepeatingCallback<ChromeUserPopulation()>
               get_user_population_callback,
           ReferrerChainProvider* referrer_chain_provider,
           const SBErrorOptions& error_display_options));
  MOCK_METHOD9(
      StartCollectingThreatDetailsWithReason,
      bool(TriggerType trigger_type,
           content::WebContents* web_contents,
           const security_interstitials::UnsafeResource& resource,
           scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
           history::HistoryService* history_service,
           base::RepeatingCallback<ChromeUserPopulation()>
               get_user_population_callback,
           ReferrerChainProvider* referrer_chain_provider,
           const SBErrorOptions& error_display_options,
           TriggerManagerReason* out_reason));

  MOCK_METHOD6(FinishCollectingThreatDetails,
               bool(TriggerType trigger_type,
                    content::WebContents* web_contents,
                    const base::TimeDelta& delay,
                    bool did_proceed,
                    int num_visits,
                    const SBErrorOptions& error_display_options));
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_TRIGGERS_MOCK_TRIGGER_MANAGER_H_
