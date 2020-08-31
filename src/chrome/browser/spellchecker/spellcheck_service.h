// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_SERVICE_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_SERVICE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"
#include "chrome/browser/spellchecker/spellcheck_hunspell_dictionary.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/spellcheck/browser/platform_spell_checker.h"
#include "components/spellcheck/common/spellcheck.mojom-forward.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "mojo/public/cpp/bindings/remote.h"

class SpellCheckHostMetrics;

namespace base {
class WaitableEvent;
class SupportsUserData;
}

namespace content {
class BrowserContext;
class NotificationDetails;
class NotificationSource;
class RenderProcessHost;
}

// Encapsulates the browser side spellcheck service. There is one of these per
// profile and each is created by the SpellCheckServiceFactory.  The
// SpellcheckService maintains any per-profile information about spellcheck.
class SpellcheckService : public KeyedService,
                          public content::NotificationObserver,
                          public SpellcheckCustomDictionary::Observer,
                          public SpellcheckHunspellDictionary::Observer {
 public:
  // Event types used for reporting the status of this class and its derived
  // classes to browser tests.
  enum EventType {
    BDICT_NOTINITIALIZED,
    BDICT_CORRUPTED,
  };

  // Dictionary format used for loading an external dictionary.
  enum DictionaryFormat {
    DICT_HUNSPELL,
    DICT_TEXT,
    DICT_UNKNOWN,
  };

  // A dictionary that can be used for spellcheck.
  struct Dictionary {
    // The shortest unambiguous identifier for a language from
    // |g_supported_spellchecker_languages|. For example, "bg" for Bulgarian,
    // because Chrome has only one Bulgarian language. However, "en-US" for
    // English (United States), because Chrome has several versions of English.
    std::string language;

    // Whether |language| is currently used for spellcheck.
    bool used_for_spellcheck;
  };

  explicit SpellcheckService(content::BrowserContext* context);
  ~SpellcheckService() override;

  base::WeakPtr<SpellcheckService> GetWeakPtr();

#if !defined(OS_MACOSX)
  // Returns all currently configured |dictionaries| to display in the context
  // menu over a text area. The context menu is used for selecting the
  // dictionaries used for spellcheck.
  static void GetDictionaries(base::SupportsUserData* browser_context,
                              std::vector<Dictionary>* dictionaries);
#endif  // !OS_MACOSX

  // Signals the event attached by AttachTestEvent() to report the specified
  // event to browser tests. This function is called by this class and its
  // derived classes to report their status. This function does not do anything
  // when we do not set an event to |status_event_|.
  static bool SignalStatusEvent(EventType type);

  // Get the best match of a supported accept language code for the provided
  // language tag. Returns an empty string if no match is found. Method cannot
  // be defined in spellcheck_common.h as it depends on l10n_util, and code
  // under components cannot depend on ui/base.
  static std::string GetSupportedAcceptLanguageCode(
      const std::string& supported_language_full_tag);

  // Instantiates SpellCheckHostMetrics object and makes it ready for recording
  // metrics. This should be called only if the metrics recording is active.
  void StartRecordingMetrics(bool spellcheck_enabled);

  // Pass the renderer some basic initialization information. Note that the
  // renderer will not load Hunspell until it needs to.
  void InitForRenderer(content::RenderProcessHost* host);

  // Returns a metrics counter associated with this object,
  // or null when metrics recording is disabled.
  SpellCheckHostMetrics* GetMetrics() const;

  // Returns the instance of the custom dictionary.
  SpellcheckCustomDictionary* GetCustomDictionary();

  // Starts the process of loading the Hunspell dictionaries.
  void LoadHunspellDictionaries();

  // Returns the instance of the vector of Hunspell dictionaries.
  const std::vector<std::unique_ptr<SpellcheckHunspellDictionary>>&
  GetHunspellDictionaries();

  // Load a dictionary from a given path. Format specifies how the dictionary
  // is stored. Return value is true if successful.
  bool LoadExternalDictionary(std::string language,
                              std::string locale,
                              std::string path,
                              DictionaryFormat format);

  // Unload a dictionary. The path is given to identify the dictionary.
  // Return value is true if successful.
  bool UnloadExternalDictionary(const std::string& /* path */);

  // NotificationProfile implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // SpellcheckCustomDictionary::Observer implementation.
  void OnCustomDictionaryLoaded() override;
  void OnCustomDictionaryChanged(
      const SpellcheckCustomDictionary::Change& change) override;

  // SpellcheckHunspellDictionary::Observer implementation.
  void OnHunspellDictionaryInitialized(const std::string& language) override;
  void OnHunspellDictionaryDownloadBegin(const std::string& language) override;
  void OnHunspellDictionaryDownloadSuccess(
      const std::string& language) override;
  void OnHunspellDictionaryDownloadFailure(
      const std::string& language) override;

  // The returned pointer can be null if the current platform doesn't need a
  // per-profile, platform-specific spell check object. Currently, only Windows
  // requires one, and only on certain versions.
  PlatformSpellChecker* platform_spell_checker() {
    return platform_spell_checker_.get();
  }

  // Allows tests to override how SpellcheckService binds its interface
  // receiver, instead of going through a RenderProcessHost by default.
  using SpellCheckerBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<spellcheck::mojom::SpellChecker>)>;
  static void OverrideBinderForTesting(SpellCheckerBinder binder);

 private:
  FRIEND_TEST_ALL_PREFIXES(SpellcheckServiceBrowserTest, DeleteCorruptedBDICT);

  // Parses a full BCP47 language tag to return just the language subtag,
  // optionally with a hyphen and script subtag appended.
  static std::string GetLanguageAndScriptTag(const std::string& full_tag,
                                             bool include_script_tag);

  // Attaches an event so browser tests can listen the status events.
  static void AttachStatusEvent(base::WaitableEvent* status_event);

  // Returns the status event type.
  static EventType GetStatusEvent();

  mojo::Remote<spellcheck::mojom::SpellChecker> GetSpellCheckerForProcess(
      content::RenderProcessHost* host);

  // Pass all renderers some basic initialization information.
  void InitForAllRenderers();

  // Reacts to a change in user preference on which languages should be used for
  // spellchecking.
  void OnSpellCheckDictionariesChanged();

  // Notification handler for changes to prefs::kSpellCheckUseSpellingService.
  void OnUseSpellingServiceChanged();

  // Notification handler for changes to prefs::kAcceptLanguages. Removes from
  // prefs::kSpellCheckDictionaries any languages that are not in
  // prefs::kAcceptLanguages.
  void OnAcceptLanguagesChanged();

  // Gets the user languages from the accept_languages pref and trims them of
  // leading and trailing whitespaces. If |normalize_for_spellcheck| is |true|,
  // also normalizes the format to xx or xx-YY based on the list of spell check
  // languages supported by Hunspell. Note that if |normalize_for_spellcheck| is
  // |true|, languages not supported by Hunspell will be returned as empty
  // strings.
  std::vector<std::string> GetNormalizedAcceptLanguages(
      bool normalize_for_spellcheck = true) const;

#if defined(OS_WIN)
  // Records statistics about spell check support for the user's Chrome locales.
  void RecordChromeLocalesStats();

  // Records statistics about which spell checker supports which of the user's
  // enabled spell check locales.
  void RecordSpellcheckLocalesStats();
#endif  // defined(OS_WIN)

  // WindowsSpellChecker must be created before the dictionary instantiation and
  // destroyed after dictionary destruction.
  std::unique_ptr<PlatformSpellChecker> platform_spell_checker_;

  PrefChangeRegistrar pref_change_registrar_;
  content::NotificationRegistrar registrar_;

  // A pointer to the BrowserContext which this service refers to.
  content::BrowserContext* context_;

  std::unique_ptr<SpellCheckHostMetrics> metrics_;

  std::unique_ptr<SpellcheckCustomDictionary> custom_dictionary_;

  std::vector<std::unique_ptr<SpellcheckHunspellDictionary>>
      hunspell_dictionaries_;

  base::WeakPtrFactory<SpellcheckService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SpellcheckService);
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_SERVICE_H_
