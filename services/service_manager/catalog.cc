// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/catalog.h"

#include <string>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "components/services/filesystem/directory_impl.h"
#include "components/services/filesystem/lock_table.h"
#include "components/services/filesystem/public/interfaces/types.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace service_manager {

namespace {

std::map<Manifest::ServiceName, const Manifest*> CreateManifestMap(
    const std::vector<Manifest>& manifests) {
  std::map<Manifest::ServiceName, const Manifest*> map;
  for (const auto& manifest : manifests) {
    map[manifest.service_name] = &manifest;
    for (const auto& entry : CreateManifestMap(manifest.packaged_services))
      map[entry.first] = entry.second;
  }
  return map;
}

std::map<Manifest::ServiceName, const Manifest*> CreateParentManifestMap(
    const std::vector<Manifest>& manifests) {
  std::map<Manifest::ServiceName, const Manifest*> map;
  for (const auto& parent : manifests) {
    for (const auto& child : parent.packaged_services)
      map[child.service_name] = &parent;
    for (const auto& entry : CreateParentManifestMap(parent.packaged_services))
      map[entry.first] = entry.second;
  }
  return map;
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

Catalog::Catalog(const std::vector<Manifest>& manifests)
    : manifests_(manifests),
      manifest_map_(CreateManifestMap(manifests_)),
      parent_manifest_map_(CreateParentManifestMap(manifests_)) {
  registry_.AddInterface<catalog::mojom::Catalog>(base::BindRepeating(
      &Catalog::BindCatalogRequest, base::Unretained(this)));
  registry_.AddInterface<filesystem::mojom::Directory>(base::BindRepeating(
      &Catalog::BindDirectoryRequest, base::Unretained(this)));
}

Catalog::~Catalog() = default;

void Catalog::BindServiceRequest(mojom::ServiceRequest request) {
  service_binding_.Bind(std::move(request));
}

const Manifest* Catalog::GetManifest(
    const Manifest::ServiceName& service_name) {
  const auto it = manifest_map_.find(service_name);
  if (it == manifest_map_.end())
    return nullptr;
  return it->second;
}

const Manifest* Catalog::GetParentManifest(
    const Manifest::ServiceName& service_name) {
  auto it = parent_manifest_map_.find(service_name);
  if (it == parent_manifest_map_.end())
    return nullptr;

  return it->second;
}

void Catalog::BindCatalogRequest(catalog::mojom::CatalogRequest request) {
  catalog_bindings_.AddBinding(this, std::move(request));
}

void Catalog::BindDirectoryRequest(
    filesystem::mojom::DirectoryRequest request) {
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
                     directory_thread_state_, std::move(request)));
}

// static
void Catalog::BindDirectoryRequestOnBackgroundThread(
    scoped_refptr<DirectoryThreadState> thread_state,
    filesystem::mojom::DirectoryRequest request) {
  base::FilePath resources_path;
  base::PathService::Get(base::DIR_MODULE, &resources_path);
  mojo::MakeStrongBinding(
      std::make_unique<filesystem::DirectoryImpl>(
          resources_path, scoped_refptr<filesystem::SharedTempDir>(),
          thread_state->lock_table()),
      std::move(request));
}

void Catalog::OnBindInterface(const BindSourceInfo& source_info,
                              const std::string& interface_name,
                              mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void Catalog::GetEntries(const base::Optional<std::vector<std::string>>& names,
                         GetEntriesCallback callback) {
  std::vector<catalog::mojom::EntryPtr> entries;
  if (!names.has_value()) {
    for (const auto& entry : manifest_map_) {
      const auto* manifest = entry.second;
      entries.push_back(catalog::mojom::Entry::New(
          manifest->service_name, manifest->display_name.raw_string));
    }
  } else {
    for (const std::string& name : names.value()) {
      const auto* manifest = GetManifest(name);
      if (manifest) {
        entries.push_back(catalog::mojom::Entry::New(
            manifest->service_name, manifest->display_name.raw_string));
      }
    }
  }
  std::move(callback).Run(std::move(entries));
}

void Catalog::GetEntriesProvidingCapability(
    const std::string& capability,
    GetEntriesProvidingCapabilityCallback callback) {
  std::vector<catalog::mojom::EntryPtr> entries;
  for (const auto& entry : manifest_map_) {
    const auto* manifest = entry.second;
    if (manifest->exposed_capabilities.find(capability) !=
        manifest->exposed_capabilities.end()) {
      entries.push_back(catalog::mojom::Entry::New(
          manifest->service_name, manifest->display_name.raw_string));
    }
  }
  std::move(callback).Run(std::move(entries));
}

}  // namespace service_manager
