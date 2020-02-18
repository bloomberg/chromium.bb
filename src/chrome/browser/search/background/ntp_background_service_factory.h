// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_BACKGROUND_NTP_BACKGROUND_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEARCH_BACKGROUND_NTP_BACKGROUND_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class NtpBackgroundService;
class Profile;

class NtpBackgroundServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the NtpBackgroundService for |profile|.
  static NtpBackgroundService* GetForProfile(Profile* profile);

  static NtpBackgroundServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<NtpBackgroundServiceFactory>;

  NtpBackgroundServiceFactory();
  ~NtpBackgroundServiceFactory() override;

  // Overridden from BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(NtpBackgroundServiceFactory);
};

#endif  // CHROME_BROWSER_SEARCH_BACKGROUND_NTP_BACKGROUND_SERVICE_FACTORY_H_
