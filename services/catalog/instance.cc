// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/catalog/instance.h"

#include "base/bind.h"
#include "services/catalog/entry.h"
#include "services/catalog/entry_cache.h"
#include "services/catalog/manifest_provider.h"
#include "services/catalog/reader.h"

namespace catalog {
namespace {

void AddEntry(const Entry& entry, std::vector<mojom::EntryPtr>* ary) {
  mojom::EntryPtr entry_ptr(mojom::Entry::New());
  entry_ptr->name = entry.name();
  entry_ptr->display_name = entry.display_name();
  ary->push_back(std::move(entry_ptr));
}

}  // namespace

Instance::Instance(Reader* system_reader) : system_reader_(system_reader) {}

Instance::~Instance() {}

void Instance::BindResolver(service_manager::mojom::ResolverRequest request) {
  if (system_cache_)
    resolver_bindings_.AddBinding(this, std::move(request));
  else
    pending_resolver_requests_.push_back(std::move(request));
}

void Instance::BindCatalog(mojom::CatalogRequest request) {
  if (system_cache_)
    catalog_bindings_.AddBinding(this, std::move(request));
  else
    pending_catalog_requests_.push_back(std::move(request));
}

void Instance::CacheReady(EntryCache* cache) {
  system_cache_ = cache;
  for (auto& request : pending_resolver_requests_)
    BindResolver(std::move(request));
  for (auto& request : pending_catalog_requests_)
    BindCatalog(std::move(request));
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
  }

  system_reader_->CreateEntryForName(service_name, system_cache_, callback);
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
