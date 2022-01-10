// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/live_caption/pref_names.h"
#include "components/media_router/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/variations/variations.mojom.h"
#include "components/variations/variations_client.h"
#include "components/variations/variations_ids_provider.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/android/jni_headers/OTRProfileID_jni.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_pref_store.h"
#include "extensions/browser/extension_pref_value_map_factory.h"
#include "extensions/browser/pref_names.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/common/chrome_constants.h"
#include "chromeos/lacros/lacros_service.h"
#endif

#if DCHECK_IS_ON()

#include <set>
#include "base/check_op.h"
#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"

namespace {

base::LazyInstance<base::Lock>::Leaky g_profile_instances_lock =
    LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<std::set<content::BrowserContext*>>::Leaky
    g_profile_instances = LAZY_INSTANCE_INITIALIZER;

}  // namespace

#endif  // DCHECK_IS_ON()

namespace {

const char kDevToolsOTRProfileIDPrefix[] = "Devtools::BrowserContext";
const char kMediaRouterOTRProfileIDPrefix[] = "MediaRouter::Presentation";
const char kTestOTRProfileIDPrefix[] = "Test::OTR";

}  // namespace

Profile::OTRProfileID::OTRProfileID(const std::string& profile_id)
    : profile_id_(profile_id) {}

bool Profile::OTRProfileID::AllowsBrowserWindows() const {
  // Non-Primary OTR profiles are not supposed to create Browser windows.
  // DevTools::BrowserContext and MediaRouter::Presentation are an
  // exception to this ban.
  return *this == PrimaryID() ||
         base::StartsWith(profile_id_, kDevToolsOTRProfileIDPrefix,
                          base::CompareCase::SENSITIVE) ||
         base::StartsWith(profile_id_, kMediaRouterOTRProfileIDPrefix,
                          base::CompareCase::SENSITIVE);
}

// static
const Profile::OTRProfileID Profile::OTRProfileID::PrimaryID() {
  // OTRProfileID value should be same as
  // |OTRProfileID.java#sPrimaryOTRProfileID| variable.
  return OTRProfileID("profile::primary_otr");
}

// static
Profile::OTRProfileID Profile::OTRProfileID::CreateUnique(
    const std::string& profile_id_prefix) {
  return OTRProfileID(base::StringPrintf(
      "%s-%s", profile_id_prefix.c_str(),
      base::GUID::GenerateRandomV4().AsLowercaseString().c_str()));
}

// static
Profile::OTRProfileID Profile::OTRProfileID::CreateUniqueForDevTools() {
  return CreateUnique(kDevToolsOTRProfileIDPrefix);
}

// static
Profile::OTRProfileID Profile::OTRProfileID::CreateUniqueForMediaRouter() {
  return CreateUnique(kMediaRouterOTRProfileIDPrefix);
}

// static
Profile::OTRProfileID Profile::OTRProfileID::CreateUniqueForTesting() {
  return CreateUnique(kTestOTRProfileIDPrefix);
}

const std::string& Profile::OTRProfileID::ToString() const {
  return profile_id_;
}

std::ostream& operator<<(std::ostream& out,
                         const Profile::OTRProfileID& profile_id) {
  out << profile_id.ToString();
  return out;
}

#if defined(OS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
Profile::OTRProfileID::ConvertToJavaOTRProfileID(JNIEnv* env) const {
  return Java_OTRProfileID_Constructor(
      env, base::android::ConvertUTF8ToJavaString(env, profile_id_));
}

// static
Profile::OTRProfileID Profile::OTRProfileID::ConvertFromJavaOTRProfileID(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_otr_profile_id) {
  return OTRProfileID(base::android::ConvertJavaStringToUTF8(
      env, Java_OTRProfileID_getProfileID(env, j_otr_profile_id)));
}

// static
base::android::ScopedJavaLocalRef<jobject>
JNI_OTRProfileID_CreateUniqueOTRProfileID(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& j_profile_id_prefix) {
  Profile::OTRProfileID profile_id = Profile::OTRProfileID::CreateUnique(
      base::android::ConvertJavaStringToUTF8(env, j_profile_id_prefix));
  return profile_id.ConvertToJavaOTRProfileID(env);
}

// static
base::android::ScopedJavaLocalRef<jobject> JNI_OTRProfileID_GetPrimaryID(
    JNIEnv* env) {
  return Profile::OTRProfileID::PrimaryID().ConvertToJavaOTRProfileID(env);
}

// static
Profile::OTRProfileID Profile::OTRProfileID::Deserialize(
    const std::string& value) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> j_value =
      base::android::ConvertUTF8ToJavaString(env, value);
  base::android::ScopedJavaLocalRef<jobject> j_otr_profile_id =
      Java_OTRProfileID_deserializeWithoutVerify(env, j_value);
  return ConvertFromJavaOTRProfileID(env, j_otr_profile_id);
}

std::string Profile::OTRProfileID::Serialize() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::android::ConvertJavaStringToUTF8(
      env, Java_OTRProfileID_serialize(env, ConvertToJavaOTRProfileID(env)));
}
#endif  // defined(OS_ANDROID)

Profile::Profile()
    : resource_context_(std::make_unique<content::ResourceContext>()) {
#if DCHECK_IS_ON()
  base::AutoLock lock(g_profile_instances_lock.Get());
  g_profile_instances.Get().insert(this);
#endif  // DCHECK_IS_ON()

  BrowserContextDependencyManager::GetInstance()->MarkBrowserContextLive(this);
}

Profile::~Profile() {
  if (content::BrowserThread::IsThreadInitialized(content::BrowserThread::IO)) {
    content::GetIOThreadTaskRunner({})->DeleteSoon(
        FROM_HERE, std::move(resource_context_));
  }
#if DCHECK_IS_ON()
  base::AutoLock lock(g_profile_instances_lock.Get());
  g_profile_instances.Get().erase(this);
#endif  // DCHECK_IS_ON()
}

// static
Profile* Profile::FromBrowserContext(content::BrowserContext* browser_context) {
  if (!browser_context)
    return nullptr;

  // For code running in a chrome/ environment, it is safe to cast to Profile*
  // because Profile is the only implementation of BrowserContext used. In
  // testing, however, there are several BrowserContext subclasses that are not
  // Profile subclasses, and we can catch them. http://crbug.com/725276
#if DCHECK_IS_ON()
  base::AutoLock lock(g_profile_instances_lock.Get());
  if (!g_profile_instances.Get().count(browser_context)) {
    DCHECK(false)
        << "Non-Profile BrowserContext passed to Profile::FromBrowserContext! "
           "If you have a test linked in chrome/ you need a chrome/ based test "
           "class such as TestingProfile in chrome/test/base/testing_profile.h "
           "or you need to subclass your test class from Profile, not from "
           "BrowserContext.";
  }
#endif  // DCHECK_IS_ON()
  return static_cast<Profile*>(browser_context);
}

// static
Profile* Profile::FromWebUI(content::WebUI* web_ui) {
  return FromBrowserContext(web_ui->GetWebContents()->GetBrowserContext());
}

void Profile::AddObserver(ProfileObserver* observer) {
  observers_.AddObserver(observer);
}

void Profile::RemoveObserver(ProfileObserver* observer) {
  observers_.RemoveObserver(observer);
}

base::FilePath Profile::GetBaseName() const {
  return GetPath().BaseName();
}

std::string Profile::GetDebugName() const {
  std::string name = GetBaseName().MaybeAsASCII();
  return name.empty() ? "UnknownProfile" : name;
}

TestingProfile* Profile::AsTestingProfile() {
  return nullptr;
}

ChromeZoomLevelPrefs* Profile::GetZoomLevelPrefs() {
  return nullptr;
}

Profile::Delegate::~Delegate() {
}

// static
const char Profile::kProfileKey[] = "__PROFILE__";

// static
void Profile::RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kSearchSuggestEnabled,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#if defined(OS_ANDROID)
  registry->RegisterStringPref(
      prefs::kContextualSearchEnabled,
      std::string(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kContextualSearchWasFullyPrivacyEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(prefs::kContextualSearchPromoCardShownCount, 0);
#endif  // defined(OS_ANDROID)
  registry->RegisterStringPref(prefs::kSessionExitType, std::string());
  registry->RegisterBooleanPref(prefs::kDisableExtensions, false);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  registry->RegisterBooleanPref(extensions::pref_names::kAlertsInitialized,
                                false);
#endif
  base::FilePath home;
  base::PathService::Get(base::DIR_HOME, &home);
  registry->RegisterStringPref(prefs::kSelectFileLastDirectory,
                               home.MaybeAsASCII());
  registry->RegisterStringPref(prefs::kAccessibilityCaptionsTextSize,
                               std::string());
  registry->RegisterStringPref(prefs::kAccessibilityCaptionsTextFont,
                               std::string());
  registry->RegisterStringPref(prefs::kAccessibilityCaptionsTextColor,
                               std::string());
  registry->RegisterIntegerPref(prefs::kAccessibilityCaptionsTextOpacity, 100);
  registry->RegisterIntegerPref(prefs::kAccessibilityCaptionsBackgroundOpacity,
                                100);
  registry->RegisterStringPref(prefs::kAccessibilityCaptionsBackgroundColor,
                               std::string());
  registry->RegisterStringPref(prefs::kAccessibilityCaptionsTextShadow,
                               std::string());
  registry->RegisterDictionaryPref(prefs::kPartitionDefaultZoomLevel);
  registry->RegisterDictionaryPref(prefs::kPartitionPerHostZoomLevels);
  registry->RegisterStringPref(prefs::kPreinstalledApps, "install");
  registry->RegisterBooleanPref(prefs::kSpeechRecognitionFilterProfanities,
                                true);
  registry->RegisterIntegerPref(prefs::kProfileIconVersion, 0);
  registry->RegisterBooleanPref(prefs::kAllowDinosaurEasterEgg, true);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(dilmah): For OS_CHROMEOS we maintain kApplicationLocale in both
  // local state and user's profile.  For other platforms we maintain
  // kApplicationLocale only in local state.
  // In the future we may want to maintain kApplicationLocale
  // in user's profile for other platforms as well.
  registry->RegisterStringPref(
      language::prefs::kApplicationLocale, std::string(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  registry->RegisterStringPref(prefs::kApplicationLocaleBackup, std::string());
  registry->RegisterStringPref(prefs::kApplicationLocaleAccepted,
                               std::string());
#endif

  data_reduction_proxy::RegisterSyncableProfilePrefs(registry);

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  // Preferences related to the avatar bubble and user manager tutorials.
  registry->RegisterIntegerPref(prefs::kProfileAvatarTutorialShown, 0);
#endif

#if defined(OS_ANDROID)
  registry->RegisterBooleanPref(prefs::kClickedUpdateMenuItem, false);
  registry->RegisterStringPref(prefs::kLatestVersionWhenClickedUpdateMenuItem,
                               std::string());
#endif

#if defined(OS_ANDROID)
  registry->RegisterStringPref(prefs::kCommerceMerchantViewerMessagesShownTime,
                               std::string());
#endif

#if !defined(OS_ANDROID)
  registry->RegisterBooleanPref(
      media_router::prefs::kMediaRouterCloudServicesPrefSet, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      media_router::prefs::kMediaRouterEnableCloudServices, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      media_router::prefs::kMediaRouterMediaRemotingEnabled, true);
  registry->RegisterListPref(
      media_router::prefs::kMediaRouterTabMirroringSources);
#endif

  registry->RegisterDictionaryPref(prefs::kWebShareVisitedTargets);
  registry->RegisterDictionaryPref(
      prefs::kProtocolHandlerPerOriginAllowedProtocols);

  registry->RegisterListPref(prefs::kAutoLaunchProtocolsFromOrigins);

  // Instead of registering new prefs here, please create a static method and
  // invoke it from RegisterProfilePrefs() in
  // chrome/browser/prefs/browser_prefs.cc.
}

bool Profile::IsRegularProfile() const {
  return profile_metrics::GetBrowserProfileType(this) ==
         profile_metrics::BrowserProfileType::kRegular;
}

bool Profile::IsIncognitoProfile() const {
  return profile_metrics::GetBrowserProfileType(this) ==
         profile_metrics::BrowserProfileType::kIncognito;
}

bool Profile::IsGuestSession() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  static bool is_guest_session =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kGuestSession);
  return is_guest_session;
#else
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  DCHECK(chromeos::LacrosService::Get());
  if (chromeos::LacrosService::Get()->init_params()->session_type ==
      crosapi::mojom::SessionType::kGuestSession) {
    return true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  return profile_metrics::GetBrowserProfileType(this) ==
         profile_metrics::BrowserProfileType::kGuest;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

PrefService* Profile::GetReadOnlyOffTheRecordPrefs() {
  return nullptr;
}

bool Profile::IsSystemProfile() const {
  return profile_metrics::GetBrowserProfileType(this) ==
         profile_metrics::BrowserProfileType::kSystem;
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// static
bool Profile::IsMainProfilePath(base::FilePath profile_path) {
  // The main profile is the one with the "Default" path.
  return profile_path.BaseName().value() == chrome::kInitialProfile;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

bool Profile::IsPrimaryOTRProfile() const {
  return IsOffTheRecord() && GetOTRProfileID() == OTRProfileID::PrimaryID();
}

bool Profile::CanUseDiskWhenOffTheRecord() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Guest mode on ChromeOS uses an in-memory file system to store the profile
  // in, so despite this being an off the record profile, it is still okay to
  // store data on disk.
  return IsGuestSession();
#else
  return false;
#endif
}

bool Profile::ShouldRestoreOldSessionCookies() {
  return false;
}

bool Profile::ShouldPersistSessionCookies() const {
  return false;
}

void Profile::MaybeSendDestroyedNotification() {
  TRACE_EVENT1("shutdown", "Profile::MaybeSendDestroyedNotification", "profile",
               this);

  if (!sent_destroyed_notification_) {
    sent_destroyed_notification_ = true;

    NotifyWillBeDestroyed();

    for (auto& observer : observers_)
      observer.OnProfileWillBeDestroyed(this);
  }
}

// static
PrefStore* Profile::CreateExtensionPrefStore(Profile* profile,
                                             bool incognito_pref_store) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return new ExtensionPrefStore(
      ExtensionPrefValueMapFactory::GetForBrowserContext(profile),
      incognito_pref_store);
#else
  return nullptr;
#endif
}

bool ProfileCompare::operator()(Profile* a, Profile* b) const {
  DCHECK(a && b);
  if (a->IsSameOrParent(b))
    return false;
  return a->GetOriginalProfile() < b->GetOriginalProfile();
}

double Profile::GetDefaultZoomLevelForProfile() {
  return GetDefaultStoragePartition()->GetHostZoomMap()->GetDefaultZoomLevel();
}

void Profile::Wipe() {
  GetBrowsingDataRemover()->Remove(
      base::Time(), base::Time::Max(),
      chrome_browsing_data_remover::WIPE_PROFILE,
      chrome_browsing_data_remover::ALL_ORIGIN_TYPES);
}

void Profile::NotifyOffTheRecordProfileCreated(Profile* off_the_record) {
  DCHECK_EQ(off_the_record->GetOriginalProfile(), this);
  DCHECK(off_the_record->IsOffTheRecord());
  for (auto& observer : observers_)
    observer.OnOffTheRecordProfileCreated(off_the_record);
}

Profile* Profile::GetPrimaryOTRProfile(bool create_if_needed) {
  return GetOffTheRecordProfile(OTRProfileID::PrimaryID(), create_if_needed);
}

bool Profile::HasPrimaryOTRProfile() {
  return HasOffTheRecordProfile(OTRProfileID::PrimaryID());
}

class Profile::ChromeVariationsClient : public variations::VariationsClient {
 public:
  explicit ChromeVariationsClient(Profile* profile) : profile_(profile) {}

  ~ChromeVariationsClient() override = default;

  bool IsOffTheRecord() const override { return profile_->IsOffTheRecord(); }

  variations::mojom::VariationsHeadersPtr GetVariationsHeaders()
      const override {
    return variations::VariationsIdsProvider::GetInstance()
        ->GetClientDataHeaders(profile_->IsSignedIn());
  }

 private:
  raw_ptr<Profile> profile_;
};

variations::VariationsClient* Profile::GetVariationsClient() {
  if (!chrome_variations_client_)
    chrome_variations_client_ = std::make_unique<ChromeVariationsClient>(this);
  return chrome_variations_client_.get();
}

content::ResourceContext* Profile::GetResourceContext() {
  return resource_context_.get();
}
