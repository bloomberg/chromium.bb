// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_LANGUAGE_LIST_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_LANGUAGE_LIST_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"

class GURL;

namespace translate {

struct TranslateEventDetails;
class TranslateURLFetcher;

// The TranslateLanguageList class is responsible for maintaining the latest
// supporting language list.
class TranslateLanguageList {
 public:
  TranslateLanguageList();
  virtual ~TranslateLanguageList();

  // Returns the last-updated time when the language list is fetched from the
  // Translate server. Returns null time if the list is yet to be fetched.
  base::Time last_updated() { return last_updated_; }

  // Fills |languages| with the list of languages that the translate server can
  // translate to and from. May attempt a language list request unless
  // |translate_allowed| is false.
  void GetSupportedLanguages(bool translate_allowed,
                             std::vector<std::string>* languages);

  // Returns the language code that can be used with the Translate method for a
  // specified |language|. (ex. GetLanguageCode("en-US") will return "en", and
  // GetLanguageCode("zh-CN") returns "zh-CN")
  std::string GetLanguageCode(const std::string& language);

  // Returns true if |language| is supported by the translation server.
  bool IsSupportedLanguage(const std::string& language);

  // Fetches the language list from the translate server if resource requests
  // are allowed, and otherwise keeps the request as pending until allowed.
  void RequestLanguageList();

  // Sets whether requests are allowed. If |allowed| is true, this resumes any
  // pending request.
  void SetResourceRequestsAllowed(bool allowed);

  typedef base::RepeatingCallback<void(const TranslateEventDetails&)>
      EventCallback;
  typedef base::CallbackList<void(const TranslateEventDetails&)>
      EventCallbackList;

  // Registers a callback for translate events related to the language list,
  // such as updates and download errors.
  std::unique_ptr<EventCallbackList::Subscription> RegisterEventCallback(
      const EventCallback& callback);

  // Helper methods used by specific unit tests.
  GURL LanguageFetchURLForTesting();
  bool HasOngoingLanguageListLoadingForTesting();

  // Disables the language list updater. This is used only for testing now.
  static void DisableUpdate();

  // static const values shared with our browser tests.
  static const char kTargetLanguagesKey[];

 private:
  FRIEND_TEST_ALL_PREFIXES(TranslateLanguageListTest, IsSupportedLanguage);
  FRIEND_TEST_ALL_PREFIXES(TranslateLanguageListTest, SetSupportedLanguages);
  FRIEND_TEST_ALL_PREFIXES(TranslateLanguageListTest, TranslateLanguageUrl);
  FRIEND_TEST_ALL_PREFIXES(TranslateLanguageListTest,
                           GetSupportedLanguagesFetch);

  // Callback function called when TranslateURLFetcher::Request() is finished.
  void OnLanguageListFetchComplete(bool success, const std::string& data);

  // Notifies the callback list of a translate event.
  void NotifyEvent(int line, const std::string& message);

  // Parses |language_list| containing the list of languages that the translate
  // server can translate to and from. Returns true iff the list is parsed
  // without syntax errors.
  bool SetSupportedLanguages(const std::string& language_list);

  // Returns the url from which to load the list of languages.
  GURL TranslateLanguageUrl();

  // Callbacks called on translate events.
  EventCallbackList callback_list_;

  // Whether the language list can be requested.
  bool resource_requests_allowed_;

  // True if the list has to be fetched when resource requests are allowed.
  bool request_pending_;

  // The languages supported by the translation server.
  std::set<std::string> supported_languages_;

  // A LanguageListFetcher instance to fetch a server providing supported
  // language list.
  std::unique_ptr<TranslateURLFetcher> language_list_fetcher_;

  // The last-updated time when the language list is sent.
  base::Time last_updated_;

  DISALLOW_COPY_AND_ASSIGN(TranslateLanguageList);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_LANGUAGE_LIST_H_
