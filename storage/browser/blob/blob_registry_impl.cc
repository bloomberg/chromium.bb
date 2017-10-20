// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_registry_impl.h"

#include "base/barrier_closure.h"
#include "base/callback_helpers.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_transport_strategy.h"

namespace storage {

namespace {

using MemoryStrategy = BlobMemoryController::Strategy;

bool CalculateBlobMemorySize(
    const std::vector<blink::mojom::DataElementPtr>& elements,
    size_t* shortcut_bytes,
    uint64_t* total_bytes) {
  DCHECK(shortcut_bytes);
  DCHECK(total_bytes);

  base::CheckedNumeric<uint64_t> total_size_checked = 0;
  base::CheckedNumeric<size_t> shortcut_size_checked = 0;
  for (const auto& e : elements) {
    if (e->is_bytes()) {
      const auto& bytes = e->get_bytes();
      total_size_checked += bytes->length;
      if (bytes->embedded_data) {
        if (bytes->embedded_data->size() != bytes->length)
          return false;
        shortcut_size_checked += bytes->length;
      }
    } else {
      continue;
    }
    if (!total_size_checked.IsValid() || !shortcut_size_checked.IsValid())
      return false;
  }
  *shortcut_bytes = shortcut_size_checked.ValueOrDie();
  *total_bytes = total_size_checked.ValueOrDie();
  return true;
}

class BlobURLHandleImpl final : public blink::mojom::BlobURLHandle {
 public:
  static blink::mojom::BlobURLHandlePtr Create(
      base::WeakPtr<BlobStorageContext> context,
      const GURL& url) {
    blink::mojom::BlobURLHandlePtr ptr;
    mojo::MakeStrongBinding(
        base::WrapUnique(new BlobURLHandleImpl(context, url)),
        mojo::MakeRequest(&ptr));
    return ptr;
  }

  ~BlobURLHandleImpl() override {
    if (context_)
      context_->RevokePublicBlobURL(url_);
  }

 private:
  BlobURLHandleImpl(base::WeakPtr<BlobStorageContext> context, const GURL& url)
      : context_(std::move(context)), url_(url) {}

  base::WeakPtr<BlobStorageContext> context_;
  const GURL url_;
  DISALLOW_COPY_AND_ASSIGN(BlobURLHandleImpl);
};

}  // namespace

class BlobRegistryImpl::BlobUnderConstruction {
 public:
  BlobUnderConstruction(BlobRegistryImpl* blob_registry,
                        const std::string& uuid,
                        const std::string& content_type,
                        const std::string& content_disposition,
                        std::vector<blink::mojom::DataElementPtr> elements,
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
  void StartTransportation();

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
    DCHECK(BlobStatusIsError(reason));
    DCHECK_EQ(bad_message_reason.empty(), !BlobStatusIsBadIPC(reason));
    // The blob might no longer have any references, in which case it may no
    // longer exist. If that happens just skip calling cancel.
    if (context()->registry().HasEntry(uuid()))
      context()->CancelBuildingBlob(uuid(), reason);
    if (!bad_message_reason.empty())
      std::move(bad_message_callback_).Run(bad_message_reason);
    MarkAsFinishedAndDeleteSelf();
  }

  void MarkAsFinishedAndDeleteSelf() {
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

  // Called when memory has been reserved for this blob and transport can begin.
  // Could also be called if something caused the blob to become invalid before
  // transportation began, in which case we just give up.
  void OnReadyForTransport(
      BlobStatus status,
      std::vector<BlobMemoryController::FileCreationInfo> file_infos);

  // Called when all data has been transported, or transport has failed.
  void TransportComplete(BlobStatus result);

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
  std::vector<blink::mojom::DataElementPtr> elements_;

  // Callback to report a BadMessage on the binding on which Register was
  // called.
  mojo::ReportBadMessageCallback bad_message_callback_;

  // Transport strategy to use when transporting data.
  std::unique_ptr<BlobTransportStrategy> transport_strategy_;

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

void BlobRegistryImpl::BlobUnderConstruction::StartTransportation() {
  size_t blob_count = 0;
  for (size_t i = 0; i < elements_.size(); ++i) {
    const auto& element = elements_[i];
    if (element->is_blob()) {
      // If connection to blob is broken, something bad happened, so mark this
      // new blob as broken, which will delete |this| and keep it from doing
      // unneeded extra work.
      element->get_blob()->blob.set_connection_error_handler(base::BindOnce(
          &BlobUnderConstruction::MarkAsBroken, weak_ptr_factory_.GetWeakPtr(),
          BlobStatus::ERR_REFERENCED_BLOB_BROKEN, ""));

      element->get_blob()->blob->GetInternalUUID(
          base::BindOnce(&BlobUnderConstruction::ReceivedBlobUUID,
                         weak_ptr_factory_.GetWeakPtr(), blob_count++));
    } else if (element->is_bytes()) {
      element->get_bytes()->data.set_connection_error_handler(base::BindOnce(
          &BlobUnderConstruction::MarkAsBroken, weak_ptr_factory_.GetWeakPtr(),
          BlobStatus::ERR_SOURCE_DIED_IN_TRANSIT, ""));
    }
  }
  referenced_blob_uuids_.resize(blob_count);

  // TODO(mek): Do we need some kind of timeout for fetching the UUIDs?
  // Without it a blob could forever remaing pending if a renderer sends us
  // a BlobPtr connected to a (malicious) non-responding implementation.

  // Do some basic validation of bytes to transport, and determine memory
  // transport strategy to use later.
  uint64_t transport_memory_size = 0;
  size_t shortcut_size = 0;
  if (!CalculateBlobMemorySize(elements_, &shortcut_size,
                               &transport_memory_size)) {
    MarkAsBroken(BlobStatus::ERR_INVALID_CONSTRUCTION_ARGUMENTS,
                 "Invalid byte element sizes in BlobRegistry::Register");
    return;
  }

  const BlobMemoryController& memory_controller =
      context()->memory_controller();
  MemoryStrategy memory_strategy =
      memory_controller.DetermineStrategy(shortcut_size, transport_memory_size);
  if (memory_strategy == MemoryStrategy::TOO_LARGE) {
    MarkAsBroken(BlobStatus::ERR_OUT_OF_MEMORY);
    return;
  }

  transport_strategy_ = BlobTransportStrategy::Create(
      memory_strategy, &builder_,
      base::BindOnce(&BlobUnderConstruction::TransportComplete,
                     weak_ptr_factory_.GetWeakPtr()),
      memory_controller.limits());

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

  auto blob_uuid_it = referenced_blob_uuids_.begin();
  for (const auto& element : elements_) {
    if (element->is_bytes()) {
      transport_strategy_->AddBytesElement(element->get_bytes().get());
    } else if (element->is_file()) {
      const auto& f = element->get_file();
      builder_.AppendFile(f->path, f->offset, f->length,
                          f->expected_modification_time.value_or(base::Time()));
    } else if (element->is_file_filesystem()) {
      const auto& f = element->get_file_filesystem();
      builder_.AppendFileSystemFile(
          f->url, f->offset, f->length,
          f->expected_modification_time.value_or(base::Time()));
    } else if (element->is_blob()) {
      DCHECK(blob_uuid_it != referenced_blob_uuids_.end());
      const std::string& blob_uuid = *blob_uuid_it++;
      builder_.AppendBlob(blob_uuid, element->get_blob()->offset,
                          element->get_blob()->length);
    }
  }

  BlobStorageContext::TransportAllowedCallback callback =
      base::Bind(&BlobUnderConstruction::OnReadyForTransport,
                 weak_ptr_factory_.GetWeakPtr());

  // OnReadyForTransport can be called synchronously, which can call
  // MarkAsFinishedAndDeleteSelf synchronously, so don't access any members
  // after this call.
  std::unique_ptr<BlobDataHandle> new_handle =
      context()->BuildPreregisteredBlob(builder_, callback);

  // TODO(mek): Update BlobImpl with new BlobDataHandle. Although handles
  // only differ in their size() attribute, which is currently not used by
  // BlobImpl.

  // BuildPreregisteredBlob might or might not have called the callback if it
  // finished synchronously, so call the callback directly. If it was already
  // called |this| would have been deleted making calling the callback a no-op.
  if (!new_handle->IsBeingBuilt()) {
    callback.Run(new_handle->GetBlobStatus(),
                 std::vector<BlobMemoryController::FileCreationInfo>());
  }
}

void BlobRegistryImpl::BlobUnderConstruction::OnReadyForTransport(
    BlobStatus status,
    std::vector<BlobMemoryController::FileCreationInfo> file_infos) {
  if (!BlobStatusIsPending(status)) {
    // Done or error.
    MarkAsFinishedAndDeleteSelf();
    return;
  }
  transport_strategy_->BeginTransport(std::move(file_infos));
}

void BlobRegistryImpl::BlobUnderConstruction::TransportComplete(
    BlobStatus result) {
  // The blob might no longer have any references, in which case it may no
  // longer exist. If that happens just skip calling Complete.
  // TODO(mek): Stop building sooner if a blob is no longer referenced.
  if (context()->registry().HasEntry(uuid())) {
    if (result == BlobStatus::DONE)
      context()->NotifyTransportComplete(uuid());
    else
      context()->CancelBuildingBlob(uuid(), result);
  }
  if (BlobStatusIsBadIPC(result)) {
    // BlobTransportStrategy might have already reported a BadMessage on the
    // BytesProvider binding, but just to be safe, also report one on the
    // BlobRegistry binding itself.
    std::move(bad_message_callback_)
        .Run("Received invalid data while transporting blob");
  }
  MarkAsFinishedAndDeleteSelf();
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

BlobRegistryImpl::BlobRegistryImpl(
    BlobStorageContext* context,
    scoped_refptr<FileSystemContext> file_system_context)
    : context_(context),
      file_system_context_(std::move(file_system_context)),
      weak_ptr_factory_(this) {}

BlobRegistryImpl::~BlobRegistryImpl() {}

void BlobRegistryImpl::Bind(blink::mojom::BlobRegistryRequest request,
                            std::unique_ptr<Delegate> delegate) {
  DCHECK(delegate);
  bindings_.AddBinding(this, std::move(request), std::move(delegate));
}

void BlobRegistryImpl::Register(
    blink::mojom::BlobRequest blob,
    const std::string& uuid,
    const std::string& content_type,
    const std::string& content_disposition,
    std::vector<blink::mojom::DataElementPtr> elements,
    RegisterCallback callback) {
  if (uuid.empty() || context_->registry().HasEntry(uuid) ||
      base::ContainsKey(blobs_under_construction_, uuid)) {
    bindings_.ReportBadMessage("Invalid UUID passed to BlobRegistry::Register");
    return;
  }

  Delegate* delegate = bindings_.dispatch_context().get();
  DCHECK(delegate);
  for (const auto& element : elements) {
    if (element->is_file()) {
      if (!delegate->CanReadFile(element->get_file()->path)) {
        // TODO(mek) Using ERR_FILE_WRITE_FAILED matches the IPC based
        // implementation, but this is really a very different error than what
        // FILE_WRITE_FAILED is used for elsewhere, so this should probably be a
        // dfferent error status. http://crbug.com/740730
        std::unique_ptr<BlobDataHandle> handle =
            context_->AddBrokenBlob(uuid, content_type, content_disposition,
                                    BlobStatus::ERR_FILE_WRITE_FAILED);
        BlobImpl::Create(std::move(handle), std::move(blob));
        std::move(callback).Run();
        return;
      }
    } else if (element->is_file_filesystem()) {
      FileSystemURL filesystem_url(
          file_system_context_->CrackURL(element->get_file_filesystem()->url));
      if (!filesystem_url.is_valid() ||
          !file_system_context_->GetFileSystemBackend(filesystem_url.type()) ||
          !delegate->CanReadFileSystemFile(filesystem_url)) {
        // TODO(mek) Using ERR_FILE_WRITE_FAILED matches the IPC based
        // implementation, but this is really a very different error than what
        // FILE_WRITE_FAILED is used for elsewhere, so this should probably be a
        // dfferent error status. http://crbug.com/740730
        std::unique_ptr<BlobDataHandle> handle =
            context_->AddBrokenBlob(uuid, content_type, content_disposition,
                                    BlobStatus::ERR_FILE_WRITE_FAILED);
        BlobImpl::Create(std::move(handle), std::move(blob));
        std::move(callback).Run();
        return;
      }
    }
  }

  blobs_under_construction_[uuid] = base::MakeUnique<BlobUnderConstruction>(
      this, uuid, content_type, content_disposition, std::move(elements),
      bindings_.GetBadMessageCallback());

  std::unique_ptr<BlobDataHandle> handle =
      context_->AddFutureBlob(uuid, content_type, content_disposition);
  BlobImpl::Create(std::move(handle), std::move(blob));

  blobs_under_construction_[uuid]->StartTransportation();

  std::move(callback).Run();
}

void BlobRegistryImpl::GetBlobFromUUID(blink::mojom::BlobRequest blob,
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

void BlobRegistryImpl::RegisterURL(blink::mojom::BlobPtr blob,
                                   const GURL& url,
                                   RegisterURLCallback callback) {
  Delegate* delegate = bindings_.dispatch_context().get();
  DCHECK(delegate);
  if (!url.SchemeIsBlob() || !delegate->CanCommitURL(url)) {
    bindings_.ReportBadMessage(
        "Invalid Blob URL passed to BlobRegistry::RegisterURL");
    return;
  }

  blink::mojom::Blob* blob_ptr = blob.get();
  blob_ptr->GetInternalUUID(base::BindOnce(
      &BlobRegistryImpl::RegisterURLWithUUID, weak_ptr_factory_.GetWeakPtr(),
      url, std::move(blob), std::move(callback)));
}

void BlobRegistryImpl::RegisterURLWithUUID(const GURL& url,
                                           blink::mojom::BlobPtr blob,
                                           RegisterURLCallback callback,
                                           const std::string& uuid) {
  // |blob| is unused, but is passed here to be kept alive until
  // RegisterBlobURL increments the refcount of it via the uuid.
  context_->RegisterPublicBlobURL(url, uuid);
  std::move(callback).Run(
      BlobURLHandleImpl::Create(context_->AsWeakPtr(), url));
}

}  // namespace storage
