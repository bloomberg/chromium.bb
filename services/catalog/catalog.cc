// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/catalog/catalog.h"

#include "base/bind.h"
#include "services/catalog/constants.h"
#include "services/catalog/reader.h"
#include "services/catalog/resolver.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/shell_connection.h"

namespace catalog {

Catalog::Catalog(base::TaskRunner* file_task_runner,
                 scoped_ptr<Store> store,
                 ManifestProvider* manifest_provider)
    : file_task_runner_(file_task_runner),
      store_(std::move(store)),
      weak_factory_(this) {
  shell::mojom::ShellClientRequest request = GetProxy(&shell_client_);
  shell_connection_.reset(new shell::ShellConnection(this, std::move(request)));

  base::FilePath system_package_dir;
  PathService::Get(base::DIR_MODULE, &system_package_dir);
  system_package_dir = system_package_dir.AppendASCII(kMojoApplicationsDirName);
  system_reader_.reset(new Reader(file_task_runner, manifest_provider));
  system_reader_->Read(system_package_dir, &system_catalog_,
                       base::Bind(&Catalog::SystemPackageDirScanned,
                                  weak_factory_.GetWeakPtr()));
}

Catalog::~Catalog() {}

shell::mojom::ShellClientPtr Catalog::TakeShellClient() {
  return std::move(shell_client_);
}

bool Catalog::AcceptConnection(shell::Connection* connection) {
  connection->AddInterface<mojom::Catalog>(this);
  connection->AddInterface<mojom::Resolver>(this);
  connection->AddInterface<shell::mojom::ShellResolver>(this);
  return true;
}

void Catalog::Create(shell::Connection* connection,
                     mojom::ResolverRequest request) {
  Resolver* resolver =
      GetResolverForUserId(connection->GetRemoteIdentity().user_id());
  resolver->BindResolver(std::move(request));
}

void Catalog::Create(shell::Connection* connection,
                     shell::mojom::ShellResolverRequest request) {
  Resolver* resolver =
      GetResolverForUserId(connection->GetRemoteIdentity().user_id());
  resolver->BindShellResolver(std::move(request));
}

void Catalog::Create(shell::Connection* connection,
                     mojom::CatalogRequest request) {
  Resolver* resolver =
      GetResolverForUserId(connection->GetRemoteIdentity().user_id());
  resolver->BindCatalog(std::move(request));
}

Resolver* Catalog::GetResolverForUserId(const std::string& user_id) {
  auto it = resolvers_.find(user_id);
  if (it != resolvers_.end())
    return it->second.get();

  // TODO(beng): There needs to be a way to load the store from different users.
  Resolver* resolver = new Resolver(std::move(store_), system_reader_.get());
  resolvers_[user_id] = make_scoped_ptr(resolver);
  if (loaded_)
    resolver->CacheReady(&system_catalog_);

  return resolver;
}

void Catalog::SystemPackageDirScanned() {
  loaded_ = true;
  for (auto& resolver : resolvers_)
    resolver.second->CacheReady(&system_catalog_);
}

}  // namespace catalog
