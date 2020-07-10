// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINT_JOB_MANAGER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINT_JOB_MANAGER_FACTORY_H_

#include "base/lazy_instance.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace chromeos {

class CupsPrintJobManager;

class CupsPrintJobManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static CupsPrintJobManagerFactory* GetInstance();
  static CupsPrintJobManager* GetForBrowserContext(
      content::BrowserContext* context);

 protected:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

 private:
  friend struct base::LazyInstanceTraitsBase<CupsPrintJobManagerFactory>;

  CupsPrintJobManagerFactory();
  ~CupsPrintJobManagerFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(CupsPrintJobManagerFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINT_JOB_MANAGER_FACTORY_H_
