// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOCOMPLETE_HISTORY_MANAGER_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_AUTOCOMPLETE_HISTORY_MANAGER_FACTORY_H_

#include "base/compiler_specific.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

class Profile;

namespace autofill {

class AutocompleteHistoryManager;

// Singleton that owns all AutocompleteHistoryManagers and associates them with
// Profiles.
// Listens for the Profile's destruction notification and cleans up the
// associated AutocompleteHistoryManager.
class AutocompleteHistoryManagerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the AutocompleteHistoryManager for |profile|, creating it if it is
  // not yet created.
  static AutocompleteHistoryManager* GetForProfile(Profile* profile);

  static AutocompleteHistoryManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<AutocompleteHistoryManagerFactory>;

  AutocompleteHistoryManagerFactory();
  ~AutocompleteHistoryManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOCOMPLETE_HISTORY_MANAGER_FACTORY_H_
