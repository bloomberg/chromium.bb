// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_registry_impl.h"

#include "base/barrier_closure.h"
#include "base/callback_helpers.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace storage {

class BlobRegistryImpl::BlobUnderConstruction {
 public:
  BlobUnderConstruction(BlobRegistryImpl* blob_registry,
                        const std::string& uuid,
                        const std::string& content_type,
                        const std::string& content_disposition,
                        std::vector<mojom::DataElementPtr> elements,
                        mojo::ReportBadMessageCallback bad_message_callback)
      : blob_registry_(blob_registry),
        builder_(uuid),
        elements_(std::move(elements)),
        bad_message_callback_(std::move(bad_message_callback)),
        weak_ptr_factory_(this) {
    builder_.set_content_type(content_type);
    builder_.set_content_disposition(content_disposition);
  }

  // Call this after constructing to kick of fetching of UUIDs of blobs
  // referenced by this new blob. This (and any further methods) could end up
  // deleting |this| by removing it from the blobs_under_construction_
  // collection in the blob service.
  void StartFetchingBlobUUIDs();

  ~BlobUnderConstruction() {}

  const std::string& uuid() const { return builder_.uuid(); }

 private:
  BlobStorageContext* context() const { return blob_registry_->context_; }

  // Marks this blob as broken. If an optional |bad_message_reason| is provided,
  // this will also report a BadMessage on the binding over which the initial
  // Register request was received.
  // Also deletes |this| by removing it from the blobs_under_construction_ list.
  void MarkAsBroken(BlobStatus reason,
                    const std::string& bad_message_reason = "") {
    context()->CancelBuildingBlob(uuid(), reason);
    if (!bad_message_reason.empty())
      std::move(bad_message_callback_).Run(bad_message_reason);
    blob_registry_->blobs_under_construction_.erase(uuid());
  }

  // Called when the UUID of a referenced blob is received.
  void ReceivedBlobUUID(size_t blob_index, const std::string& uuid);

  // Called by either StartFetchingBlobUUIDs or ReceivedBlobUUID when all the
  // UUIDs of referenced blobs have been resolved. Starts checking for circular
  // references. Before we can proceed with actually building the blob, all
  // referenced blobs also need to have resolved their referenced blobs (to
  // always be able to calculate the size of the newly built blob). To ensure
  // this we might have to wait for one or more possibly indirectly dependent
  // blobs to also have resolved the UUIDs of their dependencies. This waiting
  // is kicked of by this method.
  void ResolvedAllBlobUUIDs();

  void DependentBlobReady(BlobStatus status);

  // Called when all blob dependencies have been resolved, and we're sure there
  // are no circular dependencies. This finally kicks of the actually building
  // of the blob, and figures out how to transport any bytes that might need
  // transporting.
  void ResolvedAllBlobDependencies();

#if DCHECK_IS_ON()
  // Returns true if the DAG made up by this blob and any other blobs that
  // are currently being built by BlobRegistryImpl contains any cycles.
  // |path_from_root| should contain all the nodes that have been visited so
  // far on a path from whatever node we started our search from.
  bool ContainsCycles(
      std::unordered_set<BlobUnderConstruction*>* path_from_root);
#endif

  // BlobRegistryImpl we belong to.
  BlobRegistryImpl* blob_registry_;

  // BlobDataBuilder for the blob under construction. Is created in the
  // constructor, but not filled until all referenced blob UUIDs have been
  // resolved.
  BlobDataBuilder builder_;

  // Elements as passed in to Register.
  std::vector<mojom::DataElementPtr> elements_;

  // Callback to report a BadMessage on the binding on which Register was
  // called.
  mojo::ReportBadMessageCallback bad_message_callback_;

  // List of UUIDs for referenced blobs. Same size as |elements_|. All entries
  // for non-blob elements will remain empty strings.
  std::vector<std::string> referenced_blob_uuids_;

  // Number of blob UUIDs that have been resolved.
  size_t resolved_blob_uuid_count_ = 0;

  // Number of dependent blobs that have started constructing.
  size_t ready_dependent_blob_count_ = 0;

  base::WeakPtrFactory<BlobUnderConstruction> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(BlobUnderConstruction);
};

void BlobRegistryImpl::BlobUnderConstruction::StartFetchingBlobUUIDs() {
  size_t blob_count = 0;
  for (size_t i = 0; i < elements_.size(); ++i) {
    const auto& element = elements_[i];
    if (element->is_blob()) {
      if (element->get_blob()->blob.encountered_error()) {
        // Will delete |this|.
        MarkAsBroken(BlobStatus::ERR_REFERENCED_BLOB_BROKEN);
        return;
      }

      // If connection to blob is broken, something bad happened, so mark this
      // new blob as broken, which will delete |this| and keep it from doing
      // unneeded extra work.
      element->get_blob()->blob.set_connection_error_handler(base::BindOnce(
          &BlobUnderConstruction::MarkAsBroken, weak_ptr_factory_.GetWeakPtr(),
          BlobStatus::ERR_REFERENCED_BLOB_BROKEN, ""));

      element->get_blob()->blob->GetInternalUUID(
          base::BindOnce(&BlobUnderConstruction::ReceivedBlobUUID,
                         weak_ptr_factory_.GetWeakPtr(), blob_count++));
    }
  }
  referenced_blob_uuids_.resize(blob_count);

  // TODO(mek): Do we need some kind of timeout for fetching the UUIDs?
  // Without it a blob could forever remaing pending if a renderer sends us
  // a BlobPtr connected to a (malicious) non-responding implementation.

  // If there were no unresolved blobs, immediately proceed to the next step.
  // Currently this will only happen if there are no blobs referenced
  // whatsoever, but hopefully in the future blob UUIDs will be cached in the
  // message pipe handle, making things much more efficient in the common case.
  if (resolved_blob_uuid_count_ == referenced_blob_uuids_.size())
    ResolvedAllBlobUUIDs();
}

void BlobRegistryImpl::BlobUnderConstruction::ReceivedBlobUUID(
    size_t blob_index,
    const std::string& uuid) {
  DCHECK(referenced_blob_uuids_[blob_index].empty());
  DCHECK_LT(resolved_blob_uuid_count_, referenced_blob_uuids_.size());

  referenced_blob_uuids_[blob_index] = uuid;
  if (++resolved_blob_uuid_count_ == referenced_blob_uuids_.size())
    ResolvedAllBlobUUIDs();
}

void BlobRegistryImpl::BlobUnderConstruction::ResolvedAllBlobUUIDs() {
  DCHECK_EQ(resolved_blob_uuid_count_, referenced_blob_uuids_.size());

#if DCHECK_IS_ON()
  std::unordered_set<BlobUnderConstruction*> visited;
  if (ContainsCycles(&visited)) {
    MarkAsBroken(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
                 "Cycles in blob references in BlobRegistry::Register");
    return;
  }
#endif

  if (referenced_blob_uuids_.size() == 0) {
    ResolvedAllBlobDependencies();
    return;
  }

  for (const std::string& blob_uuid : referenced_blob_uuids_) {
    if (blob_uuid.empty() || blob_uuid == uuid() ||
        !context()->registry().HasEntry(blob_uuid)) {
      // Will delete |this|.
      MarkAsBroken(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
                   "Bad blob references in BlobRegistry::Register");
      return;
    }

    std::unique_ptr<BlobDataHandle> handle =
        context()->GetBlobDataFromUUID(blob_uuid);
    handle->RunOnConstructionBegin(
        base::Bind(&BlobUnderConstruction::DependentBlobReady,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void BlobRegistryImpl::BlobUnderConstruction::DependentBlobReady(
    BlobStatus status) {
  if (++ready_dependent_blob_count_ == referenced_blob_uuids_.size()) {
    // Asynchronously call ResolvedAllBlobDependencies, as otherwise |this|
    // might end up getting deleted while ResolvedAllBlobUUIDs is still
    // iterating over |referenced_blob_uuids_|.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&BlobUnderConstruction::ResolvedAllBlobDependencies,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void BlobRegistryImpl::BlobUnderConstruction::ResolvedAllBlobDependencies() {
  DCHECK_EQ(resolved_blob_uuid_count_, referenced_blob_uuids_.size());
  DCHECK_EQ(ready_dependent_blob_count_, referenced_blob_uuids_.size());

  // TODO(mek): Fill BlobDataBuilder with elements_ other than blobs.
  auto blob_uuid_it = referenced_blob_uuids_.begin();
  for (const auto& element : elements_) {
    if (element->is_blob()) {
      DCHECK(blob_uuid_it != referenced_blob_uuids_.end());
      const std::string& blob_uuid = *blob_uuid_it++;
      builder_.AppendBlob(blob_uuid, element->get_blob()->offset,
                          element->get_blob()->length);
    }
  }
  std::unique_ptr<BlobDataHandle> new_handle =
      context()->BuildPreregisteredBlob(
          builder_, BlobStorageContext::TransportAllowedCallback());

  // TODO(mek): Update BlobImpl with new BlobDataHandle. Although handles
  // only differ in their size() attribute, which is currently not used by
  // BlobImpl.
  DCHECK(!BlobStatusIsPending(new_handle->GetBlobStatus()));
}

#if DCHECK_IS_ON()
bool BlobRegistryImpl::BlobUnderConstruction::ContainsCycles(
    std::unordered_set<BlobUnderConstruction*>* path_from_root) {
  if (!path_from_root->insert(this).second)
    return true;
  for (const std::string& blob_uuid : referenced_blob_uuids_) {
    if (blob_uuid.empty())
      continue;
    auto it = blob_registry_->blobs_under_construction_.find(blob_uuid);
    if (it == blob_registry_->blobs_under_construction_.end())
      continue;
    if (it->second->ContainsCycles(path_from_root))
      return true;
  }
  path_from_root->erase(this);
  return false;
}
#endif

BlobRegistryImpl::BlobRegistryImpl(BlobStorageContext* context)
    : context_(context), weak_ptr_factory_(this) {}

BlobRegistryImpl::~BlobRegistryImpl() {}

void BlobRegistryImpl::Bind(mojom::BlobRegistryRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void BlobRegistryImpl::Register(mojom::BlobRequest blob,
                                const std::string& uuid,
                                const std::string& content_type,
                                const std::string& content_disposition,
                                std::vector<mojom::DataElementPtr> elements,
                                RegisterCallback callback) {
  if (uuid.empty() || context_->registry().HasEntry(uuid) ||
      base::ContainsKey(blobs_under_construction_, uuid)) {
    bindings_.ReportBadMessage("Invalid UUID passed to BlobRegistry::Register");
    return;
  }

  // TODO(mek): Security policy checks for files and filesystem items.

  blobs_under_construction_[uuid] = base::MakeUnique<BlobUnderConstruction>(
      this, uuid, content_type, content_disposition, std::move(elements),
      bindings_.GetBadMessageCallback());

  std::unique_ptr<BlobDataHandle> handle =
      context_->AddFutureBlob(uuid, content_type, content_disposition);
  BlobImpl::Create(std::move(handle), std::move(blob));

  blobs_under_construction_[uuid]->StartFetchingBlobUUIDs();

  std::move(callback).Run();
}

void BlobRegistryImpl::GetBlobFromUUID(mojom::BlobRequest blob,
                                       const std::string& uuid) {
  if (uuid.empty()) {
    bindings_.ReportBadMessage(
        "Invalid UUID passed to BlobRegistry::GetBlobFromUUID");
    return;
  }
  if (!context_->registry().HasEntry(uuid)) {
    // TODO(mek): Log histogram, old code logs Storage.Blob.InvalidReference
    return;
  }
  BlobImpl::Create(context_->GetBlobDataFromUUID(uuid), std::move(blob));
}

}  // namespace storage
