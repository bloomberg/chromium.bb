// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_KEYED_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace media_history {

class MediaHistoryKeyedService;

class MediaHistoryKeyedServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static MediaHistoryKeyedService* GetForProfile(Profile* profile);
  static MediaHistoryKeyedServiceFactory* GetInstance();

  MediaHistoryKeyedServiceFactory(const MediaHistoryKeyedServiceFactory&) =
      delete;
  MediaHistoryKeyedServiceFactory& operator=(
      const MediaHistoryKeyedServiceFactory&) = delete;

 protected:
  bool ServiceIsCreatedWithBrowserContext() const override;

 private:
  friend struct base::DefaultSingletonTraits<MediaHistoryKeyedServiceFactory>;

  MediaHistoryKeyedServiceFactory();
  ~MediaHistoryKeyedServiceFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace media_history

#endif  // CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_KEYED_SERVICE_FACTORY_H_
