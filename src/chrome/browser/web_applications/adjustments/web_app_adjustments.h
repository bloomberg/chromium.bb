// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ADJUSTMENTS_WEB_APP_ADJUSTMENTS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ADJUSTMENTS_WEB_APP_ADJUSTMENTS_H_

#include <memory>

#include "chrome/browser/web_applications/adjustments/link_capturing_pref_migration.h"
#include "chrome/browser/web_applications/adjustments/preinstalled_web_app_duplication_fixer.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

class Profile;

namespace web_app {

// A container class for temporary migrations, fix ups and clean ups dealing
// historical code that's left its mark on the user's database/prefs.
// Everything in here should have a removal date.
class WebAppAdjustments : public KeyedService {
 public:
  explicit WebAppAdjustments(Profile* profile);
  ~WebAppAdjustments() override;

  PreinstalledWebAppDuplicationFixer* preinstalled_web_app_duplication_fixer() {
    return preinstalled_web_app_duplication_fixer_.get();
  }

 private:
  // TODO(crbug.com/1262906): This was added in M97, remove in M107.
  std::unique_ptr<LinkCapturingPrefMigration> link_capturing_pref_migration_;

  // TODO(crbug.com/1290716): This was added in M100, remove in M120.
  std::unique_ptr<PreinstalledWebAppDuplicationFixer>
      preinstalled_web_app_duplication_fixer_;
};

class WebAppAdjustmentsFactory : public BrowserContextKeyedServiceFactory {
 public:
  WebAppAdjustmentsFactory();
  ~WebAppAdjustmentsFactory() override;

  static WebAppAdjustmentsFactory* GetInstance();
  WebAppAdjustments* Get(Profile* profile);

 private:
  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ADJUSTMENTS_WEB_APP_ADJUSTMENTS_H_
