// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CATALOG_CATALOG_H_
#define SERVICES_CATALOG_CATALOG_H_

#include <map>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/token.h"
#include "components/services/filesystem/public/interfaces/directory.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/catalog/public/mojom/catalog.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace base {
class SequencedTaskRunner;
}

namespace catalog {

// Creates and owns an instance of the catalog and hosts a ServiceBinding to
// bind interface requests targeting the Catalog service.
//
// TODO(https://crbug.com/904240): Move this into the Service Manager. It
// doesn't need to be its own service.
class COMPONENT_EXPORT(CATALOG) Catalog : public service_manager::Service,
                                          public mojom::Catalog {
 public:
  using Manifest = service_manager::Manifest;

  // Constructs a catalog over a set of Manifests to use for lookup.
  explicit Catalog(const std::vector<Manifest>& manifests);
  ~Catalog() override;

  void BindServiceRequest(service_manager::mojom::ServiceRequest request);

  // Returns manifest data for the service named by |service_name|. If no
  // service is known by that name, this returns null.
  const Manifest* GetManifest(const Manifest::ServiceName& service_name);

  // Returns manifest data for the parent of the service named by
  // |service_name|. If the named service has no parent (i.e. it's not packaged
  // within another service) then this returns null.
  const Manifest* GetParentManifest(const Manifest::ServiceName& service_name);

 private:
  class DirectoryThreadState;

  void BindCatalogRequest(mojom::CatalogRequest request);
  void BindDirectoryRequest(filesystem::mojom::DirectoryRequest request);

  static void BindDirectoryRequestOnBackgroundThread(
      scoped_refptr<DirectoryThreadState> thread_state,
      filesystem::mojom::DirectoryRequest request);

  // service_manager::Service:
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  // mojom::Catalog:
  void GetEntries(const base::Optional<std::vector<std::string>>& names,
                  GetEntriesCallback callback) override;
  void GetEntriesProvidingCapability(
      const std::string& capability,
      GetEntriesProvidingCapabilityCallback callback) override;

  service_manager::ServiceBinding service_binding_{this};
  service_manager::BinderRegistry registry_;

  // Bindings for consumers of the Catalog interface.
  mojo::BindingSet<mojom::Catalog> catalog_bindings_;

  // The set of all top-level manifests known to the Service Manager.
  const std::vector<Manifest> manifests_;

  // Maintains a mapping from service name to manifest for quick lookup of any
  // manifest regardless of whether it's packaged. The values in this map refer
  // to objects owned by |manifests_| above.
  const std::map<Manifest::ServiceName, const Manifest*> manifest_map_;

  // Maintains a mapping from service name to parent manifest for quick
  // reverse-lookup of packaged service relationships. The values in this map
  // refer to objects owned by |manifests_| above.
  const std::map<Manifest::ServiceName, const Manifest*> parent_manifest_map_;

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
