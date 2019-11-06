// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_USB_HOST_PERMISSION_MANAGER_FACTORY_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_USB_HOST_PERMISSION_MANAGER_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace arc {

class ArcUsbHostPermissionManager;

class ArcUsbHostPermissionManagerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static ArcUsbHostPermissionManager* GetForBrowserContext(
      content::BrowserContext* context);

  static ArcUsbHostPermissionManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      ArcUsbHostPermissionManagerFactory>;

  ArcUsbHostPermissionManagerFactory();
  ~ArcUsbHostPermissionManagerFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(ArcUsbHostPermissionManagerFactory);
};

}  // namespace arc

#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_USB_HOST_PERMISSION_MANAGER_FACTORY_H_
