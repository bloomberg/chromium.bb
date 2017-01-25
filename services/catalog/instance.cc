// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/catalog/instance.h"

#include <memory>

#include "base/bind.h"
#include "base/values.h"
#include "services/catalog/entry.h"
#include "services/catalog/entry_cache.h"
#include "services/catalog/manifest_provider.h"

namespace catalog {
namespace {

void AddEntry(const Entry& entry, std::vector<mojom::EntryPtr>* ary) {
  mojom::EntryPtr entry_ptr(mojom::Entry::New());
  entry_ptr->name = entry.name();
  entry_ptr->display_name = entry.display_name();
  ary->push_back(std::move(entry_ptr));
}

}  // namespace

Instance::Instance(EntryCache* system_cache,
                   ManifestProvider* service_manifest_provider)
    : system_cache_(system_cache),
      service_manifest_provider_(service_manifest_provider) {}

Instance::~Instance() {}

void Instance::BindResolver(service_manager::mojom::ResolverRequest request) {
  resolver_bindings_.AddBinding(this, std::move(request));
}

void Instance::BindCatalog(mojom::CatalogRequest request) {
  catalog_bindings_.AddBinding(this, std::move(request));
}

void Instance::ResolveServiceName(const std::string& service_name,
                                  const ResolveServiceNameCallback& callback) {
  DCHECK(system_cache_);

  // TODO(beng): per-user catalogs.
  const Entry* entry = system_cache_->GetEntry(service_name);
  if (entry) {
    callback.Run(service_manager::mojom::ResolveResult::From(entry),
                 service_manager::mojom::ResolveResult::From(entry->parent()));
    return;
  } else if (service_manifest_provider_) {
    auto manifest = service_manifest_provider_->GetManifest(service_name);
    if (manifest) {
      auto entry = Entry::Deserialize(*manifest);
      if (entry) {
        callback.Run(
            service_manager::mojom::ResolveResult::From(
                const_cast<const Entry*>(entry.get())),
            service_manager::mojom::ResolveResult::From(entry->parent()));

        bool added = system_cache_->AddRootEntry(std::move(entry));
        DCHECK(added);
        return;
      } else {
        LOG(ERROR) << "Received malformed manifest for " << service_name;
      }
    }
  }

  LOG(ERROR) << "Unable to locate service manifest for " << service_name;
  callback.Run(nullptr, nullptr);
}

void Instance::GetEntries(const base::Optional<std::vector<std::string>>& names,
                          const GetEntriesCallback& callback) {
  DCHECK(system_cache_);

  std::vector<mojom::EntryPtr> entries;
  if (!names.has_value()) {
    // TODO(beng): user catalog.
    for (const auto& entry : system_cache_->entries())
      AddEntry(*entry.second, &entries);
  } else {
    for (const std::string& name : names.value()) {
      const Entry* entry = system_cache_->GetEntry(name);
      // TODO(beng): user catalog.
      if (entry)
        AddEntry(*entry, &entries);
    }
  }
  callback.Run(std::move(entries));
}

void Instance::GetEntriesProvidingCapability(
    const std::string& capability,
    const GetEntriesProvidingCapabilityCallback& callback) {
  std::vector<mojom::EntryPtr> entries;
  for (const auto& entry : system_cache_->entries())
    if (entry.second->ProvidesCapability(capability))
      entries.push_back(mojom::Entry::From(*entry.second));
  callback.Run(std::move(entries));
}

void Instance::GetEntriesConsumingMIMEType(
    const std::string& mime_type,
    const GetEntriesConsumingMIMETypeCallback& callback) {
  // TODO(beng): implement.
}

void Instance::GetEntriesSupportingScheme(
    const std::string& scheme,
    const GetEntriesSupportingSchemeCallback& callback) {
  // TODO(beng): implement.
}

}  // namespace catalog
