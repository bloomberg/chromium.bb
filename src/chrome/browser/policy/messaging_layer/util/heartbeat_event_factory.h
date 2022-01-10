// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_HEARTBEAT_EVENT_FACTORY_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_HEARTBEAT_EVENT_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace reporting {

class HeartbeatEventFactory : public BrowserContextKeyedServiceFactory {
 public:
  static HeartbeatEventFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<HeartbeatEventFactory>;

  HeartbeatEventFactory();
  ~HeartbeatEventFactory() override;

  // BrowserContextKeyedServiceFactyory
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_HEARTBEAT_EVENT_FACTORY_H_
