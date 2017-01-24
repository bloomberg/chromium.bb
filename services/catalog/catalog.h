// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CATALOG_CATALOG_H_
#define SERVICES_CATALOG_CATALOG_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/filesystem/public/interfaces/directory.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/catalog/entry_cache.h"
#include "services/catalog/public/interfaces/catalog.mojom.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/interfaces/resolver.mojom.h"
#include "services/service_manager/public/interfaces/service.mojom.h"

namespace base {
class FilePath;
class Value;
}

namespace filesystem {
class LockTable;
}

namespace service_manager {
class ServiceContext;
}

namespace catalog {

class Instance;
class ManifestProvider;

// Creates and owns an instance of the catalog. Exposes a ServicePtr that
// can be passed to the service manager, potentially in a different process.
class Catalog
    : public service_manager::InterfaceFactory<mojom::Catalog>,
      public service_manager::InterfaceFactory<filesystem::mojom::Directory>,
      public service_manager::InterfaceFactory<
          service_manager::mojom::Resolver> {
 public:
  // Constructs a catalog over a static manifest. This catalog never performs
  // file I/O. Note that either |static_manifest| or |service_manifest_provider|
  // may be null. If both are null, no service names will be resolved.
  explicit Catalog(std::unique_ptr<base::Value> static_manifest,
                   ManifestProvider* service_manifest_provider = nullptr);

  ~Catalog() override;

  service_manager::mojom::ServicePtr TakeService();

  // Allows an embedder to override the default static manifest contents for
  // Catalog instances which are constructed with a null static manifest.
  static void SetDefaultCatalogManifest(
      std::unique_ptr<base::Value> static_manifest);

  // Loads a default catalog manifest from the given FilePath. |path| is taken
  // to be relative to the current executable's path.
  static void LoadDefaultCatalogManifest(const base::FilePath& path);

 private:
  class ServiceImpl;

  // service_manager::InterfaceFactory<service_manager::mojom::Resolver>:
  void Create(const service_manager::Identity& remote_identity,
              service_manager::mojom::ResolverRequest request) override;

  // service_manager::InterfaceFactory<mojom::Catalog>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::CatalogRequest request) override;

  // service_manager::InterfaceFactory<filesystem::mojom::Directory>:
  void Create(const service_manager::Identity& remote_identity,
              filesystem::mojom::DirectoryRequest request) override;

  Instance* GetInstanceForUserId(const std::string& user_id);

  service_manager::mojom::ServicePtr service_;
  std::unique_ptr<service_manager::ServiceContext> service_context_;
  ManifestProvider* service_manifest_provider_;
  EntryCache system_cache_;
  std::map<std::string, std::unique_ptr<Instance>> instances_;

  scoped_refptr<filesystem::LockTable> lock_table_;

  base::WeakPtrFactory<Catalog> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Catalog);
};

}  // namespace catalog

#endif  // SERVICES_CATALOG_CATALOG_H_
