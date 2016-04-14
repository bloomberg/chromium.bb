// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CATALOG_CATALOG_H_
#define SERVICES_CATALOG_CATALOG_H_

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

class ManifestProvider;
class Store;

struct ReadManifestResult {
  ReadManifestResult();
  ~ReadManifestResult();
  shell::mojom::ResolveResultPtr resolve_result;
  scoped_ptr<Entry> catalog_entry;
  base::FilePath package_dir;
};

class Catalog : public mojom::Resolver,
                public shell::mojom::ShellResolver,
                public mojom::Catalog {
 public:
  // |manifest_provider| may be null.
  Catalog(scoped_ptr<Store> store,
          base::TaskRunner* file_task_runner,
          EntryCache* system_catalog,
          ManifestProvider* manifest_provider);
  ~Catalog() override;

  void BindResolver(mojom::ResolverRequest request);
  void BindShellResolver(shell::mojom::ShellResolverRequest request);
  void BindCatalog(mojom::CatalogRequest request);

 private:
  using MojoNameAliasMap =
      std::map<std::string, std::pair<std::string, std::string>>;

  // mojom::Resolver:
  void ResolveInterfaces(mojo::Array<mojo::String> interfaces,
                         const ResolveInterfacesCallback& callback) override;
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

  // Populate/serialize the catalog from/to the supplied store.
  void DeserializeCatalog();
  void SerializeCatalog();

  // Receives the result of manifest parsing on |file_task_runner_|, may be
  // received after the catalog object that issued the request is destroyed.
  static void OnReadManifest(base::WeakPtr<Catalog> catalog,
                             const std::string& name,
                             const ResolveMojoNameCallback& callback,
                             scoped_ptr<ReadManifestResult> result);

  // Populate the catalog with data from |entry|, and pass it to the client
  // via callback.
  void AddEntryToCatalog(scoped_ptr<Entry> entry, bool is_system_catalog);

  ManifestProvider* const manifest_provider_;

  // Directory that contains packages and executables visible to all users.
  base::FilePath system_package_dir_;
  // Directory that contains packages visible to this Catalog instance's user.
  base::FilePath user_package_dir_;

  // User-specific persistent storage of package manifests and other settings.
  scoped_ptr<Store> store_;

  // Task runner for performing file operations.
  base::TaskRunner* const file_task_runner_;

  mojo::BindingSet<mojom::Resolver> resolver_bindings_;
  mojo::BindingSet<shell::mojom::ShellResolver> shell_resolver_bindings_;
  mojo::BindingSet<mojom::Catalog> catalog_bindings_;

  // The current user's packages, constructed from Store/package manifests.
  EntryCache user_catalog_;
  // Same as above, but for system-level (visible to all-users) packages and
  // executables.
  EntryCache* system_catalog_;

  base::WeakPtrFactory<Catalog> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Catalog);
};

}  // namespace catalog

#endif  // SERVICES_CATALOG_CATALOG_H_
