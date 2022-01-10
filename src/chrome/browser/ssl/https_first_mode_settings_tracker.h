// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_HTTPS_FIRST_MODE_SETTINGS_TRACKER_H_
#define CHROME_BROWSER_SSL_HTTPS_FIRST_MODE_SETTINGS_TRACKER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;
class Profile;

// A `KeyedService` that tracks changes to the HTTPS-First Mode pref for each
// profile. This is currently only used for recording pref state in metrics and
// registering the client for a synthetic field trial based on that state.
class HttpsFirstModeService : public KeyedService {
 public:
  explicit HttpsFirstModeService(PrefService* pref_service);
  ~HttpsFirstModeService() override;

  HttpsFirstModeService(const HttpsFirstModeService&) = delete;
  HttpsFirstModeService& operator=(const HttpsFirstModeService&) = delete;

 private:
  void OnHttpsFirstModePrefChanged();

  raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
};

// Factory boilerplate for creating the `HttpsFirstModeService` for each browser
// context (profile).
class HttpsFirstModeServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static HttpsFirstModeService* GetForProfile(Profile* profile);
  static HttpsFirstModeServiceFactory* GetInstance();

  HttpsFirstModeServiceFactory(const HttpsFirstModeServiceFactory&) = delete;
  HttpsFirstModeServiceFactory& operator=(const HttpsFirstModeServiceFactory&) =
      delete;

 private:
  friend struct base::DefaultSingletonTraits<HttpsFirstModeServiceFactory>;

  HttpsFirstModeServiceFactory();
  ~HttpsFirstModeServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SSL_HTTPS_FIRST_MODE_SETTINGS_TRACKER_H_
