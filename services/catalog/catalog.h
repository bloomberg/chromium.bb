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
#include "mojo/public/cpp/bindings/binding.h"
#include "services/catalog/public/interfaces/catalog.mojom.h"
#include "services/catalog/types.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/public/interfaces/shell_client.mojom.h"
#include "services/shell/public/interfaces/shell_resolver.mojom.h"

namespace base {
class TaskRunner;
}

namespace shell {
class ShellConnection;
}

namespace catalog {

class Instance;
class ManifestProvider;
class Reader;
class Store;

// Creates and owns an instance of the catalog. Exposes a ShellClientPtr that
// can be passed to the Shell, potentially in a different process.
class Catalog : public shell::ShellClient,
                public shell::InterfaceFactory<mojom::Catalog>,
                public shell::InterfaceFactory<shell::mojom::ShellResolver> {
 public:
  // |manifest_provider| may be null.
  Catalog(base::TaskRunner* file_task_runner,
          std::unique_ptr<Store> store,
          ManifestProvider* manifest_provider);
  ~Catalog() override;

  shell::mojom::ShellClientPtr TakeShellClient();

 private:
  // shell::ShellClient:
  bool AcceptConnection(shell::Connection* connection) override;

  // shell::InterfaceFactory<shell::mojom::ShellResolver>:
  void Create(shell::Connection* connection,
              shell::mojom::ShellResolverRequest request) override;

  // shell::InterfaceFactory<mojom::Catalog>:
  void Create(shell::Connection* connection,
              mojom::CatalogRequest request) override;

  Instance* GetInstanceForUserId(const std::string& user_id);

  void SystemPackageDirScanned();

  base::TaskRunner* const file_task_runner_;
  std::unique_ptr<Store> store_;

  shell::mojom::ShellClientPtr shell_client_;
  std::unique_ptr<shell::ShellConnection> shell_connection_;

  std::map<std::string, std::unique_ptr<Instance>> instances_;

  std::unique_ptr<Reader> system_reader_;
  EntryCache system_cache_;
  bool loaded_ = false;

  base::WeakPtrFactory<Catalog> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Catalog);
};

}  // namespace catalog

#endif  // SERVICES_CATALOG_CATALOG_H_
