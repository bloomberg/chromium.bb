// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/accessibility_handler.h"

#include <set>

#include "ash/constants/ash_pref_names.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/dictation.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace settings {
namespace {

using ::ash::AccessibilityManager;

void RecordShowShelfNavigationButtonsValueChange(bool enabled) {
  base::UmaHistogramBoolean(
      "Accessibility.CrosShelfNavigationButtonsInTabletModeChanged."
      "OsSettings",
      enabled);
}

}  // namespace

AccessibilityHandler::AccessibilityHandler(Profile* profile)
    : profile_(profile) {}

AccessibilityHandler::~AccessibilityHandler() {
  if (a11y_nav_buttons_toggle_metrics_reporter_timer_.IsRunning())
    a11y_nav_buttons_toggle_metrics_reporter_timer_.FireNow();
}

void AccessibilityHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "showChromeVoxSettings",
      base::BindRepeating(&AccessibilityHandler::HandleShowChromeVoxSettings,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "showSelectToSpeakSettings",
      base::BindRepeating(
          &AccessibilityHandler::HandleShowSelectToSpeakSettings,
          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "setStartupSoundEnabled",
      base::BindRepeating(&AccessibilityHandler::HandleSetStartupSoundEnabled,
                          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "recordSelectedShowShelfNavigationButtonValue",
      base::BindRepeating(
          &AccessibilityHandler::
              HandleRecordSelectedShowShelfNavigationButtonsValue,
          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "manageA11yPageReady",
      base::BindRepeating(&AccessibilityHandler::HandleManageA11yPageReady,
                          base::Unretained(this)));
  web_ui()->RegisterDeprecatedMessageCallback(
      "showChromeVoxTutorial",
      base::BindRepeating(&AccessibilityHandler::HandleShowChromeVoxTutorial,
                          base::Unretained(this)));
}

void AccessibilityHandler::HandleShowChromeVoxSettings(
    const base::ListValue* args) {
  OpenExtensionOptionsPage(extension_misc::kChromeVoxExtensionId);
}

void AccessibilityHandler::HandleShowSelectToSpeakSettings(
    const base::ListValue* args) {
  OpenExtensionOptionsPage(extension_misc::kSelectToSpeakExtensionId);
}

void AccessibilityHandler::HandleSetStartupSoundEnabled(
    const base::ListValue* args) {
  DCHECK_EQ(1U, args->GetList().size());
  bool enabled = false;
  if (args->GetList()[0].is_bool())
    enabled = args->GetList()[0].GetBool();
  AccessibilityManager::Get()->SetStartupSoundEnabled(enabled);
}

void AccessibilityHandler::HandleRecordSelectedShowShelfNavigationButtonsValue(
    const base::ListValue* args) {
  DCHECK_EQ(1U, args->GetList().size());
  bool enabled = false;
  if (args->GetList()[0].is_bool())
    enabled = args->GetList()[0].GetBool();

  a11y_nav_buttons_toggle_metrics_reporter_timer_.Start(
      FROM_HERE, base::Seconds(10),
      base::BindOnce(&RecordShowShelfNavigationButtonsValueChange, enabled));
}

void AccessibilityHandler::HandleManageA11yPageReady(
    const base::ListValue* args) {
  AllowJavascript();
}

void AccessibilityHandler::OnJavascriptAllowed() {
  FireWebUIListener(
      "initial-data-ready",
      base::Value(AccessibilityManager::Get()->GetStartupSoundEnabled()));
  MaybeAddSodaInstallerObserver();
  MaybeAddDictationLocales();
}

void AccessibilityHandler::OnJavascriptDisallowed() {
  if (features::IsDictationOfflineAvailableAndEnabled())
    soda_observation_.Reset();
}

void AccessibilityHandler::HandleShowChromeVoxTutorial(
    const base::ListValue* args) {
  AccessibilityManager::Get()->ShowChromeVoxTutorial();
}

void AccessibilityHandler::OpenExtensionOptionsPage(const char extension_id[]) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetExtensionById(
          extension_id, extensions::ExtensionRegistry::ENABLED);
  if (!extension)
    return;
  extensions::ExtensionTabUtil::OpenOptionsPage(
      extension,
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents()));
}

void AccessibilityHandler::MaybeAddSodaInstallerObserver() {
  if (!features::IsDictationOfflineAvailableAndEnabled())
    return;

  speech::SodaInstaller* soda_installer = speech::SodaInstaller::GetInstance();
  if (!soda_installer->IsSodaInstalled(GetDictationLocale())) {
    // Add self as an observer. If this was a page refresh we don't want to
    // get added twice.
    soda_observation_.Observe(soda_installer);
  }
}

void AccessibilityHandler::OnSodaInstallSucceeded() {
  if (!speech::SodaInstaller::GetInstance()->IsSodaInstalled(
          GetDictationLocale())) {
    return;
  }

  // Only show the success message if both the SODA binary and the language pack
  // matching the Dictation locale have been downloaded.
  FireWebUIListener(
      "dictation-locale-menu-subtitle-changed",
      base::Value(l10n_util::GetStringFUTF16(
          IDS_SETTINGS_ACCESSIBILITY_DICTATION_LOCALE_SUB_LABEL_OFFLINE,
          GetDictationLocaleDisplayName())));
}

void AccessibilityHandler::OnSodaInstallProgress(
    int progress,
    speech::LanguageCode language_code) {
  // TODO(https://crbug.com/1266491): Ensure we use combined progress instead
  // of just the language pack progress.
  if (language_code != GetDictationLocale())
    return;

  // Only show the progress message if this applies to the language pack
  // matching the Dictation locale.
  FireWebUIListener(
      "dictation-locale-menu-subtitle-changed",
      base::Value(l10n_util::GetStringFUTF16Int(
          IDS_SETTINGS_ACCESSIBILITY_DICTATION_SUBTITLE_SODA_DOWNLOAD_PROGRESS,
          progress)));
}

void AccessibilityHandler::OnSodaInstallFailed(
    speech::LanguageCode language_code) {
  if (language_code == speech::LanguageCode::kNone ||
      language_code == GetDictationLocale()) {
    // Show the failed message if either the Dictation locale failed or the SODA
    // binary failed (encoded by LanguageCode::kNone).
    FireWebUIListener(
        "dictation-locale-menu-subtitle-changed",
        base::Value(l10n_util::GetStringFUTF16(
            IDS_SETTINGS_ACCESSIBILITY_DICTATION_SUBTITLE_SODA_DOWNLOAD_ERROR,
            GetDictationLocaleDisplayName())));
  }
}

// SodaInstaller::Observer:
void AccessibilityHandler::OnSodaInstalled() {
  OnSodaInstallSucceeded();
}

void AccessibilityHandler::OnSodaLanguagePackInstalled(
    speech::LanguageCode language_code) {
  OnSodaInstallSucceeded();
}

void AccessibilityHandler::OnSodaLanguagePackProgress(
    int language_progress,
    speech::LanguageCode language_code) {
  OnSodaInstallProgress(language_progress, language_code);
}

void AccessibilityHandler::OnSodaError() {
  OnSodaInstallFailed(speech::LanguageCode::kNone);
}

void AccessibilityHandler::OnSodaLanguagePackError(
    speech::LanguageCode language_code) {
  OnSodaInstallFailed(language_code);
}

void AccessibilityHandler::MaybeAddDictationLocales() {
  if (!features::IsExperimentalAccessibilityDictationOfflineEnabled())
    return;

  base::flat_map<std::string, ash::Dictation::LocaleData> locales =
      ash::Dictation::GetAllSupportedLocales();

  // Get application locale.
  std::string application_locale = g_browser_process->GetApplicationLocale();
  std::pair<base::StringPiece, base::StringPiece> application_lang_and_locale =
      language::SplitIntoMainAndTail(application_locale);

  // Get IME locales
  input_method::InputMethodManager* ime_manager =
      input_method::InputMethodManager::Get();
  std::vector<std::string> input_method_ids =
      ime_manager->GetActiveIMEState()->GetEnabledInputMethodIds();
  std::vector<std::string> ime_languages;
  ime_manager->GetInputMethodUtil()->GetLanguageCodesFromInputMethodIds(
      input_method_ids, &ime_languages);

  // Get enabled preferred UI languages.
  std::string preferred_languages =
      profile_->GetPrefs()->GetString(language::prefs::kPreferredLanguages);
  std::vector<base::StringPiece> enabled_languages =
      base::SplitStringPiece(preferred_languages, ",", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);

  // Combine these into one set for recommending Dication languages.
  std::set<base::StringPiece> ui_languages;
  ui_languages.insert(application_lang_and_locale.first);
  for (auto& ime_language : ime_languages) {
    ui_languages.insert(language::SplitIntoMainAndTail(ime_language).first);
  }
  for (auto& enabled_language : enabled_languages) {
    ui_languages.insert(language::SplitIntoMainAndTail(enabled_language).first);
  }

  base::Value locales_list(base::Value::Type::LIST);
  for (auto& locale : locales) {
    base::Value option(base::Value::Type::DICTIONARY);
    option.SetKey("value", base::Value(locale.first));
    option.SetKey("name",
                  base::Value(l10n_util::GetDisplayNameForLocale(
                      locale.first, application_locale, /*is_for_ui=*/true)));
    option.SetKey("worksOffline", base::Value(locale.second.works_offline));
    option.SetKey("installed", base::Value(locale.second.installed));

    // We can recommend languages that match the current application
    // locale, IME languages or enabled preferred languages.
    std::pair<base::StringPiece, base::StringPiece> lang_and_locale =
        language::SplitIntoMainAndTail(locale.first);
    bool is_recommended = base::Contains(ui_languages, lang_and_locale.first);

    option.SetKey("recommended", base::Value(is_recommended));
    locales_list.Append(std::move(option));
  }

  FireWebUIListener("dictation-locales-set", locales_list);
}

// Returns the Dictation locale as a language code.
speech::LanguageCode AccessibilityHandler::GetDictationLocale() {
  const std::string dictation_locale =
      profile_->GetPrefs()->GetString(prefs::kAccessibilityDictationLocale);
  return speech::GetLanguageCode(dictation_locale);
}

std::u16string AccessibilityHandler::GetDictationLocaleDisplayName() {
  const std::string dictation_locale =
      profile_->GetPrefs()->GetString(prefs::kAccessibilityDictationLocale);

  return l10n_util::GetDisplayNameForLocale(
      /*locale=*/dictation_locale,
      /*display_locale=*/g_browser_process->GetApplicationLocale(),
      /*is_ui=*/true);
}

}  // namespace settings
}  // namespace chromeos
