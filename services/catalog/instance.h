// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CATALOG_INSTANCE_H_
#define SERVICES_CATALOG_INSTANCE_H_

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/catalog/entry.h"
#include "services/catalog/public/interfaces/catalog.mojom.h"
#include "services/catalog/store.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/interfaces/resolver.mojom.h"

namespace catalog {

class EntryCache;
class ManifestProvider;

class Instance : public service_manager::mojom::Resolver,
                 public mojom::Catalog {
 public:
  // Neither |system_cache| nor |service_manifest_provider| is owned.
  // |service_manifest_provider| may be null
  Instance(EntryCache* system_cache,
           ManifestProvider* service_manifest_provider);
  ~Instance() override;

  void BindResolver(service_manager::mojom::ResolverRequest request);
  void BindCatalog(mojom::CatalogRequest request);

 private:
  // service_manager::mojom::Resolver:
  void ResolveServiceName(const std::string& service_name,
                          const ResolveServiceNameCallback& callback) override;

  // mojom::Catalog:
  void GetEntries(const base::Optional<std::vector<std::string>>& names,
                  const GetEntriesCallback& callback) override;
  void GetEntriesProvidingCapability(
      const std::string& capability,
      const GetEntriesProvidingCapabilityCallback& callback) override;
  void GetEntriesConsumingMIMEType(
      const std::string& mime_type,
      const GetEntriesConsumingMIMETypeCallback& callback) override;
  void GetEntriesSupportingScheme(
      const std::string& scheme,
      const GetEntriesSupportingSchemeCallback& callback) override;

  mojo::BindingSet<service_manager::mojom::Resolver> resolver_bindings_;
  mojo::BindingSet<mojom::Catalog> catalog_bindings_;

  // A map of name -> Entry data structure for system-level packages (i.e. those
  // that are visible to all users).
  // TODO(beng): eventually add per-user applications.
  EntryCache* const system_cache_;

  // A runtime interface the embedder can use to provide dynamic manifest data
  // to be queried on-demand if something can't be found in |system_cache_|.
  ManifestProvider* const service_manifest_provider_;

  DISALLOW_COPY_AND_ASSIGN(Instance);
};

}  // namespace catalog

#endif  // SERVICES_CATALOG_INSTANCE_H_
