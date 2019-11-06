// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_SERVICE_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class PushMessagingServiceImpl;

class PushMessagingServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static PushMessagingServiceImpl* GetForProfile(
      content::BrowserContext* profile);
  static PushMessagingServiceFactory* GetInstance();

  // Undo a previous call to SetTestingFactory, such that subsequent calls to
  // GetForProfile get a real push service.
  void RestoreFactoryForTests(content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<PushMessagingServiceFactory>;

  PushMessagingServiceFactory();
  ~PushMessagingServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingServiceFactory);
};

#endif  // CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_SERVICE_FACTORY_H_
