// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_tasks.h"

#include <stddef.h>

#include <map>
#include <string>
#include <utility>

#include "apps/launcher.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_metrics.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/arc_file_tasks.h"
#include "chrome/browser/chromeos/file_manager/file_browser_handlers.h"
#include "chrome/browser/chromeos/file_manager/file_tasks_notifier.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/guest_os_file_tasks.h"
#include "chrome/browser/chromeos/file_manager/open_util.h"
#include "chrome/browser/chromeos/file_manager/open_with_browser.h"
#include "chrome/browser/chromeos/file_manager/web_file_tasks.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/chromeos/web_applications/default_web_app_ids.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/extensions/api/file_browser_handlers/file_browser_handler.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/drive/drive_api_util.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/file_handler_info.h"
#include "extensions/browser/api/file_handlers/mime_util.h"
#include "extensions/browser/entry_info.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_set.h"
#include "net/base/mime_util.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "ui/base/window_open_disposition.h"

using extensions::Extension;
using extensions::api::file_manager_private::Verb;
using extensions::app_file_handler_util::FindFileHandlerMatchesForEntries;
using storage::FileSystemURL;

namespace file_manager {
namespace file_tasks {

namespace {

// The values "file" and "app" are confusing, but cannot be changed easily as
// these are used in default task IDs stored in preferences.
const char kFileBrowserHandlerTaskType[] = "file";
const char kFileHandlerTaskType[] = "app";
const char kArcAppTaskType[] = "arc";
const char kCrostiniAppTaskType[] = "crostini";
const char kPluginVmAppTaskType[] = "pluginvm";
const char kWebAppTaskType[] = "web";
const char kImportCrostiniImageHandlerId[] = "import-crostini-image";
const char kInstallLinuxPackageHandlerId[] = "install-linux-package";

// Converts a TaskType to a string.
std::string TaskTypeToString(TaskType task_type) {
  switch (task_type) {
    case TASK_TYPE_FILE_BROWSER_HANDLER:
      return kFileBrowserHandlerTaskType;
    case TASK_TYPE_FILE_HANDLER:
      return kFileHandlerTaskType;
    case TASK_TYPE_ARC_APP:
      return kArcAppTaskType;
    case TASK_TYPE_CROSTINI_APP:
      return kCrostiniAppTaskType;
    case TASK_TYPE_WEB_APP:
      return kWebAppTaskType;
    case TASK_TYPE_PLUGIN_VM_APP:
      return kPluginVmAppTaskType;
    case TASK_TYPE_UNKNOWN:
    case DEPRECATED_TASK_TYPE_DRIVE_APP:
    case NUM_TASK_TYPE:
      break;
  }
  NOTREACHED();
  return "";
}

// Converts a string to a TaskType. Returns TASK_TYPE_UNKNOWN on error.
TaskType StringToTaskType(const std::string& str) {
  if (str == kFileBrowserHandlerTaskType)
    return TASK_TYPE_FILE_BROWSER_HANDLER;
  if (str == kFileHandlerTaskType)
    return TASK_TYPE_FILE_HANDLER;
  if (str == kArcAppTaskType)
    return TASK_TYPE_ARC_APP;
  if (str == kCrostiniAppTaskType)
    return TASK_TYPE_CROSTINI_APP;
  if (str == kWebAppTaskType)
    return TASK_TYPE_WEB_APP;
  if (str == kPluginVmAppTaskType)
    return TASK_TYPE_PLUGIN_VM_APP;
  return TASK_TYPE_UNKNOWN;
}

// Returns true if path_mime_set contains a Google document.
bool ContainsGoogleDocument(const std::vector<extensions::EntryInfo>& entries) {
  for (const auto& it : entries) {
    if (drive::util::HasHostedDocumentExtension(it.path))
      return true;
  }
  return false;
}

// Removes all tasks except tasks handled by file manager.
void KeepOnlyFileManagerInternalTasks(std::vector<FullTaskDescriptor>* tasks) {
  std::vector<FullTaskDescriptor> filtered;
  for (size_t i = 0; i < tasks->size(); ++i) {
    if ((*tasks)[i].task_descriptor().app_id == kFileManagerAppId)
      filtered.push_back((*tasks)[i]);
  }
  tasks->swap(filtered);
}

// Removes task |actions| handled by file manager.
void RemoveFileManagerInternalActions(const std::set<std::string>& actions,
                                      std::vector<FullTaskDescriptor>* tasks) {
  std::vector<FullTaskDescriptor> filtered;
  for (size_t i = 0; i < tasks->size(); ++i) {
    const auto& action = (*tasks)[i].task_descriptor().action_id;
    if ((*tasks)[i].task_descriptor().app_id != kFileManagerAppId) {
      filtered.push_back((*tasks)[i]);
    } else if (actions.find(action) == actions.end()) {
      filtered.push_back((*tasks)[i]);
    }
  }

  tasks->swap(filtered);
}

// Adjusts |tasks| to reflect the product decision that chrome://media-app
// should behave more like a user-installed app than a fallback handler.
// Specifically, only apps set as the default in user prefs should be preferred
// over chrome://media-app.
// Until the Gallery app is removed, this function will also hide the Gallery
// task in cases where choosing it would instead launch chrome://media-app due
// to interception done in executeTask to support the "camera roll" function,
// when the MediaApp feature is enabled. If the feature is not enabled, there
// will be no MediaApp task and this function does nothing.
void AdjustTasksForMediaApp(const std::vector<extensions::EntryInfo>& entries,
                            std::vector<FullTaskDescriptor>* tasks) {
  const auto task_for_app = [&](const std::string& app_id) {
    return std::find_if(tasks->begin(), tasks->end(), [&](const auto& task) {
      return task.task_descriptor().app_id == app_id;
    });
  };

  const auto media_app_task =
      task_for_app(chromeos::default_web_apps::kMediaAppId);
  if (media_app_task == tasks->end())
    return;

  // TODO(crbug/1030935): Once Media app supports RAW files, delete the
  // IsRawImage early exit. This is necessary while Gallery is still the
  // better option for RAW files. The any_non_image check can be removed once
  // video player functionality of the Media App is fully polished.
  bool any_non_image = false;
  for (const auto& entry : entries) {
    if (IsRawImage(entry.path)) {
      tasks->erase(media_app_task);
      return;  // Let Gallery handle it.
    }

    any_non_image =
        any_non_image || !net::MatchesMimeType("image/*", entry.mime_type);
  }

  const auto gallery_task = task_for_app(kGalleryAppId);
  if (any_non_image) {
    // Remove the gallery app (if it was found), but don't re-order prefs.
    // Picking it would launch Media App due to executeTask interception, but we
    // should still prefer the Video Player app over Media App.
    if (gallery_task != tasks->end())
      tasks->erase(gallery_task);  // Note: invalidates iterators.
    return;
  }

  // Due to https://crbug.com/1071289, configuring extension matches in SWA
  // manifests has no effect on is_file_extension_match(), which means a non-
  // "fallback" web app (i.e. a built-in app) can never be an automatic default.
  // Fallback handlers are never preferred over extension-matched handlers, so
  // we must instead pretend that the media app has an extension match.
  // Note this must be done after the any_non_image check to ensure the video
  // player app gets preference as "default".

  // First DCHECK to see if the hack can be removed.
  DCHECK(!media_app_task->is_file_extension_match());
  media_app_task->set_is_file_extension_match(true);

  // The logic in ChooseAndSetDefaultTask() also requires the following to hold.
  // This should only fail if the media app is configured for "*" (e.g. like
  // Zip Archiver). "image/*" does not count as "generic".
  DCHECK(!media_app_task->is_generic_file_handler());

  // Otherwise, build a new list with Media App at the front, and no Gallery.
  std::vector<FullTaskDescriptor> new_tasks;
  new_tasks.push_back(*media_app_task);
  for (auto it = tasks->begin(); it != tasks->end(); ++it) {
    if (it != media_app_task && it != gallery_task)
      new_tasks.push_back(std::move(*it));
  }
  std::swap(*tasks, new_tasks);
}

// Returns true if the given task is a handler by built-in apps like the Files
// app itself or QuickOffice etc. They are used as the initial default app.
bool IsFallbackFileHandler(const FullTaskDescriptor& task) {
  if ((task.task_descriptor().task_type !=
           file_tasks::TASK_TYPE_FILE_BROWSER_HANDLER &&
       task.task_descriptor().task_type !=
           file_tasks::TASK_TYPE_FILE_HANDLER) ||
      task.is_generic_file_handler()) {
    return false;
  }

  // Note that chromeos::default_web_apps::kMediaAppId does not appear in the
  // list of built-in apps below. Doing so would mean the presence of any other
  // handler of image files (e.g. Keep, Photos) would take precedence. But we
  // want that only to occur if the user has explicitly set the preference for
  // an app other than kMediaAppId to be the default (b/153387960).
  constexpr const char* kBuiltInApps[] = {
      kFileManagerAppId,
      kVideoPlayerAppId,
      kGalleryAppId,
      kTextEditorAppId,
      kAudioPlayerAppId,
      extension_misc::kQuickOfficeComponentExtensionId,
      extension_misc::kQuickOfficeInternalExtensionId,
      extension_misc::kQuickOfficeExtensionId};

  return base::Contains(kBuiltInApps, task.task_descriptor().app_id);
}

// Gets the profile in which a file task owned by |extension| should be
// launched - for example, it makes sure that a file task is not handled in OTR
// profile for platform apps (outside a guest session).
Profile* GetProfileForExtensionTask(Profile* profile,
                                    const extensions::Extension& extension) {
  // In guest profile, all available task handlers are in OTR profile.
  if (profile->IsGuestSession()) {
    DCHECK(profile->IsOffTheRecord());
    return profile;
  }

  // Outside guest sessions, if the task is handled by a platform app, launch
  // the handler in the original profile.
  if (extension.is_platform_app())
    return profile->GetOriginalProfile();
  return profile;
}

void ExecuteByArcAfterMimeTypesCollected(
    Profile* profile,
    const TaskDescriptor& task,
    const std::vector<FileSystemURL>& file_urls,
    FileTaskFinishedCallback done,
    extensions::app_file_handler_util::MimeTypeCollector* mime_collector,
    std::unique_ptr<std::vector<std::string>> mime_types) {
  ExecuteArcTask(profile, task, file_urls, *mime_types, std::move(done));
}

void PostProcessFoundTasks(
    Profile* profile,
    const std::vector<extensions::EntryInfo>& entries,
    FindTasksCallback callback,
    std::unique_ptr<std::vector<FullTaskDescriptor>> result_list) {
  // Google documents can only be handled by internal handlers.
  if (ContainsGoogleDocument(entries))
    KeepOnlyFileManagerInternalTasks(result_list.get());

  // Remove file manager internal view-pdf and view-swf actions if needed.
  std::set<std::string> disabled_actions;
  if (!util::ShouldBeOpenedWithPlugin(profile, FILE_PATH_LITERAL(".pdf"), ""))
    disabled_actions.emplace("view-pdf");
  if (!util::ShouldBeOpenedWithPlugin(profile, FILE_PATH_LITERAL(".swf"), ""))
    disabled_actions.emplace("view-swf");
  if (!disabled_actions.empty())
    RemoveFileManagerInternalActions(disabled_actions, result_list.get());

  AdjustTasksForMediaApp(entries, result_list.get());

  ChooseAndSetDefaultTask(*profile->GetPrefs(), entries, result_list.get());
  std::move(callback).Run(std::move(result_list));
}

// Returns true if |extension_id| and |action_id| indicate that the file
// currently being handled should be opened with the browser. This function
// is used to handle certain action IDs of the file manager.
bool ShouldBeOpenedWithBrowser(const std::string& extension_id,
                               const std::string& action_id) {
  return extension_id == kFileManagerAppId &&
         (action_id == "view-pdf" || action_id == "view-swf" ||
          action_id == "view-in-browser" ||
          action_id == "open-hosted-generic" ||
          action_id == "open-hosted-gdoc" ||
          action_id == "open-hosted-gsheet" ||
          action_id == "open-hosted-gslides");
}

// Opens the files specified by |file_urls| with the browser for |profile|.
// Returns true on success. It's a failure if no files are opened.
bool OpenFilesWithBrowser(Profile* profile,
                          const std::vector<FileSystemURL>& file_urls,
                          const std::string& action_id) {
  int num_opened = 0;
  for (size_t i = 0; i < file_urls.size(); ++i) {
    const FileSystemURL& file_url = file_urls[i];
    if (chromeos::FileSystemBackend::CanHandleURL(file_url)) {
      num_opened +=
          util::OpenFileWithBrowser(profile, file_url, action_id) ? 1 : 0;
    }
  }
  return num_opened > 0;
}

}  // namespace

FullTaskDescriptor::FullTaskDescriptor(const TaskDescriptor& task_descriptor,
                                       const std::string& task_title,
                                       const Verb task_verb,
                                       const GURL& icon_url,
                                       bool is_default,
                                       bool is_generic_file_handler,
                                       bool is_file_extension_match)
    : task_descriptor_(task_descriptor),
      task_title_(task_title),
      task_verb_(task_verb),
      icon_url_(icon_url),
      is_default_(is_default),
      is_generic_file_handler_(is_generic_file_handler),
      is_file_extension_match_(is_file_extension_match) {}

FullTaskDescriptor::~FullTaskDescriptor() = default;

FullTaskDescriptor::FullTaskDescriptor(const FullTaskDescriptor& other) =
    default;

void UpdateDefaultTask(PrefService* pref_service,
                       const std::string& task_id,
                       const std::set<std::string>& suffixes,
                       const std::set<std::string>& mime_types) {
  if (!pref_service)
    return;

  if (!mime_types.empty()) {
    DictionaryPrefUpdate mime_type_pref(pref_service,
                                        prefs::kDefaultTasksByMimeType);
    for (std::set<std::string>::const_iterator iter = mime_types.begin();
        iter != mime_types.end(); ++iter) {
      mime_type_pref->SetWithoutPathExpansion(
          *iter, std::make_unique<base::Value>(task_id));
    }
  }

  if (!suffixes.empty()) {
    DictionaryPrefUpdate mime_type_pref(pref_service,
                                        prefs::kDefaultTasksBySuffix);
    for (std::set<std::string>::const_iterator iter = suffixes.begin();
        iter != suffixes.end(); ++iter) {
      // Suffixes are case insensitive.
      std::string lower_suffix = base::ToLowerASCII(*iter);
      mime_type_pref->SetWithoutPathExpansion(
          lower_suffix, std::make_unique<base::Value>(task_id));
    }
  }
}

std::string GetDefaultTaskIdFromPrefs(const PrefService& pref_service,
                                      const std::string& mime_type,
                                      const std::string& suffix) {
  VLOG(1) << "Looking for default for MIME type: " << mime_type
      << " and suffix: " << suffix;
  std::string task_id;
  if (!mime_type.empty()) {
    const base::DictionaryValue* mime_task_prefs =
        pref_service.GetDictionary(prefs::kDefaultTasksByMimeType);
    DCHECK(mime_task_prefs);
    LOG_IF(ERROR, !mime_task_prefs) << "Unable to open MIME type prefs";
    if (mime_task_prefs &&
        mime_task_prefs->GetStringWithoutPathExpansion(mime_type, &task_id)) {
      VLOG(1) << "Found MIME default handler: " << task_id;
      return task_id;
    }
  }

  const base::DictionaryValue* suffix_task_prefs =
      pref_service.GetDictionary(prefs::kDefaultTasksBySuffix);
  DCHECK(suffix_task_prefs);
  LOG_IF(ERROR, !suffix_task_prefs) << "Unable to open suffix prefs";
  std::string lower_suffix = base::ToLowerASCII(suffix);
  if (suffix_task_prefs)
    suffix_task_prefs->GetStringWithoutPathExpansion(lower_suffix, &task_id);
  VLOG_IF(1, !task_id.empty()) << "Found suffix default handler: " << task_id;
  return task_id;
}

std::string MakeTaskID(const std::string& app_id,
                       TaskType task_type,
                       const std::string& action_id) {
  return base::StringPrintf("%s|%s|%s",
                            app_id.c_str(),
                            TaskTypeToString(task_type).c_str(),
                            action_id.c_str());
}

std::string TaskDescriptorToId(const TaskDescriptor& task_descriptor) {
  return MakeTaskID(task_descriptor.app_id,
                    task_descriptor.task_type,
                    task_descriptor.action_id);
}

bool ParseTaskID(const std::string& task_id, TaskDescriptor* task) {
  DCHECK(task);

  std::vector<std::string> result = base::SplitString(
      task_id, "|", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Parse a legacy task ID that only contain two parts. The legacy task IDs
  // can be stored in preferences.
  if (result.size() == 2) {
    task->task_type = TASK_TYPE_FILE_BROWSER_HANDLER;
    task->app_id = result[0];
    task->action_id = result[1];

    return true;
  }

  if (result.size() != 3)
    return false;

  TaskType task_type = StringToTaskType(result[1]);
  if (task_type == TASK_TYPE_UNKNOWN)
    return false;

  task->app_id = result[0];
  task->task_type = task_type;
  task->action_id = result[2];

  return true;
}

bool ExecuteFileTask(Profile* profile,
                     const GURL& source_url,
                     const TaskDescriptor& task,
                     const std::vector<FileSystemURL>& file_urls,
                     FileTaskFinishedCallback done) {
  UMA_HISTOGRAM_ENUMERATION("FileBrowser.ViewingTaskType", task.task_type,
                            NUM_TASK_TYPE);
  if (drive::util::GetDriveConnectionStatus(profile) ==
      drive::util::DRIVE_DISCONNECTED_NONETWORK) {
    UMA_HISTOGRAM_ENUMERATION("FileBrowser.ViewingTaskType.Offline",
                              task.task_type, NUM_TASK_TYPE);
  } else {
    UMA_HISTOGRAM_ENUMERATION("FileBrowser.ViewingTaskType.Online",
                              task.task_type, NUM_TASK_TYPE);
  }

  // TODO(crbug.com/1005640): Move recording this metric to the App Service when
  // file handling is supported there.
  apps::RecordAppLaunch(task.app_id,
                        apps::mojom::LaunchSource::kFromFileManager);

  if (auto* notifier = FileTasksNotifier::GetForProfile(profile)) {
    notifier->NotifyFileTasks(file_urls);
  }

  // ARC apps needs mime types for launching. Retrieve them first.
  if (task.task_type == TASK_TYPE_ARC_APP) {
    extensions::app_file_handler_util::MimeTypeCollector* mime_collector =
        new extensions::app_file_handler_util::MimeTypeCollector(profile);
    mime_collector->CollectForURLs(
        file_urls, base::BindOnce(&ExecuteByArcAfterMimeTypesCollected, profile,
                                  task, file_urls, std::move(done),
                                  base::Owned(mime_collector)));
    return true;
  }

  if (task.task_type == TASK_TYPE_CROSTINI_APP ||
      task.task_type == TASK_TYPE_PLUGIN_VM_APP) {
    DCHECK_EQ(kGuestOsAppActionID, task.action_id);
    ExecuteGuestOsTask(profile, task, file_urls, std::move(done));
    return true;
  }

  if (task.task_type == TASK_TYPE_WEB_APP) {
    ExecuteWebTask(profile, task, file_urls, std::move(done));
    return true;
  }

  // Some action IDs of the file manager's file browser handlers require the
  // files to be directly opened with the browser.
  if (ShouldBeOpenedWithBrowser(task.app_id, task.action_id)) {
    const bool result =
        OpenFilesWithBrowser(profile, file_urls, task.action_id);
    if (result && done) {
      std::move(done).Run(
          extensions::api::file_manager_private::TASK_RESULT_OPENED, "");
    }
    return result;
  }

  // Get the extension.
  const Extension* extension = extensions::ExtensionRegistry::Get(
      profile)->enabled_extensions().GetByID(task.app_id);
  if (!extension)
    return false;

  Profile* extension_task_profile =
      GetProfileForExtensionTask(profile, *extension);

  // Execute the task.
  if (task.task_type == TASK_TYPE_FILE_BROWSER_HANDLER) {
    return file_browser_handlers::ExecuteFileBrowserHandler(
        extension_task_profile, extension, task.action_id, file_urls,
        std::move(done));
  } else if (task.task_type == TASK_TYPE_FILE_HANDLER) {
    std::vector<base::FilePath> paths;
    for (size_t i = 0; i != file_urls.size(); ++i)
      paths.push_back(file_urls[i].path());

    DCHECK(!extension->from_bookmark());
    apps::LaunchPlatformAppWithFileHandler(extension_task_profile, extension,
                                           task.action_id, paths);
    if (!done.is_null())
      std::move(done).Run(
          extensions::api::file_manager_private::TASK_RESULT_MESSAGE_SENT, "");
    return true;
  }
  NOTREACHED();
  return false;
}

bool IsFileHandlerEnabled(Profile* profile,
                          const apps::FileHandlerInfo& file_handler_info) {
  // Crostini deb files and backup files can be disabled by policy.
  if (file_handler_info.id == kInstallLinuxPackageHandlerId) {
    return crostini::CrostiniFeatures::Get()->IsRootAccessAllowed(profile);
  }
  if (file_handler_info.id == kImportCrostiniImageHandlerId) {
    return crostini::CrostiniFeatures::Get()->IsExportImportUIAllowed(profile);
  }
  return true;
}

bool IsGoodMatchFileHandler(const apps::FileHandlerInfo& file_handler_info,
                            const std::vector<extensions::EntryInfo>& entries) {
  if (file_handler_info.extensions.count("*") > 0 ||
      file_handler_info.types.count("*") > 0 ||
      file_handler_info.types.count("*/*") > 0)
    return false;

  // If text/* file handler matches with unsupported text mime type, we don't
  // regard it as good match.
  if (file_handler_info.types.count("text/*")) {
    for (const auto& entry : entries) {
      if (blink::IsUnsupportedTextMimeType(entry.mime_type))
        return false;
    }
  }

  // We consider it a good match if no directories are selected.
  for (const auto& entry : entries) {
    if (entry.is_directory)
      return false;
  }
  return true;
}

bool IsGoodMatchAppsFileHandler(
    const apps::FileHandler& file_handler,
    const std::vector<extensions::EntryInfo>& entries) {
  // TODO(crbug.com/938103): Duplicates functionality from
  // FileHandlerManager::GetMimeTypesFromFileHandlers and
  // ::GetFileExtensionsFromFileHandlers.
  std::set<std::string> mime_types;
  std::set<std::string> file_extensions;
  for (const auto& accept_entry : file_handler.accept) {
    mime_types.insert(accept_entry.mime_type);
    file_extensions.insert(accept_entry.file_extensions.begin(),
                           accept_entry.file_extensions.end());
  }

  if (mime_types.count("*") || mime_types.count("*/*") ||
      file_extensions.count("*"))
    return false;

  // If a "text/*" file handler matches with an unsupported text MIME type, we
  // don't regard it as a good match.
  if (mime_types.count("text/*")) {
    for (const auto& entry : entries) {
      if (blink::IsUnsupportedTextMimeType(entry.mime_type))
        return false;
    }
  }

  // We consider it a good match if no directories are selected.
  for (const auto& entry : entries) {
    if (entry.is_directory)
      return false;
  }
  return true;
}

void FindFileHandlerTasks(Profile* profile,
                          const std::vector<extensions::EntryInfo>& entries,
                          std::vector<FullTaskDescriptor>* result_list) {
  DCHECK(!entries.empty());
  DCHECK(result_list);

  const extensions::ExtensionSet& enabled_extensions =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions();

  for (extensions::ExtensionSet::const_iterator iter =
           enabled_extensions.begin();
       iter != enabled_extensions.end();
       ++iter) {
    const Extension* extension = iter->get();

    // Check that the extension can be launched with files. This includes all
    // platform apps and whitelisted extensions.
    if (!CanLaunchViaEvent(extension)) {
      continue;
    }

    if (profile->IsOffTheRecord() &&
        !extensions::util::IsIncognitoEnabled(extension->id(), profile))
      continue;

    typedef std::vector<extensions::FileHandlerMatch> FileHandlerMatchList;
    FileHandlerMatchList file_handlers =
        FindFileHandlerMatchesForEntries(*extension, entries);
    if (file_handlers.empty())
      continue;

    // A map which has as key a handler verb, and as value a pair of the
    // handler with which to open the given entries and a boolean marking
    // if the handler is a good match.
    std::map<std::string, std::pair<const extensions::FileHandlerMatch*, bool>>
        handlers_for_entries;
    // Show the first good matching handler of each verb supporting the given
    // entries that corresponds to the app. If there doesn't exist such handler,
    // show the first matching handler of the verb.
    for (const auto& handler_match : file_handlers) {
      const apps::FileHandlerInfo* handler = handler_match.handler;
      if (!IsFileHandlerEnabled(profile, *handler)) {
        continue;
      }
      bool good_match = IsGoodMatchFileHandler(*handler, entries);
      auto it = handlers_for_entries.find(handler->verb);
      if (it == handlers_for_entries.end() ||
          (!it->second.second /* existing handler not a good match */ &&
           good_match)) {
        handlers_for_entries[handler->verb] =
            std::make_pair(&handler_match, good_match);
      }
    }

    for (const auto& entry : handlers_for_entries) {
      const extensions::FileHandlerMatch* match = entry.second.first;
      const apps::FileHandlerInfo* handler = match->handler;
      std::string task_id = file_tasks::MakeTaskID(
          extension->id(), file_tasks::TASK_TYPE_FILE_HANDLER, handler->id);

      GURL best_icon = extensions::ExtensionIconSource::GetIconURL(
          extension, extension_misc::EXTENSION_ICON_SMALL,
          ExtensionIconSet::MATCH_BIGGER,
          false);  // grayscale

      // If file handler doesn't match as good match, regards it as generic file
      // handler.
      const bool is_generic_file_handler =
          !IsGoodMatchFileHandler(*handler, entries);
      Verb verb;
      if (handler->verb == apps::file_handler_verbs::kAddTo) {
        verb = Verb::VERB_ADD_TO;
      } else if (handler->verb == apps::file_handler_verbs::kPackWith) {
        verb = Verb::VERB_PACK_WITH;
      } else if (handler->verb == apps::file_handler_verbs::kShareWith) {
        verb = Verb::VERB_SHARE_WITH;
      } else {
        // Only kOpenWith is a valid remaining verb. Invalid verbs should fall
        // back to it.
        DCHECK(handler->verb == apps::file_handler_verbs::kOpenWith);
        verb = Verb::VERB_OPEN_WITH;
      }
      // If the handler was matched purely on the file name extension then
      // the manifest declared its 'file_handler' to match. Used for fallback
      // selection of the handler when we don't have a default handler set
      const bool is_file_extension_match = match->matched_file_extension;

      result_list->push_back(FullTaskDescriptor(
          TaskDescriptor(extension->id(), file_tasks::TASK_TYPE_FILE_HANDLER,
                         handler->id),
          extension->name(), verb, best_icon, false /* is_default */,
          is_generic_file_handler, is_file_extension_match));
    }
  }
}

void FindFileBrowserHandlerTasks(
    Profile* profile,
    const std::vector<GURL>& file_urls,
    std::vector<FullTaskDescriptor>* result_list) {
  DCHECK(!file_urls.empty());
  DCHECK(result_list);

  file_browser_handlers::FileBrowserHandlerList common_tasks =
      file_browser_handlers::FindFileBrowserHandlers(profile, file_urls);
  if (common_tasks.empty())
    return;

  const extensions::ExtensionSet& enabled_extensions =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  for (file_browser_handlers::FileBrowserHandlerList::const_iterator iter =
           common_tasks.begin();
       iter != common_tasks.end();
       ++iter) {
    const FileBrowserHandler* handler = *iter;
    const std::string extension_id = handler->extension_id();
    const Extension* extension = enabled_extensions.GetByID(extension_id);
    DCHECK(extension);

    // TODO(zelidrag): Figure out how to expose icon URL that task defined in
    // manifest instead of the default extension icon.
    const GURL icon_url = extensions::ExtensionIconSource::GetIconURL(
        extension, extension_misc::EXTENSION_ICON_SMALL,
        ExtensionIconSet::MATCH_BIGGER,
        false);  // grayscale

    result_list->push_back(FullTaskDescriptor(
        TaskDescriptor(extension_id, file_tasks::TASK_TYPE_FILE_BROWSER_HANDLER,
                       handler->id()),
        handler->title(), Verb::VERB_NONE /* no verb for FileBrowserHandler */,
        icon_url, false /* is_default */, false /* is_generic_file_handler */,
        false /* is_file_extension_match */));
  }
}

void FindExtensionAndAppTasks(
    Profile* profile,
    const std::vector<extensions::EntryInfo>& entries,
    const std::vector<GURL>& file_urls,
    FindTasksCallback callback,
    std::unique_ptr<std::vector<FullTaskDescriptor>> result_list) {
  std::vector<FullTaskDescriptor>* result_list_ptr = result_list.get();

  // 2. Find and append Web tasks.
  FindWebTasks(profile, entries, result_list_ptr);

  // 3. Continues from FindAllTypesOfTasks. Find and append file handler tasks.
  FindFileHandlerTasks(profile, entries, result_list_ptr);

  // 4. Find and append file browser handler tasks. We know there aren't
  // duplicates because "file_browser_handlers" and "file_handlers" shouldn't
  // be used in the same manifest.json.
  FindFileBrowserHandlerTasks(profile, file_urls, result_list_ptr);

  // 5. Find and append Guest OS tasks.
  FindGuestOsTasks(profile, entries, result_list_ptr,
                   // Done. Apply post-filtering and callback.
                   base::BindOnce(PostProcessFoundTasks, profile, entries,
                                  std::move(callback), std::move(result_list)));
}

void FindAllTypesOfTasks(Profile* profile,
                         const std::vector<extensions::EntryInfo>& entries,
                         const std::vector<GURL>& file_urls,
                         FindTasksCallback callback) {
  DCHECK(profile);
  std::unique_ptr<std::vector<FullTaskDescriptor>> result_list(
      new std::vector<FullTaskDescriptor>);

  // 1. Find and append ARC handler tasks.
  FindArcTasks(profile, entries, file_urls, std::move(result_list),
               base::BindOnce(&FindExtensionAndAppTasks, profile, entries,
                              file_urls, std::move(callback)));
}

void ChooseAndSetDefaultTask(const PrefService& pref_service,
                             const std::vector<extensions::EntryInfo>& entries,
                             std::vector<FullTaskDescriptor>* tasks) {
  // Collect the task IDs of default tasks from the preferences into a set.
  std::set<std::string> default_task_ids;
  for (std::vector<extensions::EntryInfo>::const_iterator it = entries.begin();
       it != entries.end(); ++it) {
    const base::FilePath& file_path = it->path;
    const std::string& mime_type = it->mime_type;
    std::string task_id = file_tasks::GetDefaultTaskIdFromPrefs(
        pref_service, mime_type, file_path.Extension());
    default_task_ids.insert(task_id);
  }

  // Go through all the tasks from the beginning and see if there is any
  // default task. If found, pick and set it as default and return.
  for (size_t i = 0; i < tasks->size(); ++i) {
    FullTaskDescriptor* task = &tasks->at(i);
    DCHECK(!task->is_default());
    const std::string task_id = TaskDescriptorToId(task->task_descriptor());
    if (base::Contains(default_task_ids, task_id)) {
      task->set_is_default(true);
      return;
    }
  }

  // No default task, check for an explicit file extension match (without
  // MIME match) in the extension manifest and pick that over the fallback
  // handlers below (see crbug.com/803930)
  for (size_t i = 0; i < tasks->size(); ++i) {
    FullTaskDescriptor& task = (*tasks)[i];
    if (task.is_file_extension_match() && !task.is_generic_file_handler() &&
        !IsFallbackFileHandler(task)) {
      task.set_is_default(true);
      return;
    }
  }

  // No default tasks found. If there is any fallback file browser handler,
  // make it as default task, so it's selected by default.
  for (size_t i = 0; i < tasks->size(); ++i) {
    FullTaskDescriptor& task = (*tasks)[i];
    DCHECK(!task.is_default());
    if (IsFallbackFileHandler(task)) {
      task.set_is_default(true);
      return;
    }
  }
}

bool IsRawImage(const base::FilePath& path) {
  constexpr const char* kRawExtensions[] = {".arw", ".cr2", ".dng", ".nef",
                                            ".nrw", ".orf", ".raf", ".rw2"};
  for (const char* extension : kRawExtensions) {
    if (path.MatchesExtension(extension))
      return true;
  }
  return false;
}

}  // namespace file_tasks
}  // namespace file_manager
