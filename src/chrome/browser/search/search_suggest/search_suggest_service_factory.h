// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SEARCH_SUGGEST_SEARCH_SUGGEST_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEARCH_SEARCH_SUGGEST_SEARCH_SUGGEST_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class SearchSuggestService;
class Profile;

class SearchSuggestServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the SearchSuggestService for |profile|.
  static SearchSuggestService* GetForProfile(Profile* profile);

  static SearchSuggestServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<SearchSuggestServiceFactory>;

  SearchSuggestServiceFactory();
  ~SearchSuggestServiceFactory() override;

  // Overridden from BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(SearchSuggestServiceFactory);
};

#endif  // CHROME_BROWSER_SEARCH_SEARCH_SUGGEST_SEARCH_SUGGEST_SERVICE_FACTORY_H_
