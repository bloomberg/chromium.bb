// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extensions_internals_source.h"

#include <string>
#include <utility>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/activity.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"

namespace {

const char* TypeToString(extensions::Manifest::Type type) {
  switch (type) {
    case extensions::Manifest::TYPE_UNKNOWN:
      return "TYPE_UNKNOWN";
    case extensions::Manifest::TYPE_EXTENSION:
      return "TYPE_EXTENSION";
    case extensions::Manifest::TYPE_THEME:
      return "TYPE_THEME";
    case extensions::Manifest::TYPE_USER_SCRIPT:
      return "TYPE_USER_SCRIPT";
    case extensions::Manifest::TYPE_HOSTED_APP:
      return "TYPE_HOSTED_APP";
    case extensions::Manifest::TYPE_LEGACY_PACKAGED_APP:
      return "TYPE_LEGACY_PACKAGED_APP";
    case extensions::Manifest::TYPE_PLATFORM_APP:
      return "TYPE_PLATFORM_APP";
    case extensions::Manifest::TYPE_SHARED_MODULE:
      return "TYPE_SHARED_MODULE";
    case extensions::Manifest::NUM_LOAD_TYPES:
      break;
  }
  NOTREACHED();
  return "";
}

const char* LocationToString(extensions::Manifest::Location loc) {
  switch (loc) {
    case extensions::Manifest::INVALID_LOCATION:
      return "INVALID_LOCATION";
    case extensions::Manifest::INTERNAL:
      return "INTERNAL";
    case extensions::Manifest::EXTERNAL_PREF:
      return "EXTERNAL_PREF";
    case extensions::Manifest::EXTERNAL_REGISTRY:
      return "EXTERNAL_REGISTRY";
    case extensions::Manifest::UNPACKED:
      return "UNPACKED";
    case extensions::Manifest::COMPONENT:
      return "COMPONENT";
    case extensions::Manifest::EXTERNAL_PREF_DOWNLOAD:
      return "EXTERNAL_PREF_DOWNLOAD";
    case extensions::Manifest::EXTERNAL_POLICY_DOWNLOAD:
      return "EXTERNAL_POLICY_DOWNLOAD";
    case extensions::Manifest::COMMAND_LINE:
      return "COMMAND_LINE";
    case extensions::Manifest::EXTERNAL_POLICY:
      return "EXTERNAL_POLICY";
    case extensions::Manifest::EXTERNAL_COMPONENT:
      return "EXTERNAL_COMPONENT";
    case extensions::Manifest::NUM_LOCATIONS:
      break;
  }
  NOTREACHED();
  return "";
}

base::Value FormatKeepaliveData(extensions::ProcessManager* process_manager,
                                const extensions::Extension* extension) {
  base::Value keepalive_data(base::Value::Type::DICTIONARY);
  keepalive_data.SetKey(
      "count", base::Value(process_manager->GetLazyKeepaliveCount(extension)));
  const extensions::ProcessManager::ActivitiesMultiset activities =
      process_manager->GetLazyKeepaliveActivities(extension);
  base::Value activities_data(base::Value::Type::LIST);
  activities_data.GetList().reserve(activities.size());
  for (const auto& activity : activities) {
    base::Value activities_entry(base::Value::Type::DICTIONARY);
    activities_entry.SetKey(
        "type", base::Value(extensions::Activity::ToString(activity.first)));
    activities_entry.SetKey("extra_data", base::Value(activity.second));
    activities_data.GetList().push_back(std::move(activities_entry));
  }
  keepalive_data.SetKey("activites", std::move(activities_data));
  return keepalive_data;
}

}  // namespace

ExtensionsInternalsSource::ExtensionsInternalsSource(Profile* profile)
    : profile_(profile) {}

ExtensionsInternalsSource::~ExtensionsInternalsSource() = default;

std::string ExtensionsInternalsSource::GetSource() const {
  return chrome::kChromeUIExtensionsInternalsHost;
}

std::string ExtensionsInternalsSource::GetMimeType(
    const std::string& path) const {
  return "text/plain";
}

void ExtensionsInternalsSource::StartDataRequest(
    const std::string& path,
    const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  std::string json = WriteToString();
  callback.Run(base::RefCountedString::TakeString(&json));
}

std::string ExtensionsInternalsSource::WriteToString() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<extensions::ExtensionSet> extensions =
      extensions::ExtensionRegistry::Get(profile_)
          ->GenerateInstalledExtensionsSet();
  extensions::ProcessManager* process_manager =
      extensions::ProcessManager::Get(profile_);
  base::Value data(base::Value::Type::LIST);
  for (const auto& extension : *extensions) {
    base::Value extension_data(base::Value::Type::DICTIONARY);
    extension_data.SetKey("id", base::Value(extension->id()));
    extension_data.SetKey(
        "keepalive", FormatKeepaliveData(process_manager, extension.get()));
    extension_data.SetKey("location",
                          base::Value(LocationToString(extension->location())));
    extension_data.SetKey("manifest_version",
                          base::Value(extension->manifest_version()));
    extension_data.SetKey("name", base::Value(extension->name()));
    extension_data.SetKey("path",
                          base::Value(extension->path().LossyDisplayName()));
    extension_data.SetKey("type",
                          base::Value(TypeToString(extension->GetType())));
    extension_data.SetKey("version",
                          base::Value(extension->GetVersionForDisplay()));
    data.GetList().push_back(std::move(extension_data));
  }

  std::string json;
  base::JSONWriter::WriteWithOptions(
      data, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);

  return json;
}
