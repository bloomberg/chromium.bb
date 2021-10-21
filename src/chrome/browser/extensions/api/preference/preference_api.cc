// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/preference/preference_api.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_service.h"
#include "chrome/browser/extensions/api/preference/preference_api_constants.h"
#include "chrome/browser/extensions/api/preference/preference_helpers.h"
#include "chrome/browser/extensions/api/proxy/proxy_api.h"
#include "chrome/browser/extensions/api/system_indicator/system_indicator_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/embedder_support/pref_names.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_pref_names.h"  // nogncheck
#endif

using extensions::mojom::APIPermissionID;

namespace extensions {

namespace {

struct PrefMappingEntry {
  // Name of the preference referenced by the extension API JSON.
  const char* extension_pref;

  // Name of the preference in the PrefStores.
  const char* browser_pref;

  // Permission required to read and observe this preference.
  // Use APIPermissionID::kInvalid for |read_permission| to express that
  // the read permission should not be granted.
  APIPermissionID read_permission;

  // Permission required to write this preference.
  // Use APIPermissionID::kInvalid for |write_permission| to express that
  // the write permission should not be granted.
  APIPermissionID write_permission;
};

const char kOnPrefChangeFormat[] = "types.ChromeSetting.%s.onChange";
const char kConversionErrorMessage[] =
    "Internal error: Stored value for preference '*' cannot be converted "
    "properly.";

const PrefMappingEntry kPrefMapping[] = {
    {"alternateErrorPagesEnabled",
     embedder_support::kAlternateErrorPagesEnabled, APIPermissionID::kPrivacy,
     APIPermissionID::kPrivacy},
    {"autofillEnabled", autofill::prefs::kAutofillEnabledDeprecated,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"autofillAddressEnabled", autofill::prefs::kAutofillProfileEnabled,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"autofillCreditCardEnabled", autofill::prefs::kAutofillCreditCardEnabled,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"hyperlinkAuditingEnabled", prefs::kEnableHyperlinkAuditing,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"networkPredictionEnabled", prefs::kNetworkPredictionOptions,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"passwordSavingEnabled",
     password_manager::prefs::kCredentialsEnableService,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"protectedContentEnabled", prefs::kProtectedContentDefault,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"proxy", proxy_config::prefs::kProxy, APIPermissionID::kProxy,
     APIPermissionID::kProxy},
    {"referrersEnabled", prefs::kEnableReferrers, APIPermissionID::kPrivacy,
     APIPermissionID::kPrivacy},
    {"doNotTrackEnabled", prefs::kEnableDoNotTrack, APIPermissionID::kPrivacy,
     APIPermissionID::kPrivacy},
    {"safeBrowsingEnabled", prefs::kSafeBrowsingEnabled,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"safeBrowsingExtendedReportingEnabled",
     prefs::kSafeBrowsingScoutReportingEnabled, APIPermissionID::kPrivacy,
     APIPermissionID::kPrivacy},
    {"searchSuggestEnabled", prefs::kSearchSuggestEnabled,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"spellingServiceEnabled", spellcheck::prefs::kSpellCheckUseSpellingService,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"thirdPartyCookiesAllowed", prefs::kCookieControlsMode,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"privacySandboxEnabled", prefs::kPrivacySandboxApisEnabled,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"translationServiceEnabled", translate::prefs::kOfferTranslateEnabled,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"webRTCIPHandlingPolicy", prefs::kWebRTCIPHandlingPolicy,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    {"webRTCUDPPortRange", prefs::kWebRTCUDPPortRange,
     APIPermissionID::kPrivacy, APIPermissionID::kPrivacy},
    // accessibilityFeatures.animationPolicy is available for
    // all platforms but the others from accessibilityFeatures
    // is only available for OS_CHROMEOS.
    {"animationPolicy", prefs::kAnimationPolicy,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"autoclick", ash::prefs::kAccessibilityAutoclickEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"caretHighlight", ash::prefs::kAccessibilityCaretHighlightEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"cursorColor", ash::prefs::kAccessibilityCursorColorEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"cursorHighlight", ash::prefs::kAccessibilityCursorHighlightEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"dictation", ash::prefs::kAccessibilityDictationEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"dockedMagnifier", ash::prefs::kDockedMagnifierEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"focusHighlight", ash::prefs::kAccessibilityFocusHighlightEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"highContrast", ash::prefs::kAccessibilityHighContrastEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"largeCursor", ash::prefs::kAccessibilityLargeCursorEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"screenMagnifier", ash::prefs::kAccessibilityScreenMagnifierEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"selectToSpeak", ash::prefs::kAccessibilitySelectToSpeakEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"spokenFeedback", ash::prefs::kAccessibilitySpokenFeedbackEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"stickyKeys", ash::prefs::kAccessibilityStickyKeysEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"switchAccess", ash::prefs::kAccessibilitySwitchAccessEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
    {"virtualKeyboard", ash::prefs::kAccessibilityVirtualKeyboardEnabled,
     APIPermissionID::kAccessibilityFeaturesRead,
     APIPermissionID::kAccessibilityFeaturesModify},
#endif
};

class IdentityPrefTransformer : public PrefTransformerInterface {
 public:
  std::unique_ptr<base::Value> ExtensionToBrowserPref(
      const base::Value* extension_pref,
      std::string* error,
      bool* bad_message) override {
    return base::Value::ToUniquePtrValue(extension_pref->Clone());
  }

  std::unique_ptr<base::Value> BrowserToExtensionPref(
      const base::Value* browser_pref,
      bool is_incognito_profile) override {
    return base::Value::ToUniquePtrValue(browser_pref->Clone());
  }
};

// Transform the thirdPartyCookiesAllowed extension api to CookieControlsMode
// enum values.
class CookieControlsModeTransformer : public PrefTransformerInterface {
  using CookieControlsMode = content_settings::CookieControlsMode;

 public:
  std::unique_ptr<base::Value> ExtensionToBrowserPref(
      const base::Value* extension_pref,
      std::string* error,
      bool* bad_message) override {
    bool third_party_cookies_allowed = extension_pref->GetBool();
    return std::make_unique<base::Value>(static_cast<int>(
        third_party_cookies_allowed ? CookieControlsMode::kOff
                                    : CookieControlsMode::kBlockThirdParty));
  }

  std::unique_ptr<base::Value> BrowserToExtensionPref(
      const base::Value* browser_pref,
      bool is_incognito_profile) override {
    auto cookie_control_mode =
        static_cast<CookieControlsMode>(browser_pref->GetInt());

    bool third_party_cookies_allowed =
        cookie_control_mode == content_settings::CookieControlsMode::kOff ||
        (!is_incognito_profile &&
         cookie_control_mode == CookieControlsMode::kIncognitoOnly);

    return std::make_unique<base::Value>(third_party_cookies_allowed);
  }
};

class NetworkPredictionTransformer : public PrefTransformerInterface {
 public:
  std::unique_ptr<base::Value> ExtensionToBrowserPref(
      const base::Value* extension_pref,
      std::string* error,
      bool* bad_message) override {
    if (!extension_pref->is_bool()) {
      DCHECK(false) << "Preference not found.";
    } else if (extension_pref->GetBool()) {
      return std::make_unique<base::Value>(
          chrome_browser_net::NETWORK_PREDICTION_DEFAULT);
    }
    return std::make_unique<base::Value>(
        chrome_browser_net::NETWORK_PREDICTION_NEVER);
  }

  std::unique_ptr<base::Value> BrowserToExtensionPref(
      const base::Value* browser_pref,
      bool is_incognito_profile) override {
    int int_value = chrome_browser_net::NETWORK_PREDICTION_DEFAULT;
    if (browser_pref->is_int()) {
      int_value = browser_pref->GetInt();
    }
    return std::make_unique<base::Value>(
        int_value != chrome_browser_net::NETWORK_PREDICTION_NEVER);
  }
};

class ProtectedContentEnabledTransformer : public PrefTransformerInterface {
 public:
  std::unique_ptr<base::Value> ExtensionToBrowserPref(
      const base::Value* extension_pref,
      std::string* error,
      bool* bad_message) override {
    bool protected_identifier_allowed = extension_pref->GetBool();
    return std::make_unique<base::Value>(
        static_cast<int>(protected_identifier_allowed ? CONTENT_SETTING_ALLOW
                                                      : CONTENT_SETTING_BLOCK));
  }

  std::unique_ptr<base::Value> BrowserToExtensionPref(
      const base::Value* browser_pref,
      bool is_incognito_profile) override {
    auto protected_identifier_mode =
        static_cast<ContentSetting>(browser_pref->GetInt());
    return std::make_unique<base::Value>(protected_identifier_mode ==
                                         CONTENT_SETTING_ALLOW);
  }
};

class PrefMapping {
 public:
  PrefMapping(const PrefMapping&) = delete;
  PrefMapping& operator=(const PrefMapping&) = delete;

  static PrefMapping* GetInstance() {
    return base::Singleton<PrefMapping>::get();
  }

  bool FindBrowserPrefForExtensionPref(const std::string& extension_pref,
                                       std::string* browser_pref,
                                       APIPermissionID* read_permission,
                                       APIPermissionID* write_permission) {
    auto it = mapping_.find(extension_pref);
    if (it != mapping_.end()) {
      *browser_pref = it->second.pref_name;
      *read_permission = it->second.read_permission;
      *write_permission = it->second.write_permission;
      return true;
    }
    return false;
  }

  bool FindEventForBrowserPref(const std::string& browser_pref,
                               std::string* event_name,
                               APIPermissionID* permission) {
    auto it = event_mapping_.find(browser_pref);
    if (it != event_mapping_.end()) {
      *event_name = it->second.pref_name;
      *permission = it->second.read_permission;
      return true;
    }
    return false;
  }

  PrefTransformerInterface* FindTransformerForBrowserPref(
      const std::string& browser_pref) {
    auto it = transformers_.find(browser_pref);
    if (it != transformers_.end())
      return it->second.get();
    return identity_transformer_.get();
  }

 private:
  friend struct base::DefaultSingletonTraits<PrefMapping>;

  PrefMapping() {
    identity_transformer_ = std::make_unique<IdentityPrefTransformer>();
    for (const auto& pref : kPrefMapping) {
      mapping_[pref.extension_pref] = PrefMapData(
          pref.browser_pref, pref.read_permission, pref.write_permission);
      std::string event_name =
          base::StringPrintf(kOnPrefChangeFormat, pref.extension_pref);
      event_mapping_[pref.browser_pref] =
          PrefMapData(event_name, pref.read_permission, pref.write_permission);
    }
    DCHECK_EQ(base::size(kPrefMapping), mapping_.size());
    DCHECK_EQ(base::size(kPrefMapping), event_mapping_.size());
    RegisterPrefTransformer(proxy_config::prefs::kProxy,
                            std::make_unique<ProxyPrefTransformer>());
    RegisterPrefTransformer(prefs::kCookieControlsMode,
                            std::make_unique<CookieControlsModeTransformer>());
    RegisterPrefTransformer(prefs::kNetworkPredictionOptions,
                            std::make_unique<NetworkPredictionTransformer>());
    RegisterPrefTransformer(
        prefs::kProtectedContentDefault,
        std::make_unique<ProtectedContentEnabledTransformer>());
  }

  ~PrefMapping() = default;

  void RegisterPrefTransformer(
      const std::string& browser_pref,
      std::unique_ptr<PrefTransformerInterface> transformer) {
    DCHECK(!base::Contains(transformers_, browser_pref))
        << "Trying to register pref transformer for " << browser_pref
        << " twice";
    transformers_[browser_pref] = std::move(transformer);
  }

  struct PrefMapData {
    PrefMapData()
        : read_permission(APIPermissionID::kInvalid),
          write_permission(APIPermissionID::kInvalid) {}

    PrefMapData(const std::string& pref_name,
                APIPermissionID read,
                APIPermissionID write)
        : pref_name(pref_name),
          read_permission(read),
          write_permission(write) {}

    // Browser or extension preference to which the data maps.
    std::string pref_name;

    // Permission needed to read the preference.
    APIPermissionID read_permission;

    // Permission needed to write the preference.
    APIPermissionID write_permission;
  };

  using PrefMap = std::map<std::string, PrefMapData>;

  // Mapping from extension pref keys to browser pref keys and permissions.
  PrefMap mapping_;

  // Mapping from browser pref keys to extension event names and permissions.
  PrefMap event_mapping_;

  // Mapping from browser pref keys to transformers.
  std::map<std::string, std::unique_ptr<PrefTransformerInterface>>
      transformers_;

  std::unique_ptr<PrefTransformerInterface> identity_transformer_;
};

}  // namespace

PreferenceEventRouter::PreferenceEventRouter(Profile* profile)
    : profile_(profile) {
  registrar_.Init(profile_->GetPrefs());
  for (const auto& pref : kPrefMapping) {
    registrar_.Add(
        pref.browser_pref,
        base::BindRepeating(&PreferenceEventRouter::OnPrefChanged,
                            base::Unretained(this), registrar_.prefs()));
  }
  DCHECK(!profile_->IsOffTheRecord());
  observed_profiles_.AddObservation(profile_);
  if (profile->HasPrimaryOTRProfile())
    OnOffTheRecordProfileCreated(
        profile->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  else
    ObserveOffTheRecordPrefs(profile->GetReadOnlyOffTheRecordPrefs());
}

PreferenceEventRouter::~PreferenceEventRouter() = default;

void PreferenceEventRouter::OnPrefChanged(PrefService* pref_service,
                                          const std::string& browser_pref) {
  bool incognito = (pref_service != profile_->GetPrefs());

  std::string event_name;
  APIPermissionID permission = APIPermissionID::kInvalid;
  bool rv = PrefMapping::GetInstance()->FindEventForBrowserPref(
      browser_pref, &event_name, &permission);
  DCHECK(rv);

  base::ListValue args;
  const PrefService::Preference* pref =
      pref_service->FindPreference(browser_pref);
  CHECK(pref);
  PrefTransformerInterface* transformer =
      PrefMapping::GetInstance()->FindTransformerForBrowserPref(browser_pref);
  std::unique_ptr<base::Value> transformed_value =
      transformer->BrowserToExtensionPref(pref->GetValue(), incognito);
  if (!transformed_value) {
    LOG(ERROR) << ErrorUtils::FormatErrorMessage(kConversionErrorMessage,
                                                 pref->name());
    return;
  }

  auto dict = std::make_unique<base::DictionaryValue>();
  dict->Set(extensions::preference_api_constants::kValue,
            std::move(transformed_value));
  if (incognito) {
    ExtensionPrefs* ep = ExtensionPrefs::Get(profile_);
    dict->SetBoolean(extensions::preference_api_constants::kIncognitoSpecific,
                     ep->HasIncognitoPrefValue(browser_pref));
  }
  args.Append(std::move(dict));

  // TODO(kalman): Have a histogram value for each pref type.
  // This isn't so important for the current use case of these
  // histograms, which is to track which event types are waking up event
  // pages, or which are delivered to persistent background pages. Simply
  // "a setting changed" is enough detail for that. However if we try to
  // use these histograms for any fine-grained logic (like removing the
  // string event name altogether), or if we discover this event is
  // firing a lot and want to understand that better, then this will need
  // to change.
  events::HistogramValue histogram_value =
      events::TYPES_CHROME_SETTING_ON_CHANGE;
  extensions::preference_helpers::DispatchEventToExtensions(
      profile_, histogram_value, event_name, &args, permission, incognito,
      browser_pref);
}

void PreferenceEventRouter::OnOffTheRecordProfileCreated(
    Profile* off_the_record) {
  observed_profiles_.AddObservation(off_the_record);
  ObserveOffTheRecordPrefs(off_the_record->GetPrefs());
}

void PreferenceEventRouter::OnProfileWillBeDestroyed(Profile* profile) {
  observed_profiles_.RemoveObservation(profile);
  if (profile->IsOffTheRecord()) {
    // The real PrefService is about to be destroyed so we must make sure we
    // get the "dummy" one.
    ObserveOffTheRecordPrefs(profile_->GetReadOnlyOffTheRecordPrefs());
  }
}

void PreferenceEventRouter::ObserveOffTheRecordPrefs(PrefService* prefs) {
  incognito_registrar_ = std::make_unique<PrefChangeRegistrar>();
  incognito_registrar_->Init(prefs);
  for (const auto& pref : kPrefMapping) {
    incognito_registrar_->Add(
        pref.browser_pref,
        base::BindRepeating(&PreferenceEventRouter::OnPrefChanged,
                            base::Unretained(this),
                            incognito_registrar_->prefs()));
  }
}

void PreferenceAPIBase::SetExtensionControlledPref(
    const std::string& extension_id,
    const std::string& pref_key,
    ExtensionPrefsScope scope,
    base::Value value) {
#ifndef NDEBUG
  const PrefService::Preference* pref =
      extension_prefs()->pref_service()->FindPreference(pref_key);
  DCHECK(pref) << "Extension controlled preference key " << pref_key
               << " not registered.";
  DCHECK_EQ(pref->GetType(), value.type())
      << "Extension controlled preference " << pref_key << " has wrong type.";
#endif

  std::string scope_string;
  // ScopeToPrefName() returns false if the scope is not persisted.
  if (pref_names::ScopeToPrefName(scope, &scope_string)) {
    // Also store in persisted Preferences file to recover after a
    // browser restart.
    ExtensionPrefs::ScopedDictionaryUpdate update(extension_prefs(),
                                                  extension_id,
                                                  scope_string);
    auto preference = update.Create();
    preference->SetWithoutPathExpansion(
        pref_key, base::Value::ToUniquePtrValue(value.Clone()));
  }
  extension_pref_value_map()->SetExtensionPref(extension_id, pref_key, scope,
                                               std::move(value));
}

void PreferenceAPIBase::RemoveExtensionControlledPref(
    const std::string& extension_id,
    const std::string& pref_key,
    ExtensionPrefsScope scope) {
  DCHECK(extension_prefs()->pref_service()->FindPreference(pref_key))
      << "Extension controlled preference key " << pref_key
      << " not registered.";

  std::string scope_string;
  if (pref_names::ScopeToPrefName(scope, &scope_string)) {
    ExtensionPrefs::ScopedDictionaryUpdate update(extension_prefs(),
                                                  extension_id,
                                                  scope_string);
    auto preference = update.Get();
    if (preference)
      preference->RemoveWithoutPathExpansion(pref_key, nullptr);
  }
  extension_pref_value_map()->RemoveExtensionPref(
      extension_id, pref_key, scope);
}

bool PreferenceAPIBase::CanExtensionControlPref(
     const std::string& extension_id,
     const std::string& pref_key,
     bool incognito) {
  DCHECK(extension_prefs()->pref_service()->FindPreference(pref_key))
      << "Extension controlled preference key " << pref_key
      << " not registered.";

  return extension_pref_value_map()->CanExtensionControlPref(
       extension_id, pref_key, incognito);
}

bool PreferenceAPIBase::DoesExtensionControlPref(
    const std::string& extension_id,
    const std::string& pref_key,
    bool* from_incognito) {
  DCHECK(extension_prefs()->pref_service()->FindPreference(pref_key))
      << "Extension controlled preference key " << pref_key
      << " not registered.";

  return extension_pref_value_map()->DoesExtensionControlPref(
      extension_id, pref_key, from_incognito);
}

PreferenceAPI::PreferenceAPI(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  for (const auto& pref : kPrefMapping) {
    std::string event_name;
    APIPermissionID permission = APIPermissionID::kInvalid;
    bool rv = PrefMapping::GetInstance()->FindEventForBrowserPref(
        pref.browser_pref, &event_name, &permission);
    DCHECK(rv);
    EventRouter::Get(profile_)->RegisterObserver(this, event_name);
  }
  content_settings_store()->AddObserver(this);
}

PreferenceAPI::~PreferenceAPI() = default;

void PreferenceAPI::Shutdown() {
  EventRouter::Get(profile_)->UnregisterObserver(this);
  if (!extension_prefs()->extensions_disabled())
    ClearIncognitoSessionOnlyContentSettings();
  content_settings_store()->RemoveObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<PreferenceAPI>>::
    DestructorAtExit g_preference_api_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<PreferenceAPI>*
PreferenceAPI::GetFactoryInstance() {
  return g_preference_api_factory.Pointer();
}

// static
PreferenceAPI* PreferenceAPI::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<PreferenceAPI>::Get(context);
}

void PreferenceAPI::OnListenerAdded(const EventListenerInfo& details) {
  preference_event_router_ = std::make_unique<PreferenceEventRouter>(profile_);
  EventRouter::Get(profile_)->UnregisterObserver(this);
}

void PreferenceAPI::OnContentSettingChanged(const std::string& extension_id,
                                            bool incognito) {
  if (incognito) {
    extension_prefs()->UpdateExtensionPref(
        extension_id,
        pref_names::kPrefIncognitoContentSettings,
        content_settings_store()->GetSettingsForExtension(
            extension_id, kExtensionPrefsScopeIncognitoPersistent));
  } else {
    extension_prefs()->UpdateExtensionPref(
        extension_id,
        pref_names::kPrefContentSettings,
        content_settings_store()->GetSettingsForExtension(
            extension_id, kExtensionPrefsScopeRegular));
  }
}

void PreferenceAPI::ClearIncognitoSessionOnlyContentSettings() {
  ExtensionIdList extension_ids;
  extension_prefs()->GetExtensions(&extension_ids);
  for (const auto& id : extension_ids) {
    content_settings_store()->ClearContentSettingsForExtension(
        id, kExtensionPrefsScopeIncognitoSessionOnly);
  }
}

ExtensionPrefs* PreferenceAPI::extension_prefs() {
  return ExtensionPrefs::Get(profile_);
}

ExtensionPrefValueMap* PreferenceAPI::extension_pref_value_map() {
  return ExtensionPrefValueMapFactory::GetForBrowserContext(profile_);
}

scoped_refptr<ContentSettingsStore> PreferenceAPI::content_settings_store() {
  return ContentSettingsService::Get(profile_)->content_settings_store();
}

template <>
void
BrowserContextKeyedAPIFactory<PreferenceAPI>::DeclareFactoryDependencies() {
  DependsOn(ContentSettingsService::GetFactoryInstance());
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(ExtensionPrefValueMapFactory::GetInstance());
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

PreferenceFunction::~PreferenceFunction() = default;

GetPreferenceFunction::~GetPreferenceFunction() = default;

ExtensionFunction::ResponseAction GetPreferenceFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  EXTENSION_FUNCTION_VALIDATE(args()[1].is_dict());

  const std::string& pref_key = args()[0].GetString();
  const base::Value& details = args()[1];

  bool incognito = false;
  if (absl::optional<bool> result = details.FindBoolKey(
          extensions::preference_api_constants::kIncognitoKey)) {
    incognito = *result;
  }

  // Check incognito access.
  if (incognito) {
    // Extensions are only allowed to modify incognito preferences if they are
    // enabled in incognito. If the calling browser context is off the record,
    // then the extension must be allowed to run incognito. Otherwise, this
    // could be a spanning mode extension, and we need to check its incognito
    // access.
    if (!browser_context()->IsOffTheRecord() &&
        !include_incognito_information()) {
      return RespondNow(
          Error(extensions::preference_api_constants::kIncognitoErrorMessage));
    }
  }

  // Obtain pref.
  std::string browser_pref;
  APIPermissionID read_permission = APIPermissionID::kInvalid;
  APIPermissionID write_permission = APIPermissionID::kInvalid;
  EXTENSION_FUNCTION_VALIDATE(
      PrefMapping::GetInstance()->FindBrowserPrefForExtensionPref(
      pref_key, &browser_pref, &read_permission, &write_permission));
  if (!extension()->permissions_data()->HasAPIPermission(read_permission))
    return RespondNow(
        Error(extensions::preference_api_constants::kPermissionErrorMessage,
              pref_key));

  Profile* profile = Profile::FromBrowserContext(browser_context());
  PrefService* prefs =
      extensions::preference_helpers::GetProfilePrefService(profile, incognito);

  const PrefService::Preference* pref = prefs->FindPreference(browser_pref);
  CHECK(pref);

  base::Value result(base::Value::Type::DICTIONARY);

  // Retrieve level of control.
  std::string level_of_control =
      extensions::preference_helpers::GetLevelOfControl(
          profile, extension_id(), browser_pref, incognito);
  result.SetStringKey(extensions::preference_api_constants::kLevelOfControl,
                      level_of_control);

  // Retrieve pref value.
  PrefTransformerInterface* transformer =
      PrefMapping::GetInstance()->FindTransformerForBrowserPref(browser_pref);
  std::unique_ptr<base::Value> transformed_value =
      transformer->BrowserToExtensionPref(pref->GetValue(), incognito);
  if (!transformed_value) {
    // TODO(devlin): Can this happen?  When?  Should it be an error, or a bad
    // message?
    LOG(ERROR) << ErrorUtils::FormatErrorMessage(kConversionErrorMessage,
                                                 pref->name());
    return RespondNow(Error(kUnknownErrorDoNotUse));
  }
  result.SetKey(extensions::preference_api_constants::kValue,
                base::Value::FromUniquePtrValue(std::move(transformed_value)));

  // Retrieve incognito status.
  if (incognito) {
    ExtensionPrefs* ep = ExtensionPrefs::Get(browser_context());
    result.SetBoolKey(extensions::preference_api_constants::kIncognitoSpecific,
                      ep->HasIncognitoPrefValue(browser_pref));
  }

  return RespondNow(OneArgument(std::move(result)));
}

SetPreferenceFunction::~SetPreferenceFunction() = default;

ExtensionFunction::ResponseAction SetPreferenceFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  EXTENSION_FUNCTION_VALIDATE(args()[1].is_dict());

  std::string pref_key = args()[0].GetString();
  const base::Value& details = args()[1];

  const base::Value* value =
      details.FindKey(extensions::preference_api_constants::kValue);
  EXTENSION_FUNCTION_VALIDATE(value);

  ExtensionPrefsScope scope = kExtensionPrefsScopeRegular;
  if (const std::string* scope_str = details.FindStringKey(
          extensions::preference_api_constants::kScopeKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        extensions::preference_helpers::StringToScope(*scope_str, &scope));
  }

  // Check incognito scope.
  bool incognito =
      (scope == kExtensionPrefsScopeIncognitoPersistent ||
       scope == kExtensionPrefsScopeIncognitoSessionOnly);
  if (incognito) {
    // Regular profiles can't access incognito unless
    // include_incognito_information is true.
    if (!browser_context()->IsOffTheRecord() &&
        !include_incognito_information())
      return RespondNow(
          Error(extensions::preference_api_constants::kIncognitoErrorMessage));
  } else if (browser_context()->IsOffTheRecord()) {
    // If the browser_context associated with this ExtensionFunction is off the
    // record, it must have come from the incognito process for a split-mode
    // extension (spanning mode extensions only run in the on-the-record
    // process). The incognito profile of a split-mode extension should never be
    // able to modify the on-the-record profile, so error out.
    return RespondNow(
        Error("Can't modify regular settings from an incognito context."));
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (scope == kExtensionPrefsScopeIncognitoSessionOnly &&
      !profile->HasPrimaryOTRProfile()) {
    return RespondNow(Error(extensions::preference_api_constants::
                                kIncognitoSessionOnlyErrorMessage));
  }

  // Obtain pref.
  std::string browser_pref;
  APIPermissionID read_permission = APIPermissionID::kInvalid;
  APIPermissionID write_permission = APIPermissionID::kInvalid;
  EXTENSION_FUNCTION_VALIDATE(
      PrefMapping::GetInstance()->FindBrowserPrefForExtensionPref(
      pref_key, &browser_pref, &read_permission, &write_permission));
  if (!extension()->permissions_data()->HasAPIPermission(write_permission))
    return RespondNow(
        Error(extensions::preference_api_constants::kPermissionErrorMessage,
              pref_key));

  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context());
  const PrefService::Preference* pref =
      prefs->pref_service()->FindPreference(browser_pref);
  CHECK(pref);

  // Validate new value.
  PrefTransformerInterface* transformer =
      PrefMapping::GetInstance()->FindTransformerForBrowserPref(browser_pref);
  std::string error;
  bool bad_message = false;
  std::unique_ptr<base::Value> browser_pref_value(
      transformer->ExtensionToBrowserPref(value, &error, &bad_message));
  if (!browser_pref_value) {
    EXTENSION_FUNCTION_VALIDATE(!bad_message);
    return RespondNow(Error(std::move(error)));
  }
  EXTENSION_FUNCTION_VALIDATE(browser_pref_value->type() == pref->GetType());

  // Validate also that the stored value can be converted back by the
  // transformer.
  std::unique_ptr<base::Value> extension_pref_value(
      transformer->BrowserToExtensionPref(browser_pref_value.get(), incognito));
  EXTENSION_FUNCTION_VALIDATE(extension_pref_value);

  PreferenceAPI* preference_api = PreferenceAPI::Get(browser_context());

  // Set the new Autofill prefs if the extension sets the deprecated pref in
  // order to maintain backward compatibility in the extensions preference API.
  // TODO(crbug.com/870328): Remove this once the deprecated pref is retired.
  if (autofill::prefs::kAutofillEnabledDeprecated == browser_pref) {
    // |SetExtensionControlledPref| takes ownership of the base::Value pointer.
    preference_api->SetExtensionControlledPref(
        extension_id(), autofill::prefs::kAutofillCreditCardEnabled, scope,
        base::Value(browser_pref_value->GetBool()));
    preference_api->SetExtensionControlledPref(
        extension_id(), autofill::prefs::kAutofillProfileEnabled, scope,
        base::Value(browser_pref_value->GetBool()));
  }

  // Whenever an extension takes control of the |kSafeBrowsingEnabled|
  // preference, it must also set |kSafeBrowsingEnhanced| to false.
  // See crbug.com/1064722 for more background.
  //
  // TODO(crbug.com/1064722): Consider extending
  // chrome.privacy.services.safeBrowsingEnabled to a three-state enum.
  if (prefs::kSafeBrowsingEnabled == browser_pref) {
    preference_api->SetExtensionControlledPref(extension_id(),
                                               prefs::kSafeBrowsingEnhanced,
                                               scope, base::Value(false));
  }

  preference_api->SetExtensionControlledPref(
      extension_id(), browser_pref, scope,
      base::Value::FromUniquePtrValue(std::move(browser_pref_value)));

  return RespondNow(NoArguments());
}

ClearPreferenceFunction::~ClearPreferenceFunction() = default;

ExtensionFunction::ResponseAction ClearPreferenceFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 2);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  EXTENSION_FUNCTION_VALIDATE(args()[1].is_dict());

  std::string pref_key = args()[0].GetString();
  const base::Value& details = args()[1];

  ExtensionPrefsScope scope = kExtensionPrefsScopeRegular;
  if (const std::string* scope_str = details.FindStringKey(
          extensions::preference_api_constants::kScopeKey)) {
    EXTENSION_FUNCTION_VALIDATE(
        extensions::preference_helpers::StringToScope(*scope_str, &scope));
  }

  // Check incognito scope.
  bool incognito =
      (scope == kExtensionPrefsScopeIncognitoPersistent ||
       scope == kExtensionPrefsScopeIncognitoSessionOnly);
  if (incognito) {
    // We don't check incognito permissions here, as an extension should be
    // always allowed to clear its own settings.
  } else if (browser_context()->IsOffTheRecord()) {
    // Incognito profiles can't access regular mode ever, they only exist in
    // split mode.
    return RespondNow(
        Error("Can't modify regular settings from an incognito context."));
  }

  std::string browser_pref;
  APIPermissionID read_permission = APIPermissionID::kInvalid;
  APIPermissionID write_permission = APIPermissionID::kInvalid;
  EXTENSION_FUNCTION_VALIDATE(
      PrefMapping::GetInstance()->FindBrowserPrefForExtensionPref(
      pref_key, &browser_pref, &read_permission, &write_permission));
  if (!extension()->permissions_data()->HasAPIPermission(write_permission))
    return RespondNow(
        Error(extensions::preference_api_constants::kPermissionErrorMessage,
              pref_key));

  PreferenceAPI::Get(browser_context())
      ->RemoveExtensionControlledPref(extension_id(), browser_pref, scope);

  // Whenever an extension clears the |kSafeBrowsingEnabled| preference,
  // it must also clear |kSafeBrowsingEnhanced|. See crbug.com/1064722 for
  // more background.
  //
  // TODO(crbug.com/1064722): Consider extending
  // chrome.privacy.services.safeBrowsingEnabled to a three-state enum.
  if (prefs::kSafeBrowsingEnabled == browser_pref) {
    PreferenceAPI::Get(browser_context())
        ->RemoveExtensionControlledPref(extension_id(),
                                        prefs::kSafeBrowsingEnhanced, scope);
  }

  return RespondNow(NoArguments());
}

}  // namespace extensions
