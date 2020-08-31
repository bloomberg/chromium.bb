// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_STATEFUL_SSL_HOST_STATE_DELEGATE_FACTORY_H_
#define CHROME_BROWSER_SSL_STATEFUL_SSL_HOST_STATE_DELEGATE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/values.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/prefs/pref_service.h"

class StatefulSSLHostStateDelegate;
class Profile;

// Singleton that associates all StatefulSSLHostStateDelegates with
// Profiles.
class StatefulSSLHostStateDelegateFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static StatefulSSLHostStateDelegate* GetForProfile(Profile* profile);

  static StatefulSSLHostStateDelegateFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      StatefulSSLHostStateDelegateFactory>;

  StatefulSSLHostStateDelegateFactory();
  ~StatefulSSLHostStateDelegateFactory() override;

  // BrowserContextKeyedServiceFactory methods:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(StatefulSSLHostStateDelegateFactory);
};

#endif  // CHROME_BROWSER_SSL_STATEFUL_SSL_HOST_STATE_DELEGATE_FACTORY_H_
