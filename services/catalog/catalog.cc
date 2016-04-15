// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/catalog/catalog.h"

#include "base/bind.h"
#include "services/catalog/entry.h"
#include "services/catalog/manifest_provider.h"
#include "services/catalog/reader.h"
#include "services/catalog/store.h"
#include "services/shell/public/cpp/names.h"

namespace catalog {
namespace {

void AddEntryToMap(const Entry& entry,
                   mojo::Map<mojo::String, mojom::CatalogEntryPtr>* map) {
  mojom::CatalogEntryPtr entry_ptr(mojom::CatalogEntry::New());
  entry_ptr->display_name = entry.display_name();
  (*map)[entry.name()] = std::move(entry_ptr);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Catalog, public:

Catalog::Catalog(scoped_ptr<Store> store, Reader* system_reader)
    : store_(std::move(store)),
      system_reader_(system_reader),
      weak_factory_(this) {}
Catalog::~Catalog() {}

void Catalog::BindResolver(mojom::ResolverRequest request) {
  if (system_catalog_)
    resolver_bindings_.AddBinding(this, std::move(request));
  else
    pending_resolver_requests_.push_back(std::move(request));
}

void Catalog::BindShellResolver(
    shell::mojom::ShellResolverRequest request) {
  if (system_catalog_)
    shell_resolver_bindings_.AddBinding(this, std::move(request));
  else
    pending_shell_resolver_requests_.push_back(std::move(request));
}

void Catalog::BindCatalog(mojom::CatalogRequest request) {
  if (system_catalog_)
    catalog_bindings_.AddBinding(this, std::move(request));
  else
    pending_catalog_requests_.push_back(std::move(request));
}

void Catalog::CacheReady(EntryCache* cache) {
  system_catalog_ = cache;
  DeserializeCatalog();
  for (auto& request : pending_resolver_requests_)
    BindResolver(std::move(request));
  for (auto& request : pending_shell_resolver_requests_)
    BindShellResolver(std::move(request));
  for (auto& request : pending_catalog_requests_)
    BindCatalog(std::move(request));
}

////////////////////////////////////////////////////////////////////////////////
// Catalog, mojom::Resolver:

void Catalog::ResolveInterfaces(mojo::Array<mojo::String> interfaces,
                                const ResolveInterfacesCallback& callback) {
  // TODO(beng): implement.
}

void Catalog::ResolveMIMEType(const mojo::String& mime_type,
                              const ResolveMIMETypeCallback& callback) {
  // TODO(beng): implement.
}

void Catalog::ResolveProtocolScheme(
    const mojo::String& scheme,
    const ResolveProtocolSchemeCallback& callback) {
  // TODO(beng): implement.
}

////////////////////////////////////////////////////////////////////////////////
// Catalog, shell::mojom::ShellResolver:

void Catalog::ResolveMojoName(const mojo::String& mojo_name,
                              const ResolveMojoNameCallback& callback) {
  DCHECK(system_catalog_);

  std::string type = shell::GetNameType(mojo_name);
  if (type != shell::kNameType_Mojo && type != shell::kNameType_Exe) {
    scoped_ptr<Entry> entry(new Entry(mojo_name));
    callback.Run(shell::mojom::ResolveResult::From(*entry));
    return;
  }

  // TODO(beng): per-user catalogs.
  auto entry = system_catalog_->find(mojo_name);
  if (entry != system_catalog_->end()) {
    callback.Run(shell::mojom::ResolveResult::From(*entry->second));
    return;
  }

  // Manifests for mojo: names should always be in the catalog by this point.
  //DCHECK(type == shell::kNameType_Exe);
  system_reader_->CreateEntryForName(
      mojo_name, system_catalog_,
      base::Bind(&Catalog::OnReadManifest, weak_factory_.GetWeakPtr(),
                 mojo_name, callback));
}

////////////////////////////////////////////////////////////////////////////////
// Catalog, mojom::Catalog:

void Catalog::GetEntries(mojo::Array<mojo::String> names,
                         const GetEntriesCallback& callback) {
  DCHECK(system_catalog_);

  mojo::Map<mojo::String, mojom::CatalogEntryPtr> entries;
  if (names.is_null()) {
    // TODO(beng): user catalog.
    for (const auto& entry : *system_catalog_)
      AddEntryToMap(*entry.second, &entries);
  } else {
    std::vector<mojo::String> names_vec = names.PassStorage();
    for (const std::string& name : names_vec) {
      Entry* entry = nullptr;
      // TODO(beng): user catalog.
      if (system_catalog_->find(name) != system_catalog_->end())
        entry = (*system_catalog_)[name].get();
      else
        continue;
      AddEntryToMap(*entry, &entries);
    }
  }
  callback.Run(std::move(entries));
}

////////////////////////////////////////////////////////////////////////////////
// Catalog, private:

void Catalog::DeserializeCatalog() {
  DCHECK(system_catalog_);
  if (!store_)
    return;
  const base::ListValue* catalog = store_->GetStore();
  CHECK(catalog);
  // TODO(sky): make this handle aliases.
  // TODO(beng): implement this properly!
  for (auto it = catalog->begin(); it != catalog->end(); ++it) {
    const base::DictionaryValue* dictionary = nullptr;
    const base::Value* v = *it;
    CHECK(v->GetAsDictionary(&dictionary));
    scoped_ptr<Entry> entry = Entry::Deserialize(*dictionary);
    // TODO(beng): user catalog.
    if (entry)
      (*system_catalog_)[entry->name()] = std::move(entry);
  }
}

void Catalog::SerializeCatalog() {
  DCHECK(system_catalog_);
  scoped_ptr<base::ListValue> catalog(new base::ListValue);
  // TODO(beng): user catalog.
  for (const auto& entry : *system_catalog_)
    catalog->Append(entry.second->Serialize());
  if (store_)
    store_->UpdateStore(std::move(catalog));
}

// static
void Catalog::OnReadManifest(base::WeakPtr<Catalog> catalog,
                             const std::string& mojo_name,
                             const ResolveMojoNameCallback& callback,
                             shell::mojom::ResolveResultPtr result) {
  callback.Run(std::move(result));
  if (catalog)
    catalog->SerializeCatalog();
}

}  // namespace catalog
