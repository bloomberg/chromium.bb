// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CATALOG_CATALOG_H_
#define SERVICES_CATALOG_CATALOG_H_

#include <map>
#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/token.h"
#include "components/services/filesystem/public/interfaces/directory.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/catalog/entry_cache.h"
#include "services/catalog/public/mojom/catalog.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
class Value;
}

namespace catalog {

class Instance;
class ManifestProvider;

// Creates and owns an instance of the catalog. Exposes a ServicePtr that
// can be passed to the service manager, potentially in a different process.
class COMPONENT_EXPORT(CATALOG) Catalog : public service_manager::Service {
 public:
  // Constructs a catalog over a static manifest. This catalog never performs
  // file I/O. Note that either |static_manifest| or |service_manifest_provider|
  // may be null. If both are null, no service names will be resolved.
  explicit Catalog(std::unique_ptr<base::Value> static_manifest,
                   ManifestProvider* service_manifest_provider = nullptr);
  ~Catalog() override;

  void BindServiceRequest(service_manager::mojom::ServiceRequest request);

  // Allows an embedder to override the default static manifest contents for
  // Catalog instances which are constructed with a null static manifest.
  static void SetDefaultCatalogManifest(
      std::unique_ptr<base::Value> static_manifest);

  // Loads a default catalog manifest from the given FilePath. |path| is taken
  // to be relative to the current executable's path.
  static void LoadDefaultCatalogManifest(const base::FilePath& path);

  Instance* GetInstanceForGroup(const base::Token& instance_group);

 private:
  class DirectoryThreadState;
  class ServiceImpl;

  void BindCatalogRequest(mojom::CatalogRequest request,
                          const service_manager::BindSourceInfo& source_info);

  void BindDirectoryRequest(filesystem::mojom::DirectoryRequest request,
                            const service_manager::BindSourceInfo& source_info);

  static void BindDirectoryRequestOnBackgroundThread(
      scoped_refptr<DirectoryThreadState> thread_state,
      filesystem::mojom::DirectoryRequest request,
      const service_manager::BindSourceInfo& source_info);

  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  service_manager::ServiceBinding service_binding_{this};
  service_manager::BinderRegistryWithArgs<
      const service_manager::BindSourceInfo&>
      registry_;

  ManifestProvider* service_manifest_provider_;
  EntryCache system_cache_;
  std::map<base::Token, std::unique_ptr<Instance>> instances_;

  // The TaskRunner used for directory requests. Directory requests run on a
  // separate thread as they run file io, which is not allowed on the thread the
  // service manager runs on. Additionally we shouldn't block the service
  // manager while doing file io.
  scoped_refptr<base::SequencedTaskRunner> directory_task_runner_;
  scoped_refptr<DirectoryThreadState> directory_thread_state_;

  DISALLOW_COPY_AND_ASSIGN(Catalog);
};

}  // namespace catalog

#endif  // SERVICES_CATALOG_CATALOG_H_
