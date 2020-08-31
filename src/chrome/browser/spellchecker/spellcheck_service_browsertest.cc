// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_service.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spell_check_host_chrome_impl.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

#if BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
#include "components/spellcheck/common/spellcheck_features.h"
#endif  // BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)

using content::BrowserContext;
using content::RenderProcessHost;

class SpellcheckServiceBrowserTest : public InProcessBrowserTest,
                                     public spellcheck::mojom::SpellChecker {
 public:
  SpellcheckServiceBrowserTest() = default;

  void SetUpOnMainThread() override {
    renderer_.reset(new content::MockRenderProcessHost(GetContext()));
    renderer_->Init();
    prefs_ = user_prefs::UserPrefs::Get(GetContext());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sync causes the SpellcheckService to be instantiated (and initialized)
    // during startup. However, several tests rely on control over when exactly
    // the SpellcheckService gets created (e.g. by calling
    // GetEnableSpellcheckState() after InitSpellcheck(), which will wait
    // forever if the service already existed). So disable sync of the custom
    // dictionary for these tests.
    command_line->AppendSwitchASCII(switches::kDisableSyncTypes, "Dictionary");
    SpellcheckService::OverrideBinderForTesting(base::BindRepeating(
        &SpellcheckServiceBrowserTest::Bind, base::Unretained(this)));
  }

  void TearDownOnMainThread() override {
    SpellcheckService::OverrideBinderForTesting(base::NullCallback());
    receiver_.reset();
    prefs_ = nullptr;
    renderer_.reset();
  }

  RenderProcessHost* GetRenderer() const { return renderer_.get(); }

  BrowserContext* GetContext() const {
    return static_cast<BrowserContext*>(browser()->profile());
  }

  PrefService* GetPrefs() const { return prefs_; }

  void InitSpellcheck(bool enable_spellcheck,
                      const std::string& single_dictionary,
                      const std::string& multiple_dictionaries) {
    prefs_->SetBoolean(spellcheck::prefs::kSpellCheckEnable, enable_spellcheck);
    prefs_->SetString(spellcheck::prefs::kSpellCheckDictionary,
                      single_dictionary);
    base::ListValue dictionaries_value;
    dictionaries_value.AppendStrings(
        base::SplitString(multiple_dictionaries, ",", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY));
    prefs_->Set(spellcheck::prefs::kSpellCheckDictionaries, dictionaries_value);

    SpellcheckService* spellcheck =
        SpellcheckServiceFactory::GetForContext(renderer_->GetBrowserContext());

#if BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
    if (spellcheck::UseWinHybridSpellChecker()) {
      // If the Windows hybrid spell checker is in use, initialization is async.
      RunTestRunLoop();
    }
#endif  // BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)

    ASSERT_NE(nullptr, spellcheck);
  }

  void EnableSpellcheck(bool enable_spellcheck) {
    prefs_->SetBoolean(spellcheck::prefs::kSpellCheckEnable, enable_spellcheck);
  }

  void ChangeCustomDictionary() {
    SpellcheckService* spellcheck =
        SpellcheckServiceFactory::GetForContext(renderer_->GetBrowserContext());
    ASSERT_NE(nullptr, spellcheck);

    SpellcheckCustomDictionary::Change change;
    change.RemoveWord("1");
    change.AddWord("2");
    change.AddWord("3");

    spellcheck->OnCustomDictionaryChanged(change);
  }

  void SetSingleLanguageDictionary(const std::string& single_dictionary) {
    prefs_->SetString(spellcheck::prefs::kSpellCheckDictionary,
                      single_dictionary);
  }

  void SetMultiLingualDictionaries(const std::string& multiple_dictionaries) {
    base::ListValue dictionaries_value;
    dictionaries_value.AppendStrings(
        base::SplitString(multiple_dictionaries, ",", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY));
    prefs_->Set(spellcheck::prefs::kSpellCheckDictionaries, dictionaries_value);
  }

  std::string GetMultilingualDictionaries() {
    const base::ListValue* list_value =
        prefs_->GetList(spellcheck::prefs::kSpellCheckDictionaries);
    std::vector<base::StringPiece> dictionaries;
    for (const auto& item_value : *list_value) {
      base::StringPiece dictionary;
      EXPECT_TRUE(item_value.GetAsString(&dictionary));
      dictionaries.push_back(dictionary);
    }
    return base::JoinString(dictionaries, ",");
  }

  void SetAcceptLanguages(const std::string& accept_languages) {
    prefs_->SetString(language::prefs::kAcceptLanguages, accept_languages);
  }

  bool GetEnableSpellcheckState(bool initial_state = false) {
    spellcheck_enabled_state_ = initial_state;
    RunTestRunLoop();
    EXPECT_TRUE(initialize_spellcheck_called_);
    EXPECT_TRUE(bound_connection_closed_);
    return spellcheck_enabled_state_;
  }

  bool GetCustomDictionaryChangedState() {
    RunTestRunLoop();
    EXPECT_TRUE(bound_connection_closed_);
    return custom_dictionary_changed_called_;
  }

 private:
  // Spins a RunLoop to deliver the Mojo SpellChecker request flow.
  void RunTestRunLoop() {
    bound_connection_closed_ = false;
    initialize_spellcheck_called_ = false;
    custom_dictionary_changed_called_ = false;

    base::RunLoop run_loop;
    quit_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // Binds requests for the SpellChecker interface.
  void Bind(mojo::PendingReceiver<spellcheck::mojom::SpellChecker> receiver) {
    receiver_.reset();
    receiver_.Bind(std::move(receiver));
    receiver_.set_disconnect_handler(
        base::BindOnce(&SpellcheckServiceBrowserTest::BoundConnectionClosed,
                       base::Unretained(this)));
  }

  // The requester closes (disconnects) when done.
  void BoundConnectionClosed() {
    bound_connection_closed_ = true;
    receiver_.reset();
    if (quit_)
      std::move(quit_).Run();
  }

  // spellcheck::mojom::SpellChecker:
  void Initialize(
      std::vector<spellcheck::mojom::SpellCheckBDictLanguagePtr> dictionaries,
      const std::vector<std::string>& custom_words,
      bool enable) override {
    initialize_spellcheck_called_ = true;
    spellcheck_enabled_state_ = enable;
  }

  void CustomDictionaryChanged(
      const std::vector<std::string>& words_added,
      const std::vector<std::string>& words_removed) override {
    custom_dictionary_changed_called_ = true;
    EXPECT_EQ(1u, words_removed.size());
    EXPECT_EQ(2u, words_added.size());
  }

 protected:
  // Quits the RunLoop on Mojo request flow completion.
  base::OnceClosure quit_;

 private:
  // Mocked RenderProcessHost.
  std::unique_ptr<content::MockRenderProcessHost> renderer_;

  // Not owned preferences service.
  PrefService* prefs_;

  // Binding to receive the SpellChecker request flow.
  mojo::Receiver<spellcheck::mojom::SpellChecker> receiver_{this};

  // Used to verify the SpellChecker request flow.
  bool bound_connection_closed_;
  bool custom_dictionary_changed_called_;
  bool initialize_spellcheck_called_;
  bool spellcheck_enabled_state_;

  DISALLOW_COPY_AND_ASSIGN(SpellcheckServiceBrowserTest);
};

class SpellcheckServiceHostBrowserTest : public SpellcheckServiceBrowserTest {
 public:
  SpellcheckServiceHostBrowserTest() = default;

  void RequestDictionary() {
    mojo::Remote<spellcheck::mojom::SpellCheckHost> interface;
    RequestSpellCheckHost(&interface);

    interface->RequestDictionary();
  }

  void NotifyChecked() {
    mojo::Remote<spellcheck::mojom::SpellCheckHost> interface;
    RequestSpellCheckHost(&interface);

    const bool misspelt = true;
    base::UTF8ToUTF16("hallo", 5, &word_);
    interface->NotifyChecked(word_, misspelt);
    base::RunLoop().RunUntilIdle();
  }

  void CallSpellingService() {
    mojo::Remote<spellcheck::mojom::SpellCheckHost> interface;
    RequestSpellCheckHost(&interface);

    base::UTF8ToUTF16("hello", 5, &word_);
    interface->CallSpellingService(
        word_,
        base::BindOnce(&SpellcheckServiceHostBrowserTest::SpellingServiceDone,
                       base::Unretained(this)));

    base::RunLoop run_loop;
    quit_ = run_loop.QuitClosure();
    run_loop.Run();

    EXPECT_TRUE(spelling_service_done_called_);
  }

 private:
  void RequestSpellCheckHost(
      mojo::Remote<spellcheck::mojom::SpellCheckHost>* interface) {
    SpellCheckHostChromeImpl::Create(GetRenderer()->GetID(),
                                     interface->BindNewPipeAndPassReceiver());
  }

  void SpellingServiceDone(bool success,
                           const std::vector<::SpellCheckResult>& results) {
    spelling_service_done_called_ = true;
    if (quit_)
      std::move(quit_).Run();
  }

  bool spelling_service_done_called_ = false;
  base::string16 word_;

  DISALLOW_COPY_AND_ASSIGN(SpellcheckServiceHostBrowserTest);
};

// Removing a spellcheck language from accept languages should remove it from
// spellcheck languages list as well.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       RemoveSpellcheckLanguageFromAcceptLanguages) {
  InitSpellcheck(true, "", "en-US,fr");
  SetAcceptLanguages("en-US,es,ru");
  EXPECT_EQ("en-US", GetMultilingualDictionaries());
}

// Keeping spellcheck languages in accept languages should not alter spellcheck
// languages list.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       KeepSpellcheckLanguagesInAcceptLanguages) {
  InitSpellcheck(true, "", "en-US,fr");
  SetAcceptLanguages("en-US,fr,es");
  EXPECT_EQ("en-US,fr", GetMultilingualDictionaries());
}

// Starting with spellcheck enabled should send the 'enable spellcheck' message
// to the renderer. Consequently disabling spellcheck should send the 'disable
// spellcheck' message to the renderer.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest, StartWithSpellcheck) {
  InitSpellcheck(true, "", "en-US,fr");
  EXPECT_TRUE(GetEnableSpellcheckState());

  EnableSpellcheck(false);
  EXPECT_FALSE(GetEnableSpellcheckState(true));
}

// Starting with only a single-language spellcheck setting should send the
// 'enable spellcheck' message to the renderer. Consequently removing spellcheck
// languages should disable spellcheck.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       StartWithSingularLanguagePreference) {
  InitSpellcheck(true, "en-US", "");
  EXPECT_TRUE(GetEnableSpellcheckState());

  SetMultiLingualDictionaries("");
  EXPECT_FALSE(GetEnableSpellcheckState(true));
}

// Starting with a multi-language spellcheck setting should send the 'enable
// spellcheck' message to the renderer. Consequently removing spellcheck
// languages should disable spellcheck.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       StartWithMultiLanguagePreference) {
  InitSpellcheck(true, "", "en-US,fr");
  EXPECT_TRUE(GetEnableSpellcheckState());

  SetMultiLingualDictionaries("");
  EXPECT_FALSE(GetEnableSpellcheckState(true));
}

// Starting with both single-language and multi-language spellcheck settings
// should send the 'enable spellcheck' message to the renderer. Consequently
// removing spellcheck languages should disable spellcheck.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       StartWithBothLanguagePreferences) {
  InitSpellcheck(true, "en-US", "en-US,fr");
  EXPECT_TRUE(GetEnableSpellcheckState());

  SetMultiLingualDictionaries("");
  EXPECT_FALSE(GetEnableSpellcheckState(true));
}

// Starting without spellcheck languages should send the 'disable spellcheck'
// message to the renderer. Consequently adding spellchecking languages should
// enable spellcheck.
// Flaky, see https://crbug.com/600153
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       DISABLED_StartWithoutLanguages) {
  InitSpellcheck(true, "", "");
  EXPECT_FALSE(GetEnableSpellcheckState(true));

  SetMultiLingualDictionaries("en-US");
  EXPECT_TRUE(GetEnableSpellcheckState());
}

// Starting with spellcheck disabled should send the 'disable spellcheck'
// message to the renderer. Consequently enabling spellcheck should send the
// 'enable spellcheck' message to the renderer.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest, StartWithoutSpellcheck) {
  InitSpellcheck(false, "", "en-US,fr");
  EXPECT_FALSE(GetEnableSpellcheckState(true));

  EnableSpellcheck(true);
  EXPECT_TRUE(GetEnableSpellcheckState());
}

// A custom dictionary state change should send a 'custom dictionary changed'
// message to the renderer, regardless of the spellcheck enabled state.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest, CustomDictionaryChanged) {
  InitSpellcheck(true, "en-US", "");
  EXPECT_TRUE(GetEnableSpellcheckState());

  ChangeCustomDictionary();
  EXPECT_TRUE(GetCustomDictionaryChangedState());

  EnableSpellcheck(false);
  EXPECT_FALSE(GetEnableSpellcheckState(true));

  ChangeCustomDictionary();
  EXPECT_TRUE(GetCustomDictionaryChangedState());
}

// Regression test for https://crbug.com/854540.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       CustomDictionaryChangedAfterRendererCrash) {
  InitSpellcheck(true, "en-US", "");
  EXPECT_TRUE(GetEnableSpellcheckState());

  // Kill the renderer process.
  content::RenderProcessHost* process = browser()
                                            ->tab_strip_model()
                                            ->GetActiveWebContents()
                                            ->GetMainFrame()
                                            ->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(process->Shutdown(0));
  crash_observer.Wait();

  // Change the custom dictionary - the test passes if there were no crashes or
  // hangs.
  ChangeCustomDictionary();
}

// Starting with only a single-language spellcheck setting, the host should
// initialize the renderer's spellcheck system, and the same if the renderer
// explicity requests the spellcheck dictionaries.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceHostBrowserTest, RequestDictionary) {
  InitSpellcheck(true, "en-US", "");
  EXPECT_TRUE(GetEnableSpellcheckState());

  RequestDictionary();
  EXPECT_TRUE(GetEnableSpellcheckState());
}

// When the renderer notifies that it corrected a word, the render process
// host should record UMA stats about the correction.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceHostBrowserTest, NotifyChecked) {
  const char kMisspellRatio[] = "SpellCheck.MisspellRatio";

  base::HistogramTester tester;
  tester.ExpectTotalCount(kMisspellRatio, 0);
  NotifyChecked();
  tester.ExpectTotalCount(kMisspellRatio, 1);
}

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
// When the renderer requests the spelling service for correcting text, the
// render process host should call the remote spelling service.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceHostBrowserTest, CallSpellingService) {
  CallSpellingService();
}
#endif  // BUILDFLAG(USE_RENDERER_SPELLCHECKER)

// Tests that we can delete a corrupted BDICT file used by hunspell. We do not
// run this test on Mac because Mac does not use hunspell by default.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest, DeleteCorruptedBDICT) {
#if BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)
  if (spellcheck::UseWinHybridSpellChecker()) {
    // If doing hybrid spell checking on Windows, Hunspell dictionaries are not
    // used for en-US, so the corrupt dictionary event will never be raised.
    // Skip this test.
    return;
  }
#endif  // BUILDFLAG(USE_WIN_HYBRID_SPELLCHECKER)

  // Corrupted BDICT data: please do not use this BDICT data for other tests.
  const uint8_t kCorruptedBDICT[] = {
      0x42, 0x44, 0x69, 0x63, 0x02, 0x00, 0x01, 0x00, 0x20, 0x00, 0x00, 0x00,
      0x3b, 0x00, 0x00, 0x00, 0x65, 0x72, 0xe0, 0xac, 0x27, 0xc7, 0xda, 0x66,
      0x6d, 0x1e, 0xa6, 0x35, 0xd1, 0xf6, 0xb7, 0x35, 0x32, 0x00, 0x00, 0x00,
      0x38, 0x00, 0x00, 0x00, 0x39, 0x00, 0x00, 0x00, 0x3a, 0x00, 0x00, 0x00,
      0x0a, 0x0a, 0x41, 0x46, 0x20, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe6,
      0x49, 0x00, 0x68, 0x02, 0x73, 0x06, 0x74, 0x0b, 0x77, 0x11, 0x79, 0x15,
  };

  // Write the corrupted BDICT data to create a corrupted BDICT file.
  base::FilePath dict_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_APP_DICTIONARIES, &dict_dir));
  base::FilePath bdict_path =
      spellcheck::GetVersionedFileName("en-US", dict_dir);

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    size_t actual = base::WriteFile(
        bdict_path, reinterpret_cast<const char*>(kCorruptedBDICT),
        base::size(kCorruptedBDICT));
    EXPECT_EQ(base::size(kCorruptedBDICT), actual);
  }

  // Attach an event to the SpellcheckService object so we can receive its
  // status updates.
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  SpellcheckService::AttachStatusEvent(&event);

  BrowserContext* context = GetContext();

  // Ensure that the SpellcheckService object does not already exist. Otherwise
  // the next line will not force creation of the SpellcheckService and the
  // test will fail.
  SpellcheckService* service = static_cast<SpellcheckService*>(
      SpellcheckServiceFactory::GetInstance()->GetServiceForBrowserContext(
          context,
          false));
  ASSERT_EQ(NULL, service);

  // Getting the spellcheck_service will initialize the SpellcheckService
  // object with the corrupted BDICT file created above since the hunspell
  // dictionary is loaded in the SpellcheckService constructor right now.
  // The SpellCheckHost object will send a BDICT_CORRUPTED event.
  SpellcheckServiceFactory::GetForContext(context);

  // Check the received event. Also we check if Chrome has successfully deleted
  // the corrupted dictionary. We delete the corrupted dictionary to avoid
  // leaking it when this test fails.
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(SpellcheckService::BDICT_CORRUPTED,
            SpellcheckService::GetStatusEvent());
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (base::PathExists(bdict_path)) {
    ADD_FAILURE();
    EXPECT_TRUE(base::DeleteFileRecursively(bdict_path));
  }
}

// Checks that preferences migrate correctly.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest, PreferencesMigrated) {
  GetPrefs()->Set(spellcheck::prefs::kSpellCheckDictionaries,
                  base::ListValue());
  GetPrefs()->SetString(spellcheck::prefs::kSpellCheckDictionary, "en-US");

  // Create a SpellcheckService which will migrate the preferences.
  SpellcheckServiceFactory::GetForContext(GetContext());

  // Make sure the preferences have been migrated.
  std::string new_pref;
  EXPECT_TRUE(GetPrefs()
                  ->GetList(spellcheck::prefs::kSpellCheckDictionaries)
                  ->GetString(0, &new_pref));
  EXPECT_EQ("en-US", new_pref);
  EXPECT_TRUE(
      GetPrefs()->GetString(spellcheck::prefs::kSpellCheckDictionary).empty());
}

// Checks that preferences are not migrated when they shouldn't be.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest, PreferencesNotMigrated) {
  base::ListValue dictionaries;
  dictionaries.AppendString("en-US");
  GetPrefs()->Set(spellcheck::prefs::kSpellCheckDictionaries, dictionaries);
  GetPrefs()->SetString(spellcheck::prefs::kSpellCheckDictionary, "fr");

  // Create a SpellcheckService which will migrate the preferences.
  SpellcheckServiceFactory::GetForContext(GetContext());

  // Make sure the preferences have not been migrated.
  std::string new_pref;
  EXPECT_TRUE(GetPrefs()
                  ->GetList(spellcheck::prefs::kSpellCheckDictionaries)
                  ->GetString(0, &new_pref));
  EXPECT_EQ("en-US", new_pref);
  EXPECT_TRUE(
      GetPrefs()->GetString(spellcheck::prefs::kSpellCheckDictionary).empty());
}

// Checks that, if a user has spellchecking disabled, nothing changes
// during migration.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       SpellcheckingDisabledPreferenceMigration) {
  base::ListValue dictionaries;
  dictionaries.AppendString("en-US");
  GetPrefs()->Set(spellcheck::prefs::kSpellCheckDictionaries, dictionaries);
  GetPrefs()->SetBoolean(spellcheck::prefs::kSpellCheckEnable, false);

  // Migrate the preferences.
  SpellcheckServiceFactory::GetForContext(GetContext());

  EXPECT_FALSE(GetPrefs()->GetBoolean(spellcheck::prefs::kSpellCheckEnable));
  EXPECT_EQ(1U, GetPrefs()
                    ->GetList(spellcheck::prefs::kSpellCheckDictionaries)
                    ->GetSize());
}

// Make sure preferences get preserved and spellchecking stays enabled.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       MultilingualPreferenceNotMigrated) {
  base::ListValue dictionaries;
  dictionaries.AppendString("en-US");
  dictionaries.AppendString("fr");
  GetPrefs()->Set(spellcheck::prefs::kSpellCheckDictionaries, dictionaries);
  GetPrefs()->SetBoolean(spellcheck::prefs::kSpellCheckEnable, true);

  // Should not migrate any preferences.
  SpellcheckServiceFactory::GetForContext(GetContext());

  EXPECT_TRUE(GetPrefs()->GetBoolean(spellcheck::prefs::kSpellCheckEnable));
  EXPECT_EQ(2U, GetPrefs()
                    ->GetList(spellcheck::prefs::kSpellCheckDictionaries)
                    ->GetSize());
  std::string pref;
  ASSERT_TRUE(GetPrefs()
                  ->GetList(spellcheck::prefs::kSpellCheckDictionaries)
                  ->GetString(0, &pref));
  EXPECT_EQ("en-US", pref);
  ASSERT_TRUE(GetPrefs()
                  ->GetList(spellcheck::prefs::kSpellCheckDictionaries)
                  ->GetString(1, &pref));
  EXPECT_EQ("fr", pref);
}
