// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CACHED_IMAGE_FETCHER_CACHED_IMAGE_FETCHER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CACHED_IMAGE_FETCHER_CACHED_IMAGE_FETCHER_SERVICE_FACTORY_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"

class SimpleFactoryKey;
class PrefService;

namespace image_fetcher {

class CachedImageFetcherService;

// Factory to create one CachedImageFetcherService per browser context.
class CachedImageFetcherServiceFactory : public SimpleKeyedServiceFactory {
 public:
  // Return the cache path for the given profile.
  static base::FilePath GetCachePath(SimpleFactoryKey* key);

  static CachedImageFetcherService* GetForKey(SimpleFactoryKey* key,
                                              PrefService* prefs);
  static CachedImageFetcherServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<CachedImageFetcherServiceFactory>;

  CachedImageFetcherServiceFactory();
  ~CachedImageFetcherServiceFactory() override;

  // SimpleKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key,
      PrefService* prefs) const override;
  SimpleFactoryKey* GetKeyToUse(SimpleFactoryKey* key) const override;

  DISALLOW_COPY_AND_ASSIGN(CachedImageFetcherServiceFactory);
};

}  // namespace image_fetcher

#endif  // CHROME_BROWSER_CACHED_IMAGE_FETCHER_CACHED_IMAGE_FETCHER_SERVICE_FACTORY_H_
