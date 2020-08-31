// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_NOTIFICATIONS_EXTENSION_NOTIFICATION_DISPLAY_HELPER_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_NOTIFICATIONS_EXTENSION_NOTIFICATION_DISPLAY_HELPER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace extensions {

class ExtensionNotificationDisplayHelper;

class ExtensionNotificationDisplayHelperFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Get the singleton instance of the factory.
  static ExtensionNotificationDisplayHelperFactory* GetInstance();

  // Get the display helper for |profile|, creating one if needed.
  static ExtensionNotificationDisplayHelper* GetForProfile(Profile* profile);

 protected:
  // Overridden from BrowserContextKeyedServiceFactory.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

 private:
  friend struct base::DefaultSingletonTraits<
      ExtensionNotificationDisplayHelperFactory>;

  ExtensionNotificationDisplayHelperFactory();
  ~ExtensionNotificationDisplayHelperFactory() override;

  DISALLOW_COPY_AND_ASSIGN(ExtensionNotificationDisplayHelperFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_NOTIFICATIONS_EXTENSION_NOTIFICATION_DISPLAY_HELPER_FACTORY_H_
