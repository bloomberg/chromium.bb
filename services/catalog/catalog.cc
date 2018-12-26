// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/catalog/catalog.h"

#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "components/services/filesystem/directory_impl.h"
#include "components/services/filesystem/lock_table.h"
#include "components/services/filesystem/public/interfaces/types.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/catalog/constants.h"
#include "services/catalog/entry_cache.h"
#include "services/catalog/instance.h"

namespace catalog {

namespace {

std::vector<service_manager::Manifest>& GetDefaultManifests() {
  static base::NoDestructor<std::vector<service_manager::Manifest>> manifests;
  return *manifests;
}

}  // namespace

// Wraps state needed for servicing directory requests on a separate thread.
// filesystem::LockTable is not thread safe, so it's wrapped in
// DirectoryThreadState.
class Catalog::DirectoryThreadState
    : public base::RefCountedDeleteOnSequence<DirectoryThreadState> {
 public:
  explicit DirectoryThreadState(
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : base::RefCountedDeleteOnSequence<DirectoryThreadState>(
            std::move(task_runner)) {}

  scoped_refptr<filesystem::LockTable> lock_table() {
    if (!lock_table_)
      lock_table_ = new filesystem::LockTable;
    return lock_table_;
  }

 private:
  friend class base::DeleteHelper<DirectoryThreadState>;
  friend class base::RefCountedDeleteOnSequence<DirectoryThreadState>;

  ~DirectoryThreadState() = default;

  scoped_refptr<filesystem::LockTable> lock_table_;

  DISALLOW_COPY_AND_ASSIGN(DirectoryThreadState);
};

Catalog::Catalog(const std::vector<service_manager::Manifest>& manifests) {
  if (!manifests.empty()) {
    for (const auto& manifest : manifests)
      system_cache_.AddRootEntryFromManifest(manifest);
  } else {
    for (const auto& manifest : GetDefaultManifests())
      system_cache_.AddRootEntryFromManifest(manifest);
  }

  registry_.AddInterface<mojom::Catalog>(base::BindRepeating(
      &Catalog::BindCatalogRequest, base::Unretained(this)));
  registry_.AddInterface<filesystem::mojom::Directory>(base::BindRepeating(
      &Catalog::BindDirectoryRequest, base::Unretained(this)));
}

Catalog::~Catalog() = default;

void Catalog::BindServiceRequest(
    service_manager::mojom::ServiceRequest request) {
  service_binding_.Bind(std::move(request));
}

// static
void Catalog::SetDefaultCatalogManifest(
    const std::vector<service_manager::Manifest>& manifests) {
  GetDefaultManifests() = manifests;
}

Instance* Catalog::GetInstanceForGroup(const base::Token& instance_group) {
  auto it = instances_.find(instance_group);
  if (it != instances_.end())
    return it->second.get();

  auto result = instances_.emplace(instance_group,
                                   std::make_unique<Instance>(&system_cache_));
  return result.first->second.get();
}

void Catalog::BindCatalogRequest(
    mojom::CatalogRequest request,
    const service_manager::BindSourceInfo& source_info) {
  Instance* instance =
      GetInstanceForGroup(source_info.identity.instance_group());
  instance->BindCatalog(std::move(request));
}

void Catalog::BindDirectoryRequest(
    filesystem::mojom::DirectoryRequest request,
    const service_manager::BindSourceInfo& source_info) {
  if (!directory_task_runner_) {
    directory_task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
        {base::MayBlock(),
         // Use USER_BLOCKING as this gates showing UI during startup.
         base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
    directory_thread_state_ = new DirectoryThreadState(directory_task_runner_);
  }
  directory_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Catalog::BindDirectoryRequestOnBackgroundThread,
                     directory_thread_state_, std::move(request), source_info));
}

// static
void Catalog::BindDirectoryRequestOnBackgroundThread(
    scoped_refptr<DirectoryThreadState> thread_state,
    filesystem::mojom::DirectoryRequest request,
    const service_manager::BindSourceInfo& source_info) {
  base::FilePath resources_path;
  base::PathService::Get(base::DIR_MODULE, &resources_path);
  mojo::MakeStrongBinding(
      std::make_unique<filesystem::DirectoryImpl>(
          resources_path, scoped_refptr<filesystem::SharedTempDir>(),
          thread_state->lock_table()),
      std::move(request));
}

void Catalog::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe),
                          source_info);
}

}  // namespace catalog
