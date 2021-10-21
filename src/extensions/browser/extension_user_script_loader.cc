// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_user_script_loader.h"

#include <stddef.h>

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase_set.h"
#include "base/containers/cxx20_erase_vector.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/one_shot_event.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/version.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api/scripting/constants.h"
#include "extensions/browser/component_extension_resource_manager.h"
#include "extensions/browser/content_verifier.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/state_store.h"
#include "extensions/common/api/content_scripts.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"
#include "extensions/common/manifest_handlers/default_locale_handler.h"
#include "extensions/common/message_bundle.h"
#include "extensions/common/utils/content_script_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserContext;

namespace extensions {

namespace {

using SubstitutionMap = std::map<std::string, std::string>;

// Each map entry associates a UserScript::File object with the ID of the
// resource holding the content of the script.
using ScriptResourceIds = std::map<UserScript::File*, absl::optional<int>>;

struct VerifyContentInfo {
  VerifyContentInfo(const scoped_refptr<ContentVerifier>& verifier,
                    const ExtensionId& extension_id,
                    const base::FilePath& extension_root,
                    const base::FilePath relative_path,
                    const std::string& content)
      : verifier(verifier),
        extension_id(extension_id),
        extension_root(extension_root),
        relative_path(relative_path),
        content(content) {}

  scoped_refptr<ContentVerifier> verifier;
  ExtensionId extension_id;
  base::FilePath extension_root;
  base::FilePath relative_path;
  std::string content;
};

// Verifies file contents as they are read.
void VerifyContent(const VerifyContentInfo& info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(info.verifier);
  scoped_refptr<ContentVerifyJob> job(info.verifier->CreateAndStartJobFor(
      info.extension_id, info.extension_root, info.relative_path));
  if (job.get()) {
    job->Read(info.content.data(), info.content.size(), MOJO_RESULT_OK);
    job->Done();
  }
}

void ForwardVerifyContentToIO(const VerifyContentInfo& info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&VerifyContent, info));
}

// Loads user scripts from the extension who owns these scripts.
bool LoadScriptContent(const mojom::HostID& host_id,
                       UserScript::File* script_file,
                       const absl::optional<int>& script_resource_id,
                       const SubstitutionMap* localization_messages,
                       const scoped_refptr<ContentVerifier>& verifier) {
  DCHECK(script_file);
  std::string content;
  const base::FilePath& path = ExtensionResource::GetFilePath(
      script_file->extension_root(), script_file->relative_path(),
      ExtensionResource::SYMLINKS_MUST_RESOLVE_WITHIN_ROOT);
  if (path.empty()) {
    if (script_resource_id) {
      const ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      content = rb.LoadDataResourceString(*script_resource_id);
    } else {
      LOG(WARNING) << "Failed to get file path to "
                   << script_file->relative_path().value() << " from "
                   << script_file->extension_root().value();
      return false;
    }
  } else {
    if (!base::ReadFileToString(path, &content)) {
      LOG(WARNING) << "Failed to load user script file: " << path.value();
      return false;
    }
    if (verifier.get()) {
      // Call VerifyContent() after yielding on UI thread so it is ensured that
      // ContentVerifierIOData is populated at the time we call VerifyContent().
      // Priority set explicitly to avoid unwanted task priority inheritance.
      content::GetUIThreadTaskRunner({base::TaskPriority::USER_BLOCKING})
          ->PostTask(
              FROM_HERE,
              base::BindOnce(
                  &ForwardVerifyContentToIO,
                  VerifyContentInfo(verifier, host_id.id,
                                    script_file->extension_root(),
                                    script_file->relative_path(), content)));
    }
  }

  // Localize the content.
  if (localization_messages) {
    std::string error;
    MessageBundle::ReplaceMessagesWithExternalDictionary(*localization_messages,
                                                         &content, &error);
    if (!error.empty())
      LOG(WARNING) << "Failed to replace messages in script: " << error;
  }

  // Remove BOM from the content.
  if (base::StartsWith(content, base::kUtf8ByteOrderMark,
                       base::CompareCase::SENSITIVE)) {
    script_file->set_content(content.substr(strlen(base::kUtf8ByteOrderMark)));
  } else {
    script_file->set_content(content);
  }

  return true;
}

void FillScriptFileResourceIds(const UserScript::FileList& script_files,
                               ScriptResourceIds& script_resource_ids) {
  const ComponentExtensionResourceManager* extension_resource_manager =
      ExtensionsBrowserClient::Get()->GetComponentExtensionResourceManager();
  if (!extension_resource_manager)
    return;

  for (const std::unique_ptr<UserScript::File>& script_file : script_files) {
    if (!script_file->GetContent().empty())
      continue;
    int resource_id = 0;
    if (extension_resource_manager->IsComponentExtensionResource(
            script_file->extension_root(), script_file->relative_path(),
            &resource_id)) {
      script_resource_ids[script_file.get()] = resource_id;
    }
  }
}

void LoadUserScripts(
    UserScriptList* user_scripts,
    ScriptResourceIds script_resource_ids,
    const ExtensionUserScriptLoader::PathAndLocaleInfo& host_info,
    const std::set<std::string>& added_script_ids,
    const scoped_refptr<ContentVerifier>& verifier) {
  for (const std::unique_ptr<UserScript>& script : *user_scripts) {
    if (added_script_ids.count(script->id()) == 0)
      continue;
    for (const std::unique_ptr<UserScript::File>& script_file :
         script->js_scripts()) {
      if (script_file->GetContent().empty())
        LoadScriptContent(script->host_id(), script_file.get(),
                          script_resource_ids[script_file.get()], nullptr,
                          verifier);
    }
    if (script->css_scripts().size() > 0) {
      std::unique_ptr<SubstitutionMap> localization_messages(
          file_util::LoadMessageBundleSubstitutionMap(
              host_info.file_path, script->host_id().id,
              host_info.default_locale, host_info.gzip_permission));

      for (const std::unique_ptr<UserScript::File>& script_file :
           script->css_scripts()) {
        if (script_file->GetContent().empty()) {
          LoadScriptContent(script->host_id(), script_file.get(),
                            script_resource_ids[script_file.get()],
                            localization_messages.get(), verifier);
        }
      }
    }
  }
}

void LoadScriptsOnFileTaskRunner(
    std::unique_ptr<UserScriptList> user_scripts,
    ScriptResourceIds script_resource_ids,
    const ExtensionUserScriptLoader::PathAndLocaleInfo& host_info,
    const std::set<std::string>& added_script_ids,
    const scoped_refptr<ContentVerifier>& verifier,
    UserScriptLoader::LoadScriptsCallback callback) {
  DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());
  DCHECK(user_scripts.get());
  LoadUserScripts(user_scripts.get(), std::move(script_resource_ids), host_info,
                  added_script_ids, verifier);
  base::ReadOnlySharedMemoryRegion memory =
      UserScriptLoader::Serialize(*user_scripts);
  // Explicit priority to prevent unwanted task priority inheritance.
  content::GetUIThreadTaskRunner({base::TaskPriority::USER_BLOCKING})
      ->PostTask(FROM_HERE,
                 base::BindOnce(std::move(callback), std::move(user_scripts),
                                std::move(memory)));
}

UserScriptList ConvertValueToScripts(const Extension& extension,
                                     const base::Value& list) {
  const int valid_schemes = UserScript::ValidUserScriptSchemes(
      scripting::kScriptsCanExecuteEverywhere);

  UserScriptList scripts;
  for (const base::Value& value : list.GetList()) {
    std::u16string error;
    std::unique_ptr<api::content_scripts::ContentScript> content_script =
        api::content_scripts::ContentScript::FromValue(value, &error);

    if (!content_script)
      continue;

    std::unique_ptr<UserScript> script = std::make_unique<UserScript>();
    const auto* dict = static_cast<const base::DictionaryValue*>(&value);
    const base::Value* id_value = dict->FindKey(scripting::kId);
    auto* id = id_value->GetIfString();
    if (!id)
      continue;

    script->set_id(*id);
    script->set_host_id(
        mojom::HostID(mojom::HostID::HostType::kExtensions, extension.id()));

    if (content_script->all_frames)
      script->set_match_all_frames(*content_script->all_frames);
    script->set_run_location(
        script_parsing::ConvertManifestRunLocation(content_script->run_at));

    if (!script_parsing::ParseMatchPatterns(
            content_script->matches, content_script->exclude_matches.get(),
            /*definition_index=*/0, extension.creation_flags(),
            scripting::kScriptsCanExecuteEverywhere, valid_schemes,
            scripting::kAllUrlsIncludesChromeUrls, script.get(), &error,
            /*wants_file_access=*/nullptr)) {
      continue;
    }

    if (!script_parsing::ParseFileSources(
            &extension, content_script->js.get(), content_script->css.get(),
            /*definition_index=*/0, script.get(), &error)) {
      continue;
    }

    scripts.push_back(std::move(script));
  }

  return scripts;
}

// TODO(crbug.com/1248314): Eventually use
// api::scripting::RegisteredContentScript instead as an intermediate type which
// then gets converted to base::Value.
api::content_scripts::ContentScript CreateContentScriptObject(
    const UserScript& script) {
  api::content_scripts::ContentScript content_script;

  content_script.matches.reserve(script.url_patterns().size());
  for (const URLPattern& pattern : script.url_patterns())
    content_script.matches.push_back(pattern.GetAsString());

  if (!script.exclude_url_patterns().is_empty()) {
    content_script.exclude_matches =
        std::make_unique<std::vector<std::string>>();
    content_script.exclude_matches->reserve(
        script.exclude_url_patterns().size());
    for (const URLPattern& pattern : script.exclude_url_patterns())
      content_script.exclude_matches->push_back(pattern.GetAsString());
  }

  // File paths may be normalized in the returned object and can differ slightly
  // compared to what was originally passed into registerContentScripts.
  if (!script.js_scripts().empty()) {
    content_script.js = std::make_unique<std::vector<std::string>>();
    content_script.js->reserve(script.js_scripts().size());
    for (const auto& js_script : script.js_scripts())
      content_script.js->push_back(js_script->relative_path().AsUTF8Unsafe());
  }

  if (!script.css_scripts().empty()) {
    content_script.css = std::make_unique<std::vector<std::string>>();
    content_script.css->reserve(script.css_scripts().size());
    for (const auto& css_script : script.css_scripts())
      content_script.css->push_back(css_script->relative_path().AsUTF8Unsafe());
  }

  content_script.all_frames = std::make_unique<bool>(script.match_all_frames());
  content_script.match_origin_as_fallback =
      std::make_unique<bool>(script.match_origin_as_fallback() ==
                             MatchOriginAsFallbackBehavior::kAlways);

  content_script.run_at =
      script_parsing::ConvertRunLocationToManifestType(script.run_location());
  return content_script;
}

}  // namespace

ExtensionUserScriptLoader::ExtensionUserScriptLoader(
    BrowserContext* browser_context,
    const Extension& extension,
    StateStore* state_store,
    bool listen_for_extension_system_loaded)
    : ExtensionUserScriptLoader(
          browser_context,
          extension,
          state_store,
          listen_for_extension_system_loaded,
          ExtensionSystem::Get(browser_context)->content_verifier()) {}

ExtensionUserScriptLoader::ExtensionUserScriptLoader(
    BrowserContext* browser_context,
    const Extension& extension,
    StateStore* state_store,
    bool listen_for_extension_system_loaded,
    scoped_refptr<ContentVerifier> content_verifier)
    : UserScriptLoader(
          browser_context,
          mojom::HostID(mojom::HostID::HostType::kExtensions, extension.id())),
      host_info_({extension.path(), LocaleInfo::GetDefaultLocale(&extension),
                  extension_l10n_util::GetGzippedMessagesPermissionForExtension(
                      &extension)}),
      helper_(browser_context, extension.id(), state_store),
      content_verifier_(std::move(content_verifier)) {
  if (listen_for_extension_system_loaded) {
    ExtensionSystem::Get(browser_context)
        ->ready()
        .Post(FROM_HERE,
              base::BindOnce(&ExtensionUserScriptLoader::OnExtensionSystemReady,
                             weak_factory_.GetWeakPtr()));
  } else {
    SetReady(true);
  }
}

ExtensionUserScriptLoader::~ExtensionUserScriptLoader() {
}

void ExtensionUserScriptLoader::AddPendingDynamicScriptIDs(
    std::set<std::string> script_ids) {
  pending_dynamic_script_ids_.insert(
      std::make_move_iterator(script_ids.begin()),
      std::make_move_iterator(script_ids.end()));
}

void ExtensionUserScriptLoader::RemovePendingDynamicScriptIDs(
    const std::set<std::string>& script_ids) {
  for (const auto& id : script_ids)
    pending_dynamic_script_ids_.erase(id);
}

void ExtensionUserScriptLoader::AddScriptsForExtensionLoad(
    std::unique_ptr<UserScriptList> manifest_scripts,
    UserScriptLoader::ScriptsLoadedCallback callback) {
  AddScripts(std::move(manifest_scripts), std::move(callback));

  // Manifest scripts and persistent dynamic scripts are not loaded as part of
  // the same list to ensure `callback` will be not orphaned while persistent
  // scripts are being fetched, which prevents `callback` from never being
  // called if the extension is unloaded early.
  helper_.GetDynamicScripts(base::BindOnce(
      &ExtensionUserScriptLoader::OnInitialDynamicScriptsReadFromStateStore,
      weak_factory_.GetWeakPtr()));
}

void ExtensionUserScriptLoader::AddDynamicScripts(
    std::unique_ptr<UserScriptList> scripts,
    std::set<std::string> persistent_script_ids,
    DynamicScriptsModifiedCallback callback) {
  auto scripts_metadata = std::make_unique<UserScriptList>();
  for (const std::unique_ptr<UserScript>& script : *scripts) {
    // Only proceed with adding scripts that the extension still intends to add.
    // This guards again an edge case where scripts registered by an API call
    // are quickly unregistered.
    if (base::Contains(pending_dynamic_script_ids_, script->id()))
      scripts_metadata->push_back(UserScript::CopyMetadataFrom(*script));
  }

  if (scripts_metadata->empty()) {
    std::move(callback).Run(/*error=*/absl::nullopt);
    return;
  }

  AddScripts(
      std::move(scripts),
      base::BindOnce(&ExtensionUserScriptLoader::OnDynamicScriptsAdded,
                     weak_factory_.GetWeakPtr(), std::move(scripts_metadata),
                     std::move(persistent_script_ids), std::move(callback)));
}

void ExtensionUserScriptLoader::RemoveDynamicScripts(
    const std::set<std::string>& ids_to_remove,
    DynamicScriptsModifiedCallback callback) {
  if (ids_to_remove.empty()) {
    std::move(callback).Run(/*error=*/absl::nullopt);
    return;
  }

  // Remove pending script ids first, so loads from previous operations which
  // complete later will recognize the change.
  RemovePendingDynamicScriptIDs(ids_to_remove);
  RemoveScripts(
      ids_to_remove,
      base::BindOnce(&ExtensionUserScriptLoader::OnDynamicScriptsRemoved,
                     weak_factory_.GetWeakPtr(), ids_to_remove,
                     std::move(callback)));
}

void ExtensionUserScriptLoader::ClearDynamicScripts(
    DynamicScriptsModifiedCallback callback) {
  RemoveDynamicScripts(GetDynamicScriptIDs(), std::move(callback));
}

std::set<std::string> ExtensionUserScriptLoader::GetDynamicScriptIDs() const {
  std::set<std::string> dynamic_script_ids;
  dynamic_script_ids.insert(pending_dynamic_script_ids_.begin(),
                            pending_dynamic_script_ids_.end());

  for (const std::unique_ptr<UserScript>& script : loaded_dynamic_scripts_)
    dynamic_script_ids.insert(script->id());

  return dynamic_script_ids;
}

const UserScriptList& ExtensionUserScriptLoader::GetLoadedDynamicScripts()
    const {
  return loaded_dynamic_scripts_;
}

std::set<std::string> ExtensionUserScriptLoader::GetPersistentDynamicScriptIDs()
    const {
  return persistent_dynamic_script_ids_;
}

std::unique_ptr<UserScriptList> ExtensionUserScriptLoader::LoadScriptsForTest(
    std::unique_ptr<UserScriptList> user_scripts) {
  std::set<std::string> added_script_ids;
  for (const std::unique_ptr<UserScript>& script : *user_scripts)
    added_script_ids.insert(script->id());

  std::unique_ptr<UserScriptList> result;

  // Block until the scripts have been loaded on the file task runner so that
  // we can return the result synchronously.
  base::RunLoop run_loop;
  LoadScripts(std::move(user_scripts), added_script_ids,
              base::BindOnce(
                  [](base::OnceClosure done_callback,
                     std::unique_ptr<UserScriptList>& loaded_user_scripts,
                     std::unique_ptr<UserScriptList> user_scripts,
                     base::ReadOnlySharedMemoryRegion /* shared_memory */) {
                    loaded_user_scripts = std::move(user_scripts);
                    std::move(done_callback).Run();
                  },
                  run_loop.QuitClosure(), std::ref(result)));
  run_loop.Run();

  return result;
}

ExtensionUserScriptLoader::DynamicScriptsStorageHelper::
    DynamicScriptsStorageHelper(BrowserContext* browser_context,
                                const ExtensionId& extension_id,
                                StateStore* state_store)
    : browser_context_(browser_context),
      extension_id_(extension_id),
      state_store_(state_store) {}
ExtensionUserScriptLoader::DynamicScriptsStorageHelper::
    ~DynamicScriptsStorageHelper() = default;

void ExtensionUserScriptLoader::DynamicScriptsStorageHelper::GetDynamicScripts(
    DynamicScriptsReadCallback callback) {
  if (!state_store_) {
    std::move(callback).Run(UserScriptList());
    return;
  }

  state_store_->GetExtensionValue(
      extension_id_, scripting::kRegisteredScriptsStorageKey,
      base::BindOnce(&ExtensionUserScriptLoader::DynamicScriptsStorageHelper::
                         OnDynamicScriptsReadFromStorage,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ExtensionUserScriptLoader::DynamicScriptsStorageHelper::SetDynamicScripts(
    const UserScriptList& scripts,
    const std::set<std::string>& persistent_dynamic_script_ids) {
  if (!state_store_)
    return;

  auto scripts_value = std::make_unique<base::Value>(base::Value::Type::LIST);
  for (const std::unique_ptr<UserScript>& script : scripts) {
    if (!base::Contains(persistent_dynamic_script_ids, script->id()))
      continue;

    base::DictionaryValue value =
        std::move(*CreateContentScriptObject(*script).ToValue());
    value.SetStringPath(scripting::kId, script->id());

    scripts_value->Append(std::move(value));
  }

  state_store_->SetExtensionValue(extension_id_,
                                  scripting::kRegisteredScriptsStorageKey,
                                  std::move(scripts_value));
}

void ExtensionUserScriptLoader::DynamicScriptsStorageHelper::
    OnDynamicScriptsReadFromStorage(DynamicScriptsReadCallback callback,
                                    std::unique_ptr<base::Value> value) {
  const Extension* extension = ExtensionRegistry::Get(browser_context_)
                                   ->enabled_extensions()
                                   .GetByID(extension_id_);
  DCHECK(extension) << "The ExtensionUserScriptLoader should have been cleaned "
                       "up if the extension was disabled";

  UserScriptList scripts;
  if (value && value->type() == base::Value::Type::LIST) {
    UserScriptList dynamic_scripts = ConvertValueToScripts(*extension, *value);
    scripts.insert(scripts.end(),
                   std::make_move_iterator(dynamic_scripts.begin()),
                   std::make_move_iterator(dynamic_scripts.end()));
  }

  std::move(callback).Run(std::move(scripts));
}

void ExtensionUserScriptLoader::LoadScripts(
    std::unique_ptr<UserScriptList> user_scripts,
    const std::set<std::string>& added_script_ids,
    LoadScriptsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ScriptResourceIds script_resource_ids;
  for (const std::unique_ptr<UserScript>& script : *user_scripts) {
    if (!base::Contains(added_script_ids, script->id()))
      continue;
    FillScriptFileResourceIds(script->js_scripts(), script_resource_ids);
    FillScriptFileResourceIds(script->css_scripts(), script_resource_ids);
  }

  GetExtensionFileTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&LoadScriptsOnFileTaskRunner, std::move(user_scripts),
                     std::move(script_resource_ids), host_info_,
                     added_script_ids, content_verifier_, std::move(callback)));
}

void ExtensionUserScriptLoader::OnExtensionSystemReady() {
  SetReady(true);
}

void ExtensionUserScriptLoader::OnInitialDynamicScriptsReadFromStateStore(
    UserScriptList initial_dynamic_scripts) {
  auto dynamic_scripts_metadata = std::make_unique<UserScriptList>();
  auto scripts = std::make_unique<UserScriptList>();
  for (const std::unique_ptr<UserScript>& script : initial_dynamic_scripts) {
    dynamic_scripts_metadata->push_back(UserScript::CopyMetadataFrom(*script));
    pending_dynamic_script_ids_.insert(script->id());
  }

  scripts->insert(scripts->end(),
                  std::make_move_iterator(initial_dynamic_scripts.begin()),
                  std::make_move_iterator(initial_dynamic_scripts.end()));

  AddScripts(
      std::move(scripts),
      base::BindOnce(&ExtensionUserScriptLoader::OnInitialDynamicScriptsLoaded,
                     weak_factory_.GetWeakPtr(),
                     std::move(dynamic_scripts_metadata)));
}

void ExtensionUserScriptLoader::OnInitialDynamicScriptsLoaded(
    std::unique_ptr<UserScriptList> initial_dynamic_scripts,
    UserScriptLoader* loader,
    const absl::optional<std::string>& error) {
  for (const std::unique_ptr<UserScript>& script : *initial_dynamic_scripts)
    pending_dynamic_script_ids_.erase(script->id());

  if (!error.has_value()) {
    for (const std::unique_ptr<UserScript>& script : *initial_dynamic_scripts)
      persistent_dynamic_script_ids_.insert(script->id());
    loaded_dynamic_scripts_.insert(
        loaded_dynamic_scripts_.end(),
        std::make_move_iterator(initial_dynamic_scripts->begin()),
        std::make_move_iterator(initial_dynamic_scripts->end()));
  }
}

void ExtensionUserScriptLoader::OnDynamicScriptsAdded(
    std::unique_ptr<UserScriptList> added_scripts,
    std::set<std::string> new_persistent_script_ids,
    DynamicScriptsModifiedCallback callback,
    UserScriptLoader* loader,
    const absl::optional<std::string>& error) {
  // Now that a script load for all scripts contained in `added_scripts` has
  // occurred, add these scripts to `loaded_dynamic_scripts_` and remove any ids
  // in `pending_dynamic_script_ids_` that correspond to a script in
  // `added_scripts`.
  for (const std::unique_ptr<UserScript>& script : *added_scripts)
    pending_dynamic_script_ids_.erase(script->id());

  if (!error.has_value()) {
    loaded_dynamic_scripts_.insert(
        loaded_dynamic_scripts_.end(),
        std::make_move_iterator(added_scripts->begin()),
        std::make_move_iterator(added_scripts->end()));

    persistent_dynamic_script_ids_.insert(
        std::make_move_iterator(new_persistent_script_ids.begin()),
        std::make_move_iterator(new_persistent_script_ids.end()));

    helper_.SetDynamicScripts(loaded_dynamic_scripts_,
                              persistent_dynamic_script_ids_);
  }

  std::move(callback).Run(error);
}

void ExtensionUserScriptLoader::OnDynamicScriptsRemoved(
    const std::set<std::string>& removed_script_ids,
    DynamicScriptsModifiedCallback callback,
    UserScriptLoader* loader,
    const absl::optional<std::string>& error) {
  // Remove scripts from `loaded_dynamic_scripts_` only when the set of
  // `removed_script_ids` have actually been removed and the corresponding IPC
  // has been sent.
  if (!error.has_value()) {
    base::EraseIf(
        loaded_dynamic_scripts_,
        [&removed_script_ids](const std::unique_ptr<UserScript>& script) {
          return base::Contains(removed_script_ids, script->id());
        });

    base::EraseIf(persistent_dynamic_script_ids_,
                  [&removed_script_ids](const auto& id) {
                    return base::Contains(removed_script_ids, id);
                  });

    helper_.SetDynamicScripts(loaded_dynamic_scripts_,
                              persistent_dynamic_script_ids_);
  }

  std::move(callback).Run(error);
}

}  // namespace extensions
