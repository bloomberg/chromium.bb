// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/catalog/catalog.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "services/catalog/constants.h"
#include "services/catalog/instance.h"
#include "services/catalog/reader.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/shell_connection.h"

namespace catalog {

Catalog::Catalog(base::TaskRunner* file_task_runner,
                 std::unique_ptr<Store> store,
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
  system_reader_->Read(system_package_dir, &system_cache_,
                       base::Bind(&Catalog::SystemPackageDirScanned,
                                  weak_factory_.GetWeakPtr()));
}

Catalog::~Catalog() {}

shell::mojom::ShellClientPtr Catalog::TakeShellClient() {
  return std::move(shell_client_);
}

bool Catalog::AcceptConnection(shell::Connection* connection) {
  connection->AddInterface<mojom::Catalog>(this);
  connection->AddInterface<shell::mojom::ShellResolver>(this);
  return true;
}

void Catalog::Create(shell::Connection* connection,
                     shell::mojom::ShellResolverRequest request) {
  Instance* instance =
      GetInstanceForUserId(connection->GetRemoteIdentity().user_id());
  instance->BindShellResolver(std::move(request));
}

void Catalog::Create(shell::Connection* connection,
                     mojom::CatalogRequest request) {
  Instance* instance =
      GetInstanceForUserId(connection->GetRemoteIdentity().user_id());
  instance->BindCatalog(std::move(request));
}

Instance* Catalog::GetInstanceForUserId(const std::string& user_id) {
  auto it = instances_.find(user_id);
  if (it != instances_.end())
    return it->second.get();

  // TODO(beng): There needs to be a way to load the store from different users.
  Instance* instance = new Instance(std::move(store_), system_reader_.get());
  instances_[user_id] = base::WrapUnique(instance);
  if (loaded_)
    instance->CacheReady(&system_cache_);

  return instance;
}

void Catalog::SystemPackageDirScanned() {
  loaded_ = true;
  for (auto& instance : instances_)
    instance.second->CacheReady(&system_cache_);
}

}  // namespace catalog
