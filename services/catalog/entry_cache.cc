// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/catalog/entry_cache.h"

#include <map>
#include <set>
#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "services/catalog/entry.h"
#include "services/service_manager/public/cpp/interface_provider_spec.h"
#include "services/service_manager/public/mojom/interface_provider_spec.mojom.h"

namespace catalog {

namespace {

#if defined(OS_WIN)
const char kServiceExecutableExtension[] = ".service.exe";
#else
const char kServiceExecutableExtension[] = ".service";
#endif

// Temporary helper to create a cache Entry from a Manifest object. This can
// disappear once the Service Manager just uses Manifest objects directly.
std::unique_ptr<Entry> MakeEntryFromManifest(
    const service_manager::Manifest& manifest) {
  auto entry = std::make_unique<Entry>();
  entry->set_name(manifest.service_name);
  entry->set_display_name(manifest.display_name.raw_string);

  base::FilePath service_exe_root;
  CHECK(base::PathService::Get(base::DIR_ASSETS, &service_exe_root));
  entry->set_path(service_exe_root.AppendASCII(entry->name() +
                                               kServiceExecutableExtension));

  entry->set_sandbox_type(manifest.options.sandbox_type);

  ServiceOptions options;
  switch (manifest.options.instance_sharing_policy) {
    case service_manager::Manifest::InstanceSharingPolicy::kNoSharing:
      options.instance_sharing = ServiceOptions::InstanceSharingType::NONE;
      break;
    case service_manager::Manifest::InstanceSharingPolicy::kSingleton:
      options.instance_sharing = ServiceOptions::InstanceSharingType::SINGLETON;
      break;
    case service_manager::Manifest::InstanceSharingPolicy::kSharedAcrossGroups:
      options.instance_sharing =
          ServiceOptions::InstanceSharingType::SHARED_ACROSS_INSTANCE_GROUPS;
      break;
  }
  options.can_connect_to_instances_in_any_group =
      manifest.options.can_connect_to_instances_in_any_group;
  options.can_connect_to_other_services_with_any_instance_name =
      manifest.options.can_connect_to_instances_with_any_id;
  options.can_create_other_service_instances =
      manifest.options.can_register_other_service_instances;
  options.interfaces_bindable_on_any_service =
      manifest.interfaces_bindable_on_any_service;
  entry->AddOptions(options);

  service_manager::InterfaceProviderSpec main_spec;
  for (const auto& entry : manifest.exposed_capabilities)
    main_spec.provides.emplace(entry.capability_name, entry.interface_names);
  for (const auto& entry : manifest.required_capabilities) {
    main_spec.requires[entry.service_name].insert(entry.capability_name);
  }

  entry->AddInterfaceProviderSpec(
      service_manager::mojom::kServiceManager_ConnectorSpec,
      std::move(main_spec));

  std::map<std::string, service_manager::InterfaceProviderSpec> other_specs;
  for (const auto& entry : manifest.exposed_interface_filter_capabilities) {
    other_specs[entry.filter_name].provides.emplace(entry.capability_name,
                                                    entry.interface_names);
  }

  for (const auto& entry : manifest.required_interface_filter_capabilities) {
    other_specs[entry.filter_name].requires[entry.service_name].insert(
        entry.capability_name);
  }

  for (auto& spec : other_specs)
    entry->AddInterfaceProviderSpec(spec.first, std::move(spec.second));

  for (const auto& file : manifest.preloaded_files)
    entry->AddRequiredFilePath(file.key, file.path);

  for (const auto& packaged_service_manifest : manifest.packaged_services) {
    auto child = MakeEntryFromManifest(packaged_service_manifest);
    child->set_parent(entry.get());
    entry->children().emplace_back(std::move(child));
  }

  return entry;
}

}  // namespace

EntryCache::EntryCache() {}

EntryCache::~EntryCache() {}

bool EntryCache::AddRootEntry(std::unique_ptr<Entry> entry) {
  DCHECK(entry);
  const std::string& name = entry->name();
  if (!AddEntry(entry.get()))
    return false;
  root_entries_.insert(std::make_pair(name, std::move(entry)));
  return true;
}

bool EntryCache::AddRootEntryFromManifest(
    const service_manager::Manifest& manifest) {
  return AddRootEntry(MakeEntryFromManifest(manifest));
}

const Entry* EntryCache::GetEntry(const std::string& name) {
  auto iter = entries_.find(name);
  if (iter == entries_.end())
    return nullptr;
  return iter->second;
}

bool EntryCache::AddEntry(const Entry* entry) {
  auto root_iter = root_entries_.find(entry->name());
  if (root_iter != root_entries_.end()) {
    RemoveEntry(root_iter->second.get());
    root_entries_.erase(root_iter);
  } else {
    auto entry_iter = entries_.find(entry->name());
    if (entry_iter != entries_.end()) {
      // There's already a non-root entry for this name, so we change nothing.
      return false;
    }
  }

  entries_.insert({ entry->name(), entry });
  for (const auto& child : entry->children())
    AddEntry(child.get());
  return true;
}

void EntryCache::RemoveEntry(const Entry* entry) {
  auto iter = entries_.find(entry->name());
  if (iter->second == entry)
    entries_.erase(iter);
  for (const auto& child : entry->children())
    RemoveEntry(child.get());
}

}  // namespace catalog
