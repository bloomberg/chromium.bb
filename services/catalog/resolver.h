// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CATALOG_RESOLVER_H_
#define SERVICES_CATALOG_RESOLVER_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/catalog/entry.h"
#include "services/catalog/public/interfaces/catalog.mojom.h"
#include "services/catalog/public/interfaces/resolver.mojom.h"
#include "services/catalog/store.h"
#include "services/catalog/types.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/interfaces/shell_resolver.mojom.h"

namespace catalog {

class Reader;
class Store;

class Resolver : public mojom::Resolver,
                 public shell::mojom::ShellResolver,
                 public mojom::Catalog {
 public:
  // |manifest_provider| may be null.
  Resolver(scoped_ptr<Store> store, Reader* system_reader);
  ~Resolver() override;

  void BindResolver(mojom::ResolverRequest request);
  void BindShellResolver(shell::mojom::ShellResolverRequest request);
  void BindCatalog(mojom::CatalogRequest request);

  // Called when |cache| has been populated by a directory scan.
  void CacheReady(EntryCache* cache);

 private:
  using MojoNameAliasMap =
      std::map<std::string, std::pair<std::string, std::string>>;

  // mojom::Resolver:
  void ResolveClass(const mojo::String& clazz,
                    const ResolveClassCallback& callback) override;
  void ResolveMIMEType(const mojo::String& mime_type,
                       const ResolveMIMETypeCallback& callback) override;
  void ResolveProtocolScheme(
      const mojo::String& scheme,
      const ResolveProtocolSchemeCallback& callback) override;

  // shell::mojom::ShellResolver:
  void ResolveMojoName(const mojo::String& mojo_name,
                       const ResolveMojoNameCallback& callback) override;

  // mojom::Catalog:
  void GetEntries(mojo::Array<mojo::String> names,
                  const GetEntriesCallback& callback) override;

  // Populate/serialize the cache from/to the supplied store.
  void DeserializeCatalog();
  void SerializeCatalog();

  // Receives the result of manifest parsing, may be received after the
  // catalog object that issued the request is destroyed.
  static void OnReadManifest(base::WeakPtr<Resolver> resolver,
                             const std::string& mojo_name,
                             const ResolveMojoNameCallback& callback,
                             shell::mojom::ResolveResultPtr result);

  // User-specific persistent storage of package manifests and other settings.
  scoped_ptr<Store> store_;

  mojo::BindingSet<mojom::Resolver> resolver_bindings_;
  mojo::BindingSet<shell::mojom::ShellResolver> shell_resolver_bindings_;
  mojo::BindingSet<mojom::Catalog> catalog_bindings_;

  Reader* system_reader_;

  // A map of name -> Entry data structure for system-level packages (i.e. those
  // that are visible to all users).
  // TODO(beng): eventually add per-user applications.
  EntryCache* system_catalog_ = nullptr;

  // We only bind requests for these interfaces once the catalog has been
  // populated. These data structures queue requests until that happens.
  std::vector<mojom::ResolverRequest> pending_resolver_requests_;
  std::vector<shell::mojom::ShellResolverRequest>
      pending_shell_resolver_requests_;
  std::vector<mojom::CatalogRequest> pending_catalog_requests_;

  base::WeakPtrFactory<Resolver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Resolver);
};

}  // namespace catalog

#endif  // SERVICES_CATALOG_RESOLVER_H_
