// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/catalog/reader.h"

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "services/catalog/constants.h"
#include "services/catalog/entry.h"
#include "services/catalog/entry_cache.h"
#include "services/catalog/manifest_provider.h"
#include "services/catalog/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/interfaces/constants.mojom.h"

namespace catalog {
namespace {

#if defined(OS_WIN)
const char kServiceExecutableExtension[] = ".service.exe";
#else
const char kServiceExecutableExtension[] = ".service";
#endif

const char kCatalogServicesKey[] = "services";
const char kCatalogServiceEmbeddedKey[] = "embedded";
const char kCatalogServiceExecutableKey[] = "executable";
const char kCatalogServiceManifestKey[] = "manifest";

base::FilePath GetManifestPath(const base::FilePath& package_dir,
                               const std::string& name,
                               const std::string& package_name_override) {
  // TODO(beng): think more about how this should be done for exe targets.
  std::string package_name =
      package_name_override.empty() ? name : package_name_override;
  return package_dir.AppendASCII(kPackagesDirName).AppendASCII(
      package_name + "/manifest.json");
}

base::FilePath GetExecutablePath(const base::FilePath& package_dir,
                                 const std::string& name) {
  return package_dir.AppendASCII(
      name + "/" + name + kServiceExecutableExtension);
}

std::unique_ptr<Entry> ProcessManifest(const base::Value* manifest_root,
                                       const base::FilePath& package_dir,
                                       const base::FilePath& executable_path) {

  // Manifest was malformed or did not exist.
  if (!manifest_root)
    return nullptr;

  const base::DictionaryValue* dictionary = nullptr;
  if (!manifest_root->GetAsDictionary(&dictionary))
    return nullptr;

  std::unique_ptr<Entry> entry = Entry::Deserialize(*dictionary);
  if (!entry)
    return nullptr;
  if (!executable_path.empty())
    entry->set_path(executable_path);
  else
    entry->set_path(GetExecutablePath(package_dir, entry->name()));
  return entry;
}

std::unique_ptr<Entry> ProcessUniqueManifest(
    std::unique_ptr<base::Value> manifest_root,
    const base::FilePath& package_dir) {
  return ProcessManifest(manifest_root.get(), package_dir, base::FilePath());
}

std::unique_ptr<Entry> CreateEntryForManifestAt(
    const base::FilePath& manifest_path,
    const base::FilePath& package_dir) {
  JSONFileValueDeserializer deserializer(manifest_path);
  int error = 0;
  std::string message;

  // TODO(beng): probably want to do more detailed error checking. This should
  //             be done when figuring out if to unblock connection completion.
  return ProcessUniqueManifest(deserializer.Deserialize(&error, &message),
                               package_dir);
}

void ScanDir(
    const base::FilePath& package_dir,
    const Reader::ReadManifestCallback& read_manifest_callback,
    scoped_refptr<base::SingleThreadTaskRunner> original_thread_task_runner,
    const base::Closure& read_complete_closure) {
  base::FileEnumerator enumerator(package_dir, false,
                                  base::FileEnumerator::DIRECTORIES);
  while (1) {
    base::FilePath path = enumerator.Next();
    if (path.empty())
      break;
    base::FilePath manifest_path = path.AppendASCII("manifest.json");
    std::unique_ptr<Entry> entry =
        CreateEntryForManifestAt(manifest_path, package_dir);
    if (!entry)
      continue;

    // Skip over subdirs that contain only manifests, they're artifacts of the
    // build (e.g. for applications that are packaged into others) and are not
    // valid standalone packages.
    base::FilePath package_path = GetExecutablePath(package_dir, entry->name());
    if (entry->name() != service_manager::mojom::kServiceName &&
        entry->name() != catalog::mojom::kServiceName &&
        !base::PathExists(package_path)) {
      continue;
    }

    original_thread_task_runner->PostTask(
        FROM_HERE,
        base::Bind(read_manifest_callback, base::Passed(&entry)));
  }

  original_thread_task_runner->PostTask(FROM_HERE, read_complete_closure);
}

std::unique_ptr<Entry> ReadManifest(
    const base::FilePath& package_dir,
    const std::string& mojo_name,
    const std::string& package_name_override,
    const base::FilePath& manifest_path_override) {
  base::FilePath manifest_path;
  if (manifest_path_override.empty()) {
    manifest_path =
        GetManifestPath(package_dir, mojo_name, package_name_override);
  } else {
    manifest_path = manifest_path_override;
  }

  std::unique_ptr<Entry> entry = CreateEntryForManifestAt(manifest_path,
                                                          package_dir);
  if (!entry) {
    entry.reset(new Entry(mojo_name));
    entry->set_path(GetExecutablePath(
        package_dir.AppendASCII(kPackagesDirName), mojo_name));
  }
  return entry;
}

void IgnoreResolveResult(service_manager::mojom::ResolveResultPtr,
                         service_manager::mojom::ResolveResultPtr) {}

void LoadCatalogManifestIntoCache(const base::Value* root,
                                  const base::FilePath& package_dir,
                                  EntryCache* cache) {
  DCHECK(root);
  const base::DictionaryValue* catalog = nullptr;
  if (!root->GetAsDictionary(&catalog)) {
    LOG(ERROR) << "Catalog manifest is not a dictionary value.";
    return;
  }
  DCHECK(catalog);

  const base::DictionaryValue* services = nullptr;
  if (!catalog->GetDictionary(kCatalogServicesKey, &services)) {
    LOG(ERROR) << "Catalog manifest \"services\" is not a dictionary value.";
    return;
  }

  for (base::DictionaryValue::Iterator it(*services); !it.IsAtEnd();
       it.Advance()) {
    const base::DictionaryValue* service_entry = nullptr;
    if (!it.value().GetAsDictionary(&service_entry)) {
      LOG(ERROR) << "Catalog service entry for \"" << it.key()
                 << "\" is not a dictionary value.";
      continue;
    }

    bool is_embedded = false;
    service_entry->GetBoolean(kCatalogServiceEmbeddedKey, &is_embedded);

    base::FilePath executable_path;
    std::string executable_path_string;
    if (service_entry->GetString(kCatalogServiceExecutableKey,
                                 &executable_path_string)) {
      base::FilePath exe_dir;
      CHECK(base::PathService::Get(base::DIR_EXE, &exe_dir));
#if defined(OS_WIN)
      executable_path_string += ".exe";
      base::ReplaceFirstSubstringAfterOffset(
          &executable_path_string, 0, "@EXE_DIR",
          base::UTF16ToUTF8(exe_dir.value()));
      executable_path =
          base::FilePath(base::UTF8ToUTF16(executable_path_string));
#else
      base::ReplaceFirstSubstringAfterOffset(
          &executable_path_string, 0, "@EXE_DIR", exe_dir.value());
      executable_path = base::FilePath(executable_path_string);
#endif
    }

    const base::DictionaryValue* manifest = nullptr;
    if (!service_entry->GetDictionary(kCatalogServiceManifestKey, &manifest)) {
      LOG(ERROR) << "Catalog entry for \"" << it.key() << "\" has an invalid "
                 << "\"manifest\" value.";
      continue;
    }

    DCHECK(!(is_embedded && !executable_path.empty()));

    auto entry = ProcessManifest(
        manifest, is_embedded ? base::FilePath() : package_dir,
        executable_path);
    if (entry) {
      bool added = cache->AddRootEntry(std::move(entry));
      DCHECK(added);
    } else {
      LOG(ERROR) << "Failed to read manifest entry for \"" << it.key() << "\".";
    }
  }
}

}  // namespace

Reader::Reader(std::unique_ptr<base::Value> static_manifest,
               EntryCache* cache)
    : using_static_catalog_(true),
      manifest_provider_(nullptr),
      weak_factory_(this) {
  PathService::Get(base::DIR_MODULE, &system_package_dir_);
  LoadCatalogManifestIntoCache(
      static_manifest.get(), system_package_dir_.AppendASCII(kPackagesDirName),
      cache);
}

// A sequenced task runner is used to guarantee requests are serviced in the
// order requested. To do otherwise means we may run callbacks in an
// unpredictable order, leading to flake.
Reader::Reader(base::SequencedWorkerPool* worker_pool,
               ManifestProvider* manifest_provider)
    : Reader(manifest_provider) {
  file_task_runner_ = worker_pool->GetSequencedTaskRunnerWithShutdownBehavior(
      base::SequencedWorkerPool::GetSequenceToken(),
      base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
}

Reader::Reader(base::SingleThreadTaskRunner* task_runner,
               ManifestProvider* manifest_provider)
    : Reader(manifest_provider) {
  file_task_runner_ = task_runner;
}

Reader::~Reader() {}

void Reader::Read(const base::FilePath& package_dir,
                  EntryCache* cache,
                  const base::Closure& read_complete_closure) {
  file_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ScanDir, package_dir,
                 base::Bind(&Reader::OnReadManifest, weak_factory_.GetWeakPtr(),
                            cache, base::Bind(&IgnoreResolveResult)),
                 base::ThreadTaskRunnerHandle::Get(),
                 read_complete_closure));
}

void Reader::CreateEntryForName(
    const std::string& name,
    EntryCache* cache,
    const CreateEntryForNameCallback& entry_created_callback) {
  if (manifest_provider_) {
    std::unique_ptr<base::Value> manifest_root =
        manifest_provider_->GetManifest(name);
    if (manifest_root) {
      base::PostTaskAndReplyWithResult(
          file_task_runner_.get(), FROM_HERE,
          base::Bind(&ProcessUniqueManifest, base::Passed(&manifest_root),
                     system_package_dir_),
          base::Bind(&Reader::OnReadManifest, weak_factory_.GetWeakPtr(), cache,
                     entry_created_callback));
      return;
    }
  } else if (using_static_catalog_) {
    // A Reader using a static catalog manifest does not support dynamic
    // discovery or introduction of new catalog entries.
    entry_created_callback.Run(service_manager::mojom::ResolveResultPtr(),
                               service_manager::mojom::ResolveResultPtr());
    return;
  }

  base::FilePath manifest_path_override;
  {
    auto override_iter = manifest_path_overrides_.find(name);
    if (override_iter != manifest_path_overrides_.end())
      manifest_path_override = override_iter->second;
  }

  std::string package_name_override;
  {
    auto override_iter = package_name_overrides_.find(name);
    if (override_iter != package_name_overrides_.end())
      package_name_override = override_iter->second;
  }
  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(), FROM_HERE,
      base::Bind(&ReadManifest, system_package_dir_, name,
                 package_name_override, manifest_path_override),
      base::Bind(&Reader::OnReadManifest, weak_factory_.GetWeakPtr(), cache,
                  entry_created_callback));
}

void Reader::OverridePackageName(const std::string& service_name,
                                 const std::string& package_name) {
  package_name_overrides_.insert(std::make_pair(service_name, package_name));
}

void Reader::OverrideManifestPath(const std::string& service_name,
                                  const base::FilePath& path) {
  manifest_path_overrides_.insert(std::make_pair(service_name, path));
}

Reader::Reader(ManifestProvider* manifest_provider)
    : using_static_catalog_(false),
      manifest_provider_(manifest_provider),
      weak_factory_(this) {
  PathService::Get(base::DIR_MODULE, &system_package_dir_);
}

void Reader::OnReadManifest(
    EntryCache* cache,
    const CreateEntryForNameCallback& entry_created_callback,
    std::unique_ptr<Entry> entry) {
  if (!entry)
    return;

  std::string name = entry->name();
  cache->AddRootEntry(std::move(entry));

  // NOTE: It's currently possible to end up with a duplicate entry, in which
  // case the above call to AddRootEntry() may actually discard |entry|. We
  // therefore look up the Entry by name here to get resolution metadata.

  const Entry* resolved_entry = cache->GetEntry(name);
  DCHECK(resolved_entry);
  entry_created_callback.Run(
      service_manager::mojom::ResolveResult::From(resolved_entry),
      service_manager::mojom::ResolveResult::From(resolved_entry->parent()));
}

}  // namespace catalog
