// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"

#include <utility>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/dip_px_util.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_shelf_utils.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_features.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/vm_applications/apps.pb.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "ui/base/l10n/l10n_util.h"

using vm_tools::apps::App;

namespace guest_os {

namespace {

// This prefix is used when generating the crostini app list id.
constexpr char kCrostiniAppIdPrefix[] = "crostini:";

constexpr char kCrostiniIconFolder[] = "crostini.icons";

constexpr char kCrostiniAppsInstalledHistogram[] =
    "Crostini.AppsInstalledAtLogin";

constexpr char kPluginVmAppsInstalledHistogram[] =
    "PluginVm.AppsInstalledAtLogin";

constexpr char kBorealisAppsInstalledHistogram[] =
    "Borealis.AppsInstalledAtLogin";

base::Value ProtoToDictionary(const App::LocaleString& locale_string) {
  base::Value result(base::Value::Type::DICTIONARY);
  for (const App::LocaleString::Entry& entry : locale_string.values()) {
    const std::string& locale = entry.locale();

    std::string locale_with_dashes(locale);
    std::replace(locale_with_dashes.begin(), locale_with_dashes.end(), '_',
                 '-');
    if (!locale.empty() && !l10n_util::IsValidLocaleSyntax(locale_with_dashes))
      continue;

    result.SetKey(locale, base::Value(entry.value()));
  }
  return result;
}

std::set<std::string> ListToStringSet(const base::Value* list,
                                      bool to_lower_ascii = false) {
  std::set<std::string> result;
  if (!list)
    return result;
  for (const base::Value& value : list->GetList())
    result.insert(to_lower_ascii ? base::ToLowerASCII(value.GetString())
                                 : value.GetString());
  return result;
}

base::Value ProtoToList(
    const google::protobuf::RepeatedPtrField<std::string>& strings) {
  base::Value result(base::Value::Type::LIST);
  for (const std::string& string : strings)
    result.Append(string);
  return result;
}

base::Value LocaleStringsProtoToDictionary(
    const App::LocaleStrings& repeated_locale_string) {
  base::Value result(base::Value::Type::DICTIONARY);
  for (const auto& strings_with_locale : repeated_locale_string.values()) {
    const std::string& locale = strings_with_locale.locale();

    std::string locale_with_dashes(locale);
    std::replace(locale_with_dashes.begin(), locale_with_dashes.end(), '_',
                 '-');
    if (!locale.empty() && !l10n_util::IsValidLocaleSyntax(locale_with_dashes))
      continue;
    result.SetKey(locale, ProtoToList(strings_with_locale.value()));
  }
  return result;
}

// Populate |pref_registration| based on the given App proto.
// |name| should be |app.name()| in Dictionary form.
void PopulatePrefRegistrationFromApp(base::Value& pref_registration,
                                     GuestOsRegistryService::VmType vm_type,
                                     const std::string& vm_name,
                                     const std::string& container_name,
                                     const vm_tools::apps::App& app,
                                     base::Value name) {
  pref_registration.SetKey(guest_os::prefs::kAppDesktopFileIdKey,
                           base::Value(app.desktop_file_id()));
  pref_registration.SetIntKey(guest_os::prefs::kAppVmTypeKey,
                              static_cast<int>(vm_type));
  pref_registration.SetKey(guest_os::prefs::kAppVmNameKey,
                           base::Value(vm_name));
  pref_registration.SetKey(guest_os::prefs::kAppContainerNameKey,
                           base::Value(container_name));
  pref_registration.SetKey(guest_os::prefs::kAppNameKey, std::move(name));
  pref_registration.SetKey(guest_os::prefs::kAppCommentKey,
                           ProtoToDictionary(app.comment()));
  pref_registration.SetKey(guest_os::prefs::kAppExecKey,
                           base::Value(app.exec()));
  pref_registration.SetKey(guest_os::prefs::kAppExecutableFileNameKey,
                           base::Value(app.executable_file_name()));
  pref_registration.SetKey(guest_os::prefs::kAppExtensionsKey,
                           ProtoToList(app.extensions()));
  pref_registration.SetKey(guest_os::prefs::kAppMimeTypesKey,
                           ProtoToList(app.mime_types()));
  pref_registration.SetKey(guest_os::prefs::kAppKeywordsKey,
                           LocaleStringsProtoToDictionary(app.keywords()));
  pref_registration.SetKey(guest_os::prefs::kAppNoDisplayKey,
                           base::Value(app.no_display()));
  pref_registration.SetKey(guest_os::prefs::kAppStartupWMClassKey,
                           base::Value(app.startup_wm_class()));
  pref_registration.SetKey(guest_os::prefs::kAppStartupNotifyKey,
                           base::Value(app.startup_notify()));
  pref_registration.SetKey(guest_os::prefs::kAppPackageIdKey,
                           base::Value(app.package_id()));
}

// This is the companion to GuestOsRegistryService::SetCurrentTime().
base::Time GetTime(const base::Value& pref, const char* key) {
  if (!pref.is_dict())
    return base::Time();

  const base::Value* value = pref.FindKeyOfType(key, base::Value::Type::STRING);
  int64_t time;
  if (!value || !base::StringToInt64(value->GetString(), &time))
    return base::Time();
  return base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(time));
}

bool EqualsExcludingTimestamps(const base::Value& left,
                               const base::Value& right) {
  auto left_items = left.DictItems();
  auto right_items = right.DictItems();
  auto left_iter = left_items.begin();
  auto right_iter = right_items.begin();
  while (left_iter != left_items.end() && right_iter != right_items.end()) {
    if (left_iter->first == guest_os::prefs::kAppInstallTimeKey ||
        left_iter->first == guest_os::prefs::kAppLastLaunchTimeKey) {
      ++left_iter;
      continue;
    }
    if (right_iter->first == guest_os::prefs::kAppInstallTimeKey ||
        right_iter->first == guest_os::prefs::kAppLastLaunchTimeKey) {
      ++right_iter;
      continue;
    }
    if (*left_iter != *right_iter)
      return false;
    ++left_iter;
    ++right_iter;
  }
  return left_iter == left_items.end() && right_iter == right_items.end();
}

void InstallIconFromFileThread(const base::FilePath& icon_path,
                               const std::string& content_png) {
  DCHECK(!content_png.empty());

  base::CreateDirectory(icon_path.DirName());

  int wrote =
      base::WriteFile(icon_path, content_png.c_str(), content_png.size());
  if (wrote != static_cast<int>(content_png.size())) {
    VLOG(2) << "Failed to write Crostini icon file: "
            << icon_path.MaybeAsASCII();
    if (!base::DeleteFile(icon_path)) {
      VLOG(2) << "Couldn't delete broken icon file" << icon_path.MaybeAsASCII();
    }
  }
}

void DeleteIconFolderFromFileThread(const base::FilePath& path) {
  DCHECK(path.DirName().BaseName().MaybeAsASCII() == kCrostiniIconFolder &&
         (!base::PathExists(path) || base::DirectoryExists(path)));
  const bool deleted = base::DeletePathRecursively(path);
  DCHECK(deleted);
}

template <typename List>
static std::string Join(const List& list);

static std::string ToString(bool b) {
  return b ? "true" : "false";
}

static std::string ToString(int i) {
  return base::NumberToString(i);
}

static std::string ToString(const std::string& string) {
  return '"' + string + '"';
}

static std::string ToString(
    const google::protobuf::RepeatedPtrField<std::string>& list) {
  return Join(list);
}

static std::string ToString(
    const vm_tools::apps::App_LocaleString_Entry& entry) {
  return "{locale: " + ToString(entry.locale()) +
         ", value: " + ToString(entry.value()) + "}";
}

static std::string ToString(
    const vm_tools::apps::App_LocaleStrings_StringsWithLocale&
        strings_with_locale) {
  return "{locale: " + ToString(strings_with_locale.locale()) +
         ", value: " + ToString(strings_with_locale.value()) + "}";
}

static std::string ToString(const vm_tools::apps::App_LocaleString& string) {
  return Join(string.values());
}

static std::string ToString(const vm_tools::apps::App_LocaleStrings& strings) {
  return Join(strings.values());
}

static std::string ToString(const vm_tools::apps::App& app) {
  return "{desktop_file_id: " + ToString(app.desktop_file_id()) +
         ", name: " + ToString(app.name()) +
         ", comment: " + ToString(app.comment()) +
         ", mime_types: " + ToString(app.mime_types()) +
         ", no_display: " + ToString(app.no_display()) +
         ", startup_wm_class: " + ToString(app.startup_wm_class()) +
         ", startup_notify: " + ToString(app.startup_notify()) +
         ", keywords: " + ToString(app.keywords()) +
         ", exec: " + ToString(app.exec()) +
         ", executable_file_name: " + ToString(app.executable_file_name()) +
         ", package_id: " + ToString(app.package_id()) +
         ", extensions: " + ToString(app.extensions()) + "}";
}

static std::string ToString(const vm_tools::apps::ApplicationList& list) {
  return "{apps: " + Join(list.apps()) +
         ", vm_type: " + ToString(list.vm_type()) +
         ", vm_name: " + ToString(list.vm_name()) +
         ", container_name: " + ToString(list.container_name()) +
         ", owner_id: " + ToString(list.owner_id()) + "}";
}

template <typename List>
static std::string Join(const List& list) {
  std::string joined = "[";
  const char* seperator = "";
  for (const auto& list_item : list) {
    joined += seperator + ToString(list_item);
    seperator = ", ";
  }
  joined += "]";
  return joined;
}

void SetLocaleString(App::LocaleString* locale_string,
                     const std::string& locale,
                     const std::string& value) {
  DCHECK(!locale.empty());
  App::LocaleString::Entry* entry = locale_string->add_values();
  // Add both specified locale, and empty default.
  for (auto& l : {locale, std::string()}) {
    entry->set_locale(l);
    entry->set_value(value);
  }
}

void SetLocaleStrings(App::LocaleStrings* locale_strings,
                      const std::string& locale,
                      std::vector<std::string> values) {
  DCHECK(!locale.empty());
  App::LocaleStrings::StringsWithLocale* strings = locale_strings->add_values();
  // Add both specified locale, and empty default.
  for (auto& l : {locale, std::string()}) {
    strings->set_locale(l);
    for (auto& v : values) {
      strings->add_value(v);
    }
  }
}

// Creates a Terminal registration using partial values from prefs such as
// last_launch_time.
GuestOsRegistryService::Registration GetTerminalRegistration(
    const base::Value* pref) {
  std::string locale =
      l10n_util::NormalizeLocale(g_browser_process->GetApplicationLocale());
  vm_tools::apps::App app;
  SetLocaleString(app.mutable_name(), locale,
                  l10n_util::GetStringUTF8(IDS_CROSTINI_TERMINAL_APP_NAME));
  app.add_mime_types(
      extensions::app_file_handler_util::kMimeTypeInodeDirectory);
  SetLocaleStrings(
      app.mutable_keywords(), locale,
      {"linux", "terminal", "crostini",
       l10n_util::GetStringUTF8(IDS_CROSTINI_TERMINAL_APP_SEARCH_TERMS)});

  base::Value pref_registration =
      pref ? pref->Clone() : base::Value(base::Value::Type::DICTIONARY);
  PopulatePrefRegistrationFromApp(
      pref_registration,
      GuestOsRegistryService::VmType::ApplicationList_VmType_TERMINA,
      crostini::kCrostiniDefaultVmName, crostini::kCrostiniDefaultContainerName,
      app, ProtoToDictionary(app.name()));
  return GuestOsRegistryService::Registration(
      crostini::kCrostiniTerminalSystemAppId, std::move(pref_registration));
}

}  // namespace

GuestOsRegistryService::Registration::Registration(std::string app_id,
                                                   base::Value pref)
    : app_id_(std::move(app_id)), pref_(std::move(pref)) {}

GuestOsRegistryService::Registration::~Registration() = default;

std::string GuestOsRegistryService::Registration::DesktopFileId() const {
  return pref_
      .FindKeyOfType(guest_os::prefs::kAppDesktopFileIdKey,
                     base::Value::Type::STRING)
      ->GetString();
}

GuestOsRegistryService::VmType GuestOsRegistryService::Registration::VmType()
    const {
  // The VmType field is new, existing Apps that do not include it must be
  // TERMINA (0) Apps, as Plugin VM apps are not yet in production.
  return static_cast<GuestOsRegistryService::VmType>(
      pref_.FindIntKey(guest_os::prefs::kAppVmTypeKey).value_or(0));
}

std::string GuestOsRegistryService::Registration::VmName() const {
  return pref_
      .FindKeyOfType(guest_os::prefs::kAppVmNameKey, base::Value::Type::STRING)
      ->GetString();
}

std::string GuestOsRegistryService::Registration::ContainerName() const {
  return pref_
      .FindKeyOfType(guest_os::prefs::kAppContainerNameKey,
                     base::Value::Type::STRING)
      ->GetString();
}

std::string GuestOsRegistryService::Registration::Name() const {
  if (VmType() == VmType::ApplicationList_VmType_PLUGIN_VM) {
    return l10n_util::GetStringFUTF8(
        IDS_PLUGIN_VM_APP_NAME_WINDOWS_SUFFIX,
        base::UTF8ToUTF16(LocalizedString(guest_os::prefs::kAppNameKey)));
  }
  return LocalizedString(guest_os::prefs::kAppNameKey);
}

std::string GuestOsRegistryService::Registration::Comment() const {
  return LocalizedString(guest_os::prefs::kAppCommentKey);
}

std::string GuestOsRegistryService::Registration::Exec() const {
  return pref_
      .FindKeyOfType(guest_os::prefs::kAppExecKey, base::Value::Type::STRING)
      ->GetString();
}

std::string GuestOsRegistryService::Registration::ExecutableFileName() const {
  if (pref_.is_none())
    return std::string();
  const base::Value* executable_file_name = pref_.FindKeyOfType(
      guest_os::prefs::kAppExecutableFileNameKey, base::Value::Type::STRING);
  if (!executable_file_name)
    return std::string();
  return executable_file_name->GetString();
}

std::set<std::string> GuestOsRegistryService::Registration::Extensions() const {
  if (pref_.is_none())
    return {};
  // Convert to lowercase ASCII to allow case-insensitive match.
  return ListToStringSet(pref_.FindKeyOfType(guest_os::prefs::kAppExtensionsKey,
                                             base::Value::Type::LIST),
                         /*to_lower_ascii=*/true);
}

std::set<std::string> GuestOsRegistryService::Registration::MimeTypes() const {
  if (pref_.is_none())
    return {};
  // Convert to lowercase ASCII to allow case-insensitive match.
  return ListToStringSet(pref_.FindKeyOfType(guest_os::prefs::kAppMimeTypesKey,
                                             base::Value::Type::LIST),
                         /*to_lower_ascii=*/true);
}

std::set<std::string> GuestOsRegistryService::Registration::Keywords() const {
  return LocalizedList(guest_os::prefs::kAppKeywordsKey);
}

bool GuestOsRegistryService::Registration::NoDisplay() const {
  if (pref_.is_none())
    return false;
  const base::Value* no_display = pref_.FindKeyOfType(
      guest_os::prefs::kAppNoDisplayKey, base::Value::Type::BOOLEAN);
  if (no_display)
    return no_display->GetBool();
  return false;
}

std::string GuestOsRegistryService::Registration::PackageId() const {
  if (pref_.is_none())
    return std::string();
  const base::Value* package_id = pref_.FindKeyOfType(
      guest_os::prefs::kAppPackageIdKey, base::Value::Type::STRING);
  if (!package_id)
    return std::string();
  return package_id->GetString();
}

bool GuestOsRegistryService::Registration::CanUninstall() const {
  if (pref_.is_none())
    return false;
  // We can uninstall if and only if there is a package that owns the
  // application. If no package owns the application, we don't know how to
  // uninstall the app.
  //
  // We don't check other things that might prevent us from uninstalling the
  // app. In particular, we don't check if there are other packages which
  // depend on the owning package. This should be rare for packages that have
  // desktop files, and it's better to show an error message (which the user can
  // then Google to learn more) than to just not have an uninstall option at
  // all.
  const base::Value* package_id = pref_.FindKeyOfType(
      guest_os::prefs::kAppPackageIdKey, base::Value::Type::STRING);
  if (package_id)
    return !package_id->GetString().empty();
  return false;
}

base::Time GuestOsRegistryService::Registration::InstallTime() const {
  return GetTime(pref_, guest_os::prefs::kAppInstallTimeKey);
}

base::Time GuestOsRegistryService::Registration::LastLaunchTime() const {
  return GetTime(pref_, guest_os::prefs::kAppLastLaunchTimeKey);
}

bool GuestOsRegistryService::Registration::IsScaled() const {
  if (pref_.is_none())
    return false;
  const base::Value* scaled = pref_.FindKeyOfType(
      guest_os::prefs::kAppScaledKey, base::Value::Type::BOOLEAN);
  if (!scaled)
    return false;
  return scaled->GetBool();
}

// We store in prefs all the localized values for given fields (formatted with
// undescores, e.g. 'fr' or 'en_US'), but users of the registry don't need to
// deal with this.
std::string GuestOsRegistryService::Registration::LocalizedString(
    base::StringPiece key) const {
  if (pref_.is_none())
    return std::string();
  const base::Value* dict =
      pref_.FindKeyOfType(key, base::Value::Type::DICTIONARY);
  if (!dict)
    return std::string();

  std::string current_locale =
      l10n_util::NormalizeLocale(g_browser_process->GetApplicationLocale());
  std::vector<std::string> locales;
  l10n_util::GetParentLocales(current_locale, &locales);
  // We use an empty locale as fallback.
  locales.push_back(std::string());

  for (const std::string& locale : locales) {
    const base::Value* value =
        dict->FindKeyOfType(locale, base::Value::Type::STRING);
    if (value)
      return value->GetString();
  }
  return std::string();
}

std::set<std::string> GuestOsRegistryService::Registration::LocalizedList(
    base::StringPiece key) const {
  if (pref_.is_none())
    return {};
  const base::Value* dict =
      pref_.FindKeyOfType(key, base::Value::Type::DICTIONARY);
  if (!dict)
    return {};

  std::string current_locale =
      l10n_util::NormalizeLocale(g_browser_process->GetApplicationLocale());
  std::vector<std::string> locales;
  l10n_util::GetParentLocales(current_locale, &locales);
  // We use an empty locale as fallback.
  locales.push_back(std::string());

  for (const std::string& locale : locales) {
    const base::Value* value =
        dict->FindKeyOfType(locale, base::Value::Type::LIST);
    if (value)
      return ListToStringSet(value);
  }
  return {};
}

GuestOsRegistryService::GuestOsRegistryService(Profile* profile)
    : profile_(profile),
      prefs_(profile->GetPrefs()),
      base_icon_path_(profile->GetPath().AppendASCII(kCrostiniIconFolder)),
      clock_(base::DefaultClock::GetInstance()) {
  RecordStartupMetrics();
  MigrateTerminal();
}

GuestOsRegistryService::~GuestOsRegistryService() = default;

base::WeakPtr<GuestOsRegistryService> GuestOsRegistryService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::map<std::string, GuestOsRegistryService::Registration>
GuestOsRegistryService::GetAllRegisteredApps() const {
  const base::DictionaryValue* apps =
      prefs_->GetDictionary(guest_os::prefs::kGuestOsRegistry);
  std::map<std::string, GuestOsRegistryService::Registration> result;
  // Register Terminal by merging optional prefs with app values.
  // TODO(crbug.com/1028898): Register Terminal as a System App rather than a
  // crostini app.
  result.emplace(crostini::kCrostiniTerminalSystemAppId,
                 GetTerminalRegistration(
                     apps->FindKeyOfType(crostini::kCrostiniTerminalSystemAppId,
                                         base::Value::Type::DICTIONARY)));
  for (const auto item : apps->DictItems()) {
    if (item.first != crostini::kCrostiniTerminalSystemAppId) {
      result.emplace(item.first, Registration(item.first, item.second.Clone()));
    }
  }
  return result;
}

std::map<std::string, GuestOsRegistryService::Registration>
GuestOsRegistryService::GetEnabledApps() const {
  bool crostini_enabled =
      crostini::CrostiniFeatures::Get()->IsEnabled(profile_);
  bool plugin_vm_enabled =
      plugin_vm::PluginVmFeatures::Get()->IsEnabled(profile_);
  bool borealis_enabled = borealis::BorealisService::GetForProfile(profile_)
                              ->Features()
                              .IsEnabled();
  if (!crostini_enabled && !plugin_vm_enabled && !borealis_enabled)
    return {};

  auto apps = GetAllRegisteredApps();
  for (auto it = apps.cbegin(); it != apps.cend();) {
    bool enabled = false;
    switch (it->second.VmType()) {
      case VmType::ApplicationList_VmType_TERMINA:
        enabled = crostini_enabled;
        break;
      case VmType::ApplicationList_VmType_PLUGIN_VM:
        enabled = plugin_vm_enabled;
        break;
      case VmType::ApplicationList_VmType_BOREALIS:
        enabled = borealis_enabled;
        break;
      default:
        LOG(ERROR) << "Unsupported VmType: "
                   << static_cast<int>(it->second.VmType());
    }
    if (enabled) {
      ++it;
    } else {
      it = apps.erase(it);
    }
  }
  return apps;
}

std::map<std::string, GuestOsRegistryService::Registration>
GuestOsRegistryService::GetRegisteredApps(VmType vm_type) const {
  auto apps = GetAllRegisteredApps();
  for (auto it = apps.cbegin(); it != apps.cend();) {
    if (it->second.VmType() == vm_type) {
      ++it;
    } else {
      it = apps.erase(it);
    }
  }
  return apps;
}

absl::optional<GuestOsRegistryService::Registration>
GuestOsRegistryService::GetRegistration(const std::string& app_id) const {
  const base::DictionaryValue* apps =
      prefs_->GetDictionary(guest_os::prefs::kGuestOsRegistry);

  if (app_id == crostini::kCrostiniTerminalSystemAppId) {
    return GetTerminalRegistration(apps->FindKeyOfType(
        crostini::kCrostiniTerminalSystemAppId, base::Value::Type::DICTIONARY));
  }

  const base::Value* pref_registration =
      apps->FindKeyOfType(app_id, base::Value::Type::DICTIONARY);
  if (!pref_registration)
    return absl::nullopt;
  return absl::make_optional<Registration>(app_id, pref_registration->Clone());
}

void GuestOsRegistryService::RecordStartupMetrics() {
  const base::DictionaryValue* apps =
      prefs_->GetDictionary(guest_os::prefs::kGuestOsRegistry);

  base::flat_map<int, int> num_apps;

  for (const auto item : apps->DictItems()) {
    if (item.first == crostini::kCrostiniTerminalSystemAppId)
      continue;

    absl::optional<bool> no_display =
        item.second.FindBoolKey(guest_os::prefs::kAppNoDisplayKey);
    if (no_display && no_display.value())
      continue;

    int vm_type =
        item.second.FindIntKey(guest_os::prefs::kAppVmTypeKey).value_or(0);
    num_apps[vm_type]++;
  }

  if (crostini::CrostiniFeatures::Get()->IsEnabled(profile_)) {
    UMA_HISTOGRAM_COUNTS_1000(kCrostiniAppsInstalledHistogram,
                              num_apps[VmType::ApplicationList_VmType_TERMINA]);
  }
  if (plugin_vm::PluginVmFeatures::Get()->IsEnabled(profile_)) {
    UMA_HISTOGRAM_COUNTS_1000(
        kPluginVmAppsInstalledHistogram,
        num_apps[VmType::ApplicationList_VmType_PLUGIN_VM]);
  }
  if (borealis::BorealisService::GetForProfile(profile_)
          ->Features()
          .IsEnabled()) {
    UMA_HISTOGRAM_COUNTS_1000(
        kBorealisAppsInstalledHistogram,
        num_apps[VmType::ApplicationList_VmType_BOREALIS]);
  }
}

base::FilePath GuestOsRegistryService::GetAppPath(
    const std::string& app_id) const {
  return base_icon_path_.AppendASCII(app_id);
}

base::FilePath GuestOsRegistryService::GetIconPath(
    const std::string& app_id,
    ui::ResourceScaleFactor scale_factor) const {
  const base::FilePath app_path = GetAppPath(app_id);
  switch (scale_factor) {
    case ui::k100Percent:
      return app_path.AppendASCII("icon_100p.png");
    case ui::k200Percent:
      return app_path.AppendASCII("icon_200p.png");
    case ui::k300Percent:
      return app_path.AppendASCII("icon_300p.png");
    default:
      NOTREACHED();
      return base::FilePath();
  }
}

void GuestOsRegistryService::LoadIcon(
    const std::string& app_id,
    apps::mojom::IconKeyPtr icon_key,
    apps::mojom::IconType icon_type,
    int32_t size_hint_in_dip,
    bool allow_placeholder_icon,
    int fallback_icon_resource_id,
    apps::mojom::Publisher::LoadIconCallback callback) {
  if (icon_key) {
    if (icon_key->resource_id != apps::mojom::IconKey::kInvalidResourceId) {
      // The icon is a resource built into the Chrome OS binary.
      constexpr bool is_placeholder_icon = false;
      apps::LoadIconFromResource(
          icon_type, size_hint_in_dip, icon_key->resource_id,
          is_placeholder_icon,
          static_cast<apps::IconEffects>(icon_key->icon_effects),
          std::move(callback));
      return;
    } else {
      // There are paths where nothing higher up the call stack will resize so
      // we need to ensure that returned icons are always resized to be
      // size_hint_in_dip big. crbug/1170455 is an example.
      icon_key->icon_effects |= apps::IconEffects::kResizeAndPad;
      auto scale_factor = apps_util::GetPrimaryDisplayUIScaleFactor();

      // Try loading the icon from an on-disk cache. If that fails, fall back
      // to LoadIconFromVM.
      apps::LoadIconFromFileWithFallback(
          icon_type, size_hint_in_dip, GetIconPath(app_id, scale_factor),
          static_cast<apps::IconEffects>(icon_key->icon_effects),
          std::move(callback),
          base::BindOnce(&GuestOsRegistryService::LoadIconFromVM,
                         weak_ptr_factory_.GetWeakPtr(), app_id, icon_type,
                         size_hint_in_dip, scale_factor,
                         static_cast<apps::IconEffects>(icon_key->icon_effects),
                         fallback_icon_resource_id));
      return;
    }
  }

  // On failure, we still run the callback, with the zero IconValue.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

void GuestOsRegistryService::LoadIconFromVM(
    const std::string& app_id,
    apps::mojom::IconType icon_type,
    int32_t size_hint_in_dip,
    ui::ResourceScaleFactor scale_factor,
    apps::IconEffects icon_effects,
    int fallback_icon_resource_id,
    apps::mojom::Publisher::LoadIconCallback callback) {
  RequestIcon(app_id, scale_factor,
              base::BindOnce(&GuestOsRegistryService::OnLoadIconFromVM,
                             weak_ptr_factory_.GetWeakPtr(), app_id, icon_type,
                             size_hint_in_dip, icon_effects,
                             fallback_icon_resource_id, std::move(callback)));
}

void GuestOsRegistryService::OnLoadIconFromVM(
    const std::string& app_id,
    apps::mojom::IconType icon_type,
    int32_t size_hint_in_dip,
    apps::IconEffects icon_effects,
    int fallback_icon_resource_id,
    apps::mojom::Publisher::LoadIconCallback callback,
    std::string compressed_icon_data) {
  if (compressed_icon_data.empty()) {
    if (fallback_icon_resource_id != apps::mojom::IconKey::kInvalidResourceId) {
      // We load the fallback icon, but we tell AppsService that this is not
      // a placeholder to avoid endless repeat calls since we don't expect to
      // find a better icon than this any time soon.
      apps::LoadIconFromResource(
          icon_type, size_hint_in_dip, fallback_icon_resource_id,
          /*is_placeholder_icon=*/false, icon_effects, std::move(callback));
    } else {
      std::move(callback).Run(apps::mojom::IconValue::New());
    }
  } else {
    apps::LoadIconFromCompressedData(icon_type, size_hint_in_dip, icon_effects,
                                     compressed_icon_data, std::move(callback));
  }
}

void GuestOsRegistryService::RequestIcon(
    const std::string& app_id,
    ui::ResourceScaleFactor scale_factor,
    base::OnceCallback<void(std::string)> callback) {
  if (!GetRegistration(app_id)) {
    // App isn't registered (e.g. a GUI app launched from within Crostini
    // that doesn't have a .desktop file). Can't get an icon for that case so
    // return an empty icon.
    std::move(callback).Run({});
    return;
  }

  // Coalesce calls to the container.
  auto& callbacks = active_icon_requests_[{app_id, scale_factor}];
  callbacks.emplace_back(std::move(callback));
  if (callbacks.size() > 1) {
    return;
  }
  RequestContainerAppIcon(app_id, scale_factor);
}

void GuestOsRegistryService::ClearApplicationList(
    VmType vm_type,
    const std::string& vm_name,
    const std::string& container_name) {
  std::vector<std::string> removed_apps;
  // The DictionaryPrefUpdate should be destructed before calling the observer.
  {
    DictionaryPrefUpdate update(prefs_, guest_os::prefs::kGuestOsRegistry);
    base::DictionaryValue* apps = update.Get();

    for (const auto item : apps->DictItems()) {
      if (item.first == crostini::kCrostiniTerminalSystemAppId)
        continue;
      Registration registration(item.first, item.second.Clone());
      if (vm_type != registration.VmType())
        continue;
      if (vm_name != registration.VmName())
        continue;
      if (!container_name.empty() &&
          container_name != registration.ContainerName()) {
        continue;
      }
      removed_apps.push_back(item.first);
    }
    for (const std::string& removed_app : removed_apps) {
      RemoveAppData(removed_app);
      apps->RemoveKey(removed_app);
    }
  }

  if (removed_apps.empty())
    return;

  std::vector<std::string> updated_apps;
  std::vector<std::string> inserted_apps;
  for (Observer& obs : observers_) {
    obs.OnRegistryUpdated(this, vm_type, updated_apps, removed_apps,
                          inserted_apps);
  }
}

void GuestOsRegistryService::UpdateApplicationList(
    const vm_tools::apps::ApplicationList& app_list) {
  VLOG(1) << "Received ApplicationList : " << ToString(app_list);

  if (app_list.vm_name().empty()) {
    LOG(WARNING) << "Received app list with missing VM name";
    return;
  }
  if (app_list.container_name().empty()) {
    LOG(WARNING) << "Received app list with missing container name";
    return;
  }

  // We need to compute the diff between the new list of apps and the old list
  // of apps (with matching vm/container names). We keep a set of the new app
  // ids so that we can compute these and update the Dictionary directly.
  std::set<std::string> new_app_ids;
  std::vector<std::string> updated_apps;
  std::vector<std::string> removed_apps;
  std::vector<std::string> inserted_apps;

  // The DictionaryPrefUpdate should be destructed before calling the observer.
  {
    DictionaryPrefUpdate update(prefs_, guest_os::prefs::kGuestOsRegistry);
    base::DictionaryValue* apps = update.Get();
    for (const App& app : app_list.apps()) {
      if (app.desktop_file_id().empty()) {
        LOG(WARNING) << "Received app with missing desktop file id";
        continue;
      }

      base::Value name = ProtoToDictionary(app.name());
      if (name.FindKey(base::StringPiece()) == nullptr) {
        LOG(WARNING) << "Received app '" << app.desktop_file_id()
                     << "' with missing unlocalized name";
        continue;
      }

      std::string app_id = GenerateAppId(
          app.desktop_file_id(), app_list.vm_name(), app_list.container_name());
      new_app_ids.insert(app_id);

      base::Value pref_registration(base::Value::Type::DICTIONARY);
      PopulatePrefRegistrationFromApp(
          pref_registration, app_list.vm_type(), app_list.vm_name(),
          app_list.container_name(), app, std::move(name));

      base::Value* old_app = apps->FindKey(app_id);
      if (old_app && EqualsExcludingTimestamps(pref_registration, *old_app))
        continue;

      base::Value* old_install_time = nullptr;
      base::Value* old_last_launch_time = nullptr;
      if (old_app) {
        updated_apps.push_back(app_id);
        old_install_time =
            old_app->FindKey(guest_os::prefs::kAppInstallTimeKey);
        old_last_launch_time =
            old_app->FindKey(guest_os::prefs::kAppLastLaunchTimeKey);
      } else {
        inserted_apps.push_back(app_id);
      }

      if (old_install_time)
        pref_registration.SetKey(guest_os::prefs::kAppInstallTimeKey,
                                 old_install_time->Clone());
      else
        SetCurrentTime(&pref_registration, guest_os::prefs::kAppInstallTimeKey);

      if (old_last_launch_time) {
        pref_registration.SetKey(guest_os::prefs::kAppLastLaunchTimeKey,
                                 old_last_launch_time->Clone());
      }

      apps->SetKey(app_id, std::move(pref_registration));
    }

    for (const auto item : apps->DictItems()) {
      if (item.first == crostini::kCrostiniTerminalSystemAppId)
        continue;
      if (item.second.FindKey(guest_os::prefs::kAppVmNameKey)->GetString() ==
              app_list.vm_name() &&
          item.second.FindKey(guest_os::prefs::kAppContainerNameKey)
                  ->GetString() == app_list.container_name() &&
          new_app_ids.find(item.first) == new_app_ids.end()) {
        removed_apps.push_back(item.first);
      }
    }

    for (const std::string& removed_app : removed_apps) {
      RemoveAppData(removed_app);
      apps->RemoveKey(removed_app);
    }
  }

  // When we receive notification of the application list then the container
  // *should* be online and we can retry all of our icon requests that failed
  // due to the container being offline.
  for (auto retry_iter = retry_icon_requests_.begin();
       retry_iter != retry_icon_requests_.end(); ++retry_iter) {
    for (ui::ResourceScaleFactor scale_factor :
         ui::GetSupportedResourceScaleFactors()) {
      if (retry_iter->second & (1 << scale_factor)) {
        RequestContainerAppIcon(retry_iter->first, scale_factor);
      }
    }
  }
  retry_icon_requests_.clear();

  if (updated_apps.empty() && removed_apps.empty() && inserted_apps.empty())
    return;

  for (Observer& obs : observers_) {
    obs.OnRegistryUpdated(this, app_list.vm_type(), updated_apps, removed_apps,
                          inserted_apps);
  }
}

void GuestOsRegistryService::RemoveAppData(const std::string& app_id) {
  // Remove any pending requests we have for this icon.
  retry_icon_requests_.erase(app_id);

  // Remove local data on filesystem for the icons.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&DeleteIconFolderFromFileThread, GetAppPath(app_id)));
}

void GuestOsRegistryService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void GuestOsRegistryService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void GuestOsRegistryService::AppLaunched(const std::string& app_id) {
  DictionaryPrefUpdate update(prefs_, guest_os::prefs::kGuestOsRegistry);
  base::DictionaryValue* apps = update.Get();

  base::Value* app = apps->FindKey(app_id);
  if (!app) {
    DCHECK_EQ(app_id, crostini::kCrostiniTerminalSystemAppId);
    base::Value pref(base::Value::Type::DICTIONARY);
    SetCurrentTime(&pref, guest_os::prefs::kAppLastLaunchTimeKey);
    apps->SetKey(app_id, std::move(pref));
    return;
  }

  SetCurrentTime(app, guest_os::prefs::kAppLastLaunchTimeKey);
}

void GuestOsRegistryService::SetCurrentTime(base::Value* dictionary,
                                            const char* key) const {
  DCHECK(dictionary);
  int64_t time = clock_->Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
  dictionary->SetKey(key, base::Value(base::NumberToString(time)));
}

void GuestOsRegistryService::SetAppScaled(const std::string& app_id,
                                          bool scaled) {
  DCHECK_NE(app_id, crostini::kCrostiniTerminalSystemAppId);

  DictionaryPrefUpdate update(prefs_, guest_os::prefs::kGuestOsRegistry);
  base::DictionaryValue* apps = update.Get();

  base::Value* app = apps->FindKey(app_id);
  if (!app) {
    LOG(ERROR)
        << "Tried to set display scaled property on the app with this app_id "
        << app_id << " that doesn't exist in the registry.";
    return;
  }
  app->SetKey(guest_os::prefs::kAppScaledKey, base::Value(scaled));
}

// static
std::string GuestOsRegistryService::GenerateAppId(
    const std::string& desktop_file_id,
    const std::string& vm_name,
    const std::string& container_name) {
  // These can collide in theory because the user could choose VM and container
  // names which contain slashes, but this will only result in apps missing from
  // the launcher.
  return crx_file::id_util::GenerateId(kCrostiniAppIdPrefix + vm_name + "/" +
                                       container_name + "/" + desktop_file_id);
}

void GuestOsRegistryService::RequestContainerAppIcon(
    const std::string& app_id,
    ui::ResourceScaleFactor scale_factor) {
  // Ignore requests for app_id that isn't registered.
  absl::optional<GuestOsRegistryService::Registration> registration =
      GetRegistration(app_id);
  DCHECK(registration);
  if (!registration) {
    LOG(ERROR) << "Request to load icon for non-registered app: " << app_id;
    return;
  }
  VLOG(1) << "Request to load icon for app: " << app_id;

  // Now make the call to request the actual icon.
  std::vector<std::string> desktop_file_ids{registration->DesktopFileId()};
  // We can only send integer scale factors to Crostini, so if we have a
  // non-integral scale factor we need round the scale factor. We do not expect
  // Crostini to give us back exactly what we ask for and we deal with that in
  // the CrostiniAppIcon class and may rescale the result in there to match our
  // needs.
  uint32_t icon_scale = 1;
  switch (scale_factor) {
    case ui::k200Percent:
      icon_scale = 2;
      break;
    case ui::k300Percent:
      icon_scale = 3;
      break;
    default:
      break;
  }

  crostini::CrostiniManager::GetForProfile(profile_)->GetContainerAppIcons(
      crostini::ContainerId(registration->VmName(),
                            registration->ContainerName()),
      desktop_file_ids,
      ash::SharedAppListConfig::instance().default_grid_icon_dimension(),
      icon_scale,
      base::BindOnce(&GuestOsRegistryService::OnContainerAppIcon,
                     weak_ptr_factory_.GetWeakPtr(), app_id, scale_factor));
}

void GuestOsRegistryService::OnContainerAppIcon(
    const std::string& app_id,
    ui::ResourceScaleFactor scale_factor,
    bool success,
    const std::vector<crostini::Icon>& icons) {
  std::string icon_content;
  if (!success) {
    VLOG(1) << "Failed to load icon for app: " << app_id;
    // Add this to the list of retryable icon requests so we redo this when
    // we get feedback from the container that it's available.
    retry_icon_requests_[app_id] |= (1 << scale_factor);
  } else if (icons.empty()) {
    VLOG(1) << "No icon in container for app: " << app_id;
  } else {
    VLOG(1) << "Found icon in container for app: " << app_id;
    // Now install the icon that we received.
    const base::FilePath icon_path = GetIconPath(app_id, scale_factor);
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&InstallIconFromFileThread, icon_path,
                       icons[0].content));
    icon_content = std::move(icons[0].content);
  }

  // Invoke all active icon request callbacks with the icon.
  auto key =
      std::pair<std::string, ui::ResourceScaleFactor>(app_id, scale_factor);
  auto& callbacks = active_icon_requests_[key];
  VLOG(1) << "Invoking icon callbacks for app: " << app_id
          << ", num callbacks: " << callbacks.size();
  for (auto& callback : callbacks) {
    std::move(callback).Run(icon_content);
  }
  active_icon_requests_.erase(key);
}

void GuestOsRegistryService::MigrateTerminal() const {
  // Remove the old terminal from the registry.
  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              guest_os::prefs::kGuestOsRegistry);
  base::DictionaryValue* apps = update.Get();
  apps->RemoveKey(crostini::kCrostiniDeletedTerminalId);

  // Transfer item attributes from old terminal to new, and delete old terminal
  // once AppListSyncableService is initialized.
  auto* app_list_syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile_);
  if (!app_list_syncable_service) {
    return;
  }
  app_list_syncable_service->on_initialized().Post(
      FROM_HERE,
      base::BindOnce(
          [](app_list::AppListSyncableService* service) {
            if (service->GetSyncItem(crostini::kCrostiniDeletedTerminalId)) {
              service->TransferItemAttributes(
                  crostini::kCrostiniDeletedTerminalId,
                  crostini::kCrostiniTerminalSystemAppId);
              service->RemoveItem(crostini::kCrostiniDeletedTerminalId);
            }
          },
          base::Unretained(app_list_syncable_service)));
}

}  // namespace guest_os
