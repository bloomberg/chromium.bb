// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_FACTORY_H_
#define CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace predictors {

class LoadingPredictor;

class LoadingPredictorFactory : public BrowserContextKeyedServiceFactory {
 public:
  static LoadingPredictor* GetForProfile(Profile* profile);
  static LoadingPredictorFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<LoadingPredictorFactory>;

  LoadingPredictorFactory();
  ~LoadingPredictorFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(LoadingPredictorFactory);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_FACTORY_H_
