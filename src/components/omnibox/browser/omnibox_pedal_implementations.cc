// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_pedal_implementations.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/buildflags.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_pedal.h"
#include "components/strings/grit/components_strings.h"

#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#endif

// =============================================================================

OmniboxPedalClearBrowsingData::OmniboxPedalClearBrowsingData()
    : OmniboxPedal(
          LabelStrings(
              IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_HINT,
              IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_HINT_SHORT,
              IDS_OMNIBOX_PEDAL_CLEAR_BROWSING_DATA_SUGGESTION_CONTENTS),
          GURL("chrome://settings/clearBrowserData"),
          {
              "how to clear browsing data on chrome",
              "how to clear history",
              "how to clear history on google chrome",
              "how to clear history on chrome",
              "how to clear history in google chrome",
              "how to clear google chrome history",
              "how to clear history google chrome",
              "how to clear browsing history in chrome",
              "clear browsing data",
              "clear history",
              "clear browsing data on chrome",
              "clear history on google chrome",
              "clear history google chrome",
              "clear browsing history in chrome",
              "clear cookies chrome",
              "clear chrome history",
              "clear chrome cache",
              "history clear",
              "history clear chrome",
          },
          {
              SynonymGroup(false,
                           {
                               "google chrome",
                               "browser",
                               "chrome",
                           }),
              SynonymGroup(true,
                           {
                               "delete",
                               "remove",
                               "erase",
                               "clear",
                               "wipe",
                           }),
              SynonymGroup(true,
                           {
                               "history",
                               "cache",
                               "data",
                           }),
          }) {}

#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
const gfx::VectorIcon& OmniboxPedalClearBrowsingData::GetVectorIcon() const {
  return omnibox::kAnswerWhenIsIcon;
}
#endif

// =============================================================================

class OmniboxPedalChangeSearchEngine : public OmniboxPedal {
 public:
  OmniboxPedalChangeSearchEngine()
      : OmniboxPedal(
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CHANGE_SEARCH_ENGINE_HINT,
                IDS_OMNIBOX_PEDAL_CHANGE_SEARCH_ENGINE_HINT_SHORT,
                IDS_OMNIBOX_PEDAL_CHANGE_SEARCH_ENGINE_SUGGESTION_CONTENTS),
            GURL("chrome://settings/searchEngines"),
            {
                "how to change search engine",
                "how to change default search engine",
                "how to change default search engine in chrome",
                "how to change search engine on chrome",
                "how to set google as default search engine on chrome",
                "how to set google as default search engine in chrome",
                "how to make google default search engine",
                "how to change default search engine in google chrome",
                "change search engine",
                "change google search engine",
                "change chrome searh engine",
                "change default search engine in chrome",
                "change search engine chrome",
                "change default search chrome",
                "change search chrome",
                "switch chrome search engine",
                "switch search engine",
            },
            {
                SynonymGroup(false,
                             {
                                 "google chrome",
                                 "browser",
                                 "chrome",
                             }),
                SynonymGroup(true,
                             {
                                 "choose",
                                 "change",
                                 "switch",
                                 "select",
                             }),
                SynonymGroup(true,
                             {
                                 "standard search engine",
                                 "default search engine",
                                 "search engine",
                                 "search",
                             }),
            }) {}
};

// =============================================================================

class OmniboxPedalManagePasswords : public OmniboxPedal {
 public:
  OmniboxPedalManagePasswords()
      : OmniboxPedal(
            LabelStrings(
                IDS_OMNIBOX_PEDAL_MANAGE_PASSWORDS_HINT,
                IDS_OMNIBOX_PEDAL_MANAGE_PASSWORDS_HINT_SHORT,
                IDS_OMNIBOX_PEDAL_MANAGE_PASSWORDS_SUGGESTION_CONTENTS),
            GURL("chrome://settings/passwords"),
            {
                "passwords",
                "find my passwords",
                "save passwords in chrome",
                "view saved passwords",
                "delete passwords",
                "find saved passwords",
                "where does chrome store passwords",
                "how to see passwords in chrome",
            },
            {
                SynonymGroup(false,
                             {
                                 "google chrome",
                                 "browser",
                                 "chrome",
                             }),
                SynonymGroup(true,
                             {
                                 "manager",
                                 "manage",
                                 "update",
                                 "change",
                             }),
                SynonymGroup(true,
                             {
                                 "passwords",
                             }),
            }) {}
};

// =============================================================================

// TODO(orinj): Use better scoping for existing setting, or link to new UI.
class OmniboxPedalChangeHomePage : public OmniboxPedal {
 public:
  OmniboxPedalChangeHomePage()
      : OmniboxPedal(
            LabelStrings(
                IDS_OMNIBOX_PEDAL_CHANGE_HOME_PAGE_HINT,
                IDS_OMNIBOX_PEDAL_CHANGE_HOME_PAGE_HINT_SHORT,
                IDS_OMNIBOX_PEDAL_CHANGE_HOME_PAGE_SUGGESTION_CONTENTS),
            GURL("chrome://settings/?search=show+home+button"),
            {
                "how to change home page",
                "how to change your home page",
                "how do i change my home page",
                "change home page google",
                "home page chrome",
                "change home chrome",
                "change chrome home page",
                "how to change home page on chrome",
                "how to change home page in chrome",
                "change chrome home",
            },
            {
                SynonymGroup(false,
                             {
                                 "google chrome",
                                 "browser",
                                 "chrome",
                             }),
                SynonymGroup(true,
                             {
                                 "change",
                                 "choose",
                                 "set",
                             }),
                SynonymGroup(true,
                             {
                                 "home page",
                                 "homepage",
                             }),
            }) {}
};

// =============================================================================

class OmniboxPedalUpdateCreditCard : public OmniboxPedal {
 public:
  OmniboxPedalUpdateCreditCard()
      : OmniboxPedal(
            OmniboxPedal::LabelStrings(
                IDS_OMNIBOX_PEDAL_UPDATE_CREDIT_CARD_HINT,
                IDS_OMNIBOX_PEDAL_UPDATE_CREDIT_CARD_HINT_SHORT,
                IDS_OMNIBOX_PEDAL_UPDATE_CREDIT_CARD_SUGGESTION_CONTENTS),
            GURL("chrome://settings/autofill"),
            {
                "how to save credit card info on chrome",
                "how to remove credit card from google chrome",
                "remove google chrome credit cards",
                "access google chrome credit cards",
                "google chrome credit cards",
                "chrome credit cards",
                "get to chrome credit cards",
                "chrome credit saved",
            },
            {
                SynonymGroup(false,
                             {
                                 "google chrome",
                                 "browser",
                                 "chrome",
                             }),
                SynonymGroup(true,
                             {
                                 "update",
                             }),
                SynonymGroup(true,
                             {
                                 "credit card",
                                 "card info",
                                 "cards",
                             }),
            }) {}
};

// =============================================================================

class OmniboxPedalLaunchIncognito : public OmniboxPedal {
 public:
  OmniboxPedalLaunchIncognito()
      : OmniboxPedal(
            LabelStrings(
                IDS_OMNIBOX_PEDAL_LAUNCH_INCOGNITO_HINT,
                IDS_OMNIBOX_PEDAL_LAUNCH_INCOGNITO_HINT_SHORT,
                IDS_OMNIBOX_PEDAL_LAUNCH_INCOGNITO_SUGGESTION_CONTENTS),
            GURL(),
            {
                "what is incognito",
                "what's incognito mode",
            },
            {
                SynonymGroup(false,
                             {
                                 "google chrome",
                                 "browser",
                                 "chrome",
                             }),
                SynonymGroup(true,
                             {
                                 "launch",
                                 "start",
                                 "enter",
                                 "open",
                             }),
                SynonymGroup(true,
                             {
                                 "incognito window",
                                 "incognito mode",
                                 "private window",
                                 "incognito tab",
                                 "private mode",
                                 "dark window",
                                 "private tab",
                                 "incognito",
                                 "dark mode",
                                 "dark tab",
                             }),
            }) {}

  void Execute(ExecutionContext& context) const override {
    context.client_.NewIncognitoWindow();
  }
};

// =============================================================================

class OmniboxPedalTranslate : public OmniboxPedal {
 public:
  OmniboxPedalTranslate()
      : OmniboxPedal(
            LabelStrings(IDS_OMNIBOX_PEDAL_TRANSLATE_HINT,
                         IDS_OMNIBOX_PEDAL_TRANSLATE_HINT_SHORT,
                         IDS_OMNIBOX_PEDAL_TRANSLATE_SUGGESTION_CONTENTS),
            GURL(),
            {
                "how to change language in google chrome",
                "change language chrome",
                "change chrome language",
                "change language in chrome",
                "switch chrome language",
                "translate language",
                "translate in chrome",
                "translate on page",
                "translate language chrome",
            },
            {
                SynonymGroup(false,
                             {
                                 "google chrome",
                                 "browser",
                                 "chrome",
                             }),
                SynonymGroup(true,
                             {
                                 "change language",
                                 "translate",
                             }),
                SynonymGroup(true,
                             {
                                 "this page",
                                 "page",
                                 "this",
                             }),
            }) {}

  void Execute(ExecutionContext& context) const override {
    context.client_.PromptPageTranslation();
  }
};

// =============================================================================

OmniboxPedalUpdateChrome::OmniboxPedalUpdateChrome()
    : OmniboxPedal(
          LabelStrings(IDS_OMNIBOX_PEDAL_UPDATE_CHROME_HINT,
                       IDS_OMNIBOX_PEDAL_UPDATE_CHROME_HINT_SHORT,
                       IDS_OMNIBOX_PEDAL_UPDATE_CHROME_SUGGESTION_CONTENTS),
          GURL(),
          {
              "how to update google chrome",
              "how to update chrome",
              "how do i update google chrome",
              "how to update chrome browser",
              "update google chrome",
              "update chrome",
              "update chrome browser",
          },
          {
              SynonymGroup(true,
                           {
                               "google chrome",
                               "browser",
                               "chrome",
                           }),
              SynonymGroup(true,
                           {
                               "upgrade",
                               "install",
                               "update",
                           }),
          }) {}

void OmniboxPedalUpdateChrome::Execute(ExecutionContext& context) const {
  context.client_.OpenUpdateChromeDialog();
}

bool OmniboxPedalUpdateChrome::IsReadyToTrigger(
    const AutocompleteProviderClient& client) const {
  return client.IsBrowserUpdateAvailable();
}

// =============================================================================

std::vector<std::unique_ptr<OmniboxPedal>> GetPedalImplementations() {
  std::vector<std::unique_ptr<OmniboxPedal>> pedals;
  const auto add = [&](OmniboxPedal* pedal) {
    pedals.push_back(std::unique_ptr<OmniboxPedal>(pedal));
  };
  add(new OmniboxPedalClearBrowsingData());
  add(new OmniboxPedalChangeSearchEngine());
  add(new OmniboxPedalManagePasswords());
  add(new OmniboxPedalChangeHomePage());
  add(new OmniboxPedalUpdateCreditCard());
  add(new OmniboxPedalLaunchIncognito());
  add(new OmniboxPedalTranslate());
  add(new OmniboxPedalUpdateChrome());
  return pedals;
}
