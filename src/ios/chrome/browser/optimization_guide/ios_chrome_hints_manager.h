// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_IOS_CHROME_HINTS_MANAGER_H_
#define IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_IOS_CHROME_HINTS_MANAGER_H_

#include "components/optimization_guide/core/hints_manager.h"

namespace web {
class BrowserState;
}  // namespace web

namespace optimization_guide {

class IOSChromeHintsManager : public HintsManager {
 public:
  IOSChromeHintsManager(
      web::BrowserState* browser_state,
      PrefService* pref_service,
      base::WeakPtr<optimization_guide::OptimizationGuideStore> hint_store,
      optimization_guide::TopHostProvider* top_host_provider,
      optimization_guide::TabUrlProvider* tab_url_provider,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      network::NetworkConnectionTracker* network_connection_tracker);

  ~IOSChromeHintsManager() override = default;

  IOSChromeHintsManager(const IOSChromeHintsManager&) = delete;
  IOSChromeHintsManager& operator=(const IOSChromeHintsManager&) = delete;
};

}  // namespace optimization_guide

#endif  // IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_IOS_CHROME_HINTS_MANAGER_H_
