// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONSENT_AUDITOR_CONSENT_AUDITOR_FACTORY_H_
#define CHROME_BROWSER_CONSENT_AUDITOR_CONSENT_AUDITOR_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace consent_auditor {
class ConsentAuditor;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class Profile;

class ConsentAuditorFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the singleton instance of ChromeConsentAuditorFactory.
  static ConsentAuditorFactory* GetInstance();

  // Returns the ContextAuditor associated with |profile|.
  static consent_auditor::ConsentAuditor* GetForProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<ConsentAuditorFactory>;

  ConsentAuditorFactory();
  ~ConsentAuditorFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  DISALLOW_COPY_AND_ASSIGN(ConsentAuditorFactory);
};

#endif  // CHROME_BROWSER_CONSENT_AUDITOR_CONSENT_AUDITOR_FACTORY_H_
