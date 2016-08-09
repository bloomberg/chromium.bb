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
#include "services/catalog/public/interfaces/catalog.mojom.h"
#include "services/catalog/types.h"
#include "services/shell/public/cpp/service.h"
#include "services/shell/public/interfaces/resolver.mojom.h"
#include "services/shell/public/interfaces/service.mojom.h"

namespace base {
class SequencedWorkerPool;
class SingleThreadTaskRunner;
}

namespace filesystem {
class LockTable;
}

namespace shell {
class ServiceContext;
}

namespace catalog {

class Instance;
class ManifestProvider;
class Reader;
class Store;

// Creates and owns an instance of the catalog. Exposes a ServicePtr that
// can be passed to the Shell, potentially in a different process.
class Catalog : public shell::Service,
                public shell::InterfaceFactory<mojom::Catalog>,
                public shell::InterfaceFactory<filesystem::mojom::Directory>,
                public shell::InterfaceFactory<shell::mojom::Resolver> {
 public:
  // |manifest_provider| may be null.
  Catalog(base::SequencedWorkerPool* worker_pool,
          std::unique_ptr<Store> store,
          ManifestProvider* manifest_provider);
  Catalog(base::SingleThreadTaskRunner* task_runner,
          std::unique_ptr<Store> store,
          ManifestProvider* manifest_provider);
  ~Catalog() override;

  shell::mojom::ServicePtr TakeService();

 private:
  explicit Catalog(std::unique_ptr<Store> store);

  // Starts a scane for system packages.
  void ScanSystemPackageDir();

  // shell::Service:
  bool OnConnect(const shell::Identity& remote_identity,
                 shell::InterfaceRegistry* registry) override;

  // shell::InterfaceFactory<shell::mojom::Resolver>:
  void Create(const shell::Identity& remote_identity,
              shell::mojom::ResolverRequest request) override;

  // shell::InterfaceFactory<mojom::Catalog>:
  void Create(const shell::Identity& remote_identity,
              mojom::CatalogRequest request) override;

  // shell::InterfaceFactory<filesystem::mojom::Directory>:
  void Create(const shell::Identity& remote_identity,
              filesystem::mojom::DirectoryRequest request) override;

  Instance* GetInstanceForUserId(const std::string& user_id);

  void SystemPackageDirScanned();

  std::unique_ptr<Store> store_;

  shell::mojom::ServicePtr service_;
  std::unique_ptr<shell::ServiceContext> shell_connection_;

  std::map<std::string, std::unique_ptr<Instance>> instances_;

  std::unique_ptr<Reader> system_reader_;
  EntryCache system_cache_;
  bool loaded_ = false;

  scoped_refptr<filesystem::LockTable> lock_table_;

  base::WeakPtrFactory<Catalog> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Catalog);
};

}  // namespace catalog

#endif  // SERVICES_CATALOG_CATALOG_H_
