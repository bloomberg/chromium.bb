// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_VALIDATOR_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_VALIDATOR_KEYED_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace optimization_guide {

class ModelValidatorKeyedService;

// LazyInstance that owns all ModelValidatorKeyedServices and associates them
// with Profiles.
class ModelValidatorKeyedServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Gets the LazyInstance that owns all ModelValidatorKeyedService(s).
  // Returns null if the model validation command-line flag is not specified.
  static ModelValidatorKeyedServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<ModelValidatorKeyedServiceFactory>;

  ModelValidatorKeyedServiceFactory();
  ~ModelValidatorKeyedServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace optimization_guide

#endif  //  CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_VALIDATOR_KEYED_SERVICE_FACTORY_H_
