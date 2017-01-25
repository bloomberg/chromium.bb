// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/catalog/catalog.h"

#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/filesystem/directory_impl.h"
#include "components/filesystem/lock_table.h"
#include "components/filesystem/public/interfaces/types.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/catalog/constants.h"
#include "services/catalog/entry_cache.h"
#include "services/catalog/instance.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace catalog {

namespace {

const char kCatalogServicesKey[] = "services";
const char kCatalogServiceEmbeddedKey[] = "embedded";
const char kCatalogServiceExecutableKey[] = "executable";
const char kCatalogServiceManifestKey[] = "manifest";

base::LazyInstance<std::unique_ptr<base::Value>> g_default_static_manifest =
    LAZY_INSTANCE_INITIALIZER;

void LoadCatalogManifestIntoCache(const base::Value* root, EntryCache* cache) {
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

    auto entry = Entry::Deserialize(*manifest);
    if (entry) {
      if (!executable_path.empty())
        entry->set_path(executable_path);
      bool added = cache->AddRootEntry(std::move(entry));
      DCHECK(added);
    } else {
      LOG(ERROR) << "Failed to read manifest entry for \"" << it.key() << "\".";
    }
  }
}

}  // namespace

class Catalog::ServiceImpl : public service_manager::Service {
 public:
  explicit ServiceImpl(Catalog* catalog) : catalog_(catalog) {}
  ~ServiceImpl() override {}

  // service_manager::Service:
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override {
    registry->AddInterface<mojom::Catalog>(catalog_);
    registry->AddInterface<filesystem::mojom::Directory>(catalog_);
    registry->AddInterface<service_manager::mojom::Resolver>(catalog_);
    return true;
  }

 private:
  Catalog* const catalog_;

  DISALLOW_COPY_AND_ASSIGN(ServiceImpl);
};

Catalog::Catalog(std::unique_ptr<base::Value> static_manifest,
                 ManifestProvider* service_manifest_provider)
    : service_context_(new service_manager::ServiceContext(
          base::MakeUnique<ServiceImpl>(this),
          service_manager::mojom::ServiceRequest(&service_))),
      service_manifest_provider_(service_manifest_provider),
      weak_factory_(this) {
  if (static_manifest) {
    LoadCatalogManifestIntoCache(static_manifest.get(), &system_cache_);
  } else if (g_default_static_manifest.Get()) {
    LoadCatalogManifestIntoCache(
        g_default_static_manifest.Get().get(), &system_cache_);
  }
}

Catalog::~Catalog() {}

service_manager::mojom::ServicePtr Catalog::TakeService() {
  return std::move(service_);
}

// static
void Catalog::SetDefaultCatalogManifest(
    std::unique_ptr<base::Value> static_manifest) {
  g_default_static_manifest.Get() = std::move(static_manifest);
}

// static
void Catalog::LoadDefaultCatalogManifest(const base::FilePath& path) {
  std::string catalog_contents;
  base::FilePath exe_path;
  base::PathService::Get(base::DIR_EXE, &exe_path);
  base::FilePath catalog_path = exe_path.Append(path);
  bool result = base::ReadFileToString(catalog_path, &catalog_contents);
  DCHECK(result);
  std::unique_ptr<base::Value> manifest_value =
      base::JSONReader::Read(catalog_contents);
  DCHECK(manifest_value);
  catalog::Catalog::SetDefaultCatalogManifest(std::move(manifest_value));
}

void Catalog::Create(const service_manager::Identity& remote_identity,
                     service_manager::mojom::ResolverRequest request) {
  Instance* instance = GetInstanceForUserId(remote_identity.user_id());
  instance->BindResolver(std::move(request));
}

void Catalog::Create(const service_manager::Identity& remote_identity,
                     mojom::CatalogRequest request) {
  Instance* instance = GetInstanceForUserId(remote_identity.user_id());
  instance->BindCatalog(std::move(request));
}

void Catalog::Create(const service_manager::Identity& remote_identity,
                     filesystem::mojom::DirectoryRequest request) {
  if (!lock_table_)
    lock_table_ = new filesystem::LockTable;

  base::FilePath resources_path;
  base::PathService::Get(base::DIR_MODULE, &resources_path);
  mojo::MakeStrongBinding(
      base::MakeUnique<filesystem::DirectoryImpl>(
          resources_path, scoped_refptr<filesystem::SharedTempDir>(),
          lock_table_),
      std::move(request));
}

Instance* Catalog::GetInstanceForUserId(const std::string& user_id) {
  auto it = instances_.find(user_id);
  if (it != instances_.end())
    return it->second.get();

  auto result = instances_.insert(std::make_pair(
      user_id,
      base::MakeUnique<Instance>(&system_cache_, service_manifest_provider_)));
  return result.first->second.get();
}

}  // namespace catalog
