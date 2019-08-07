// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_script_cached_metadata_handler.h"

#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"

namespace blink {

ServiceWorkerScriptCachedMetadataHandler::
    ServiceWorkerScriptCachedMetadataHandler(
        WorkerGlobalScope* worker_global_scope,
        const KURL& script_url,
        std::unique_ptr<Vector<uint8_t>> meta_data)
    : worker_global_scope_(worker_global_scope), script_url_(script_url) {
  if (meta_data) {
    cached_metadata_ =
        CachedMetadata::CreateFromSerializedData(std::move(*meta_data));
  }
}

ServiceWorkerScriptCachedMetadataHandler::
    ~ServiceWorkerScriptCachedMetadataHandler() = default;

void ServiceWorkerScriptCachedMetadataHandler::Trace(blink::Visitor* visitor) {
  visitor->Trace(worker_global_scope_);
  CachedMetadataHandler::Trace(visitor);
}

void ServiceWorkerScriptCachedMetadataHandler::SetCachedMetadata(
    uint32_t data_type_id,
    const uint8_t* data,
    size_t size,
    CacheType type) {
  if (type != kSendToPlatform)
    return;
  cached_metadata_ = CachedMetadata::Create(data_type_id, data, size);
  const Vector<uint8_t>& serialized_data = cached_metadata_->SerializedData();
  ServiceWorkerGlobalScopeClient::From(worker_global_scope_)
      ->SetCachedMetadata(script_url_, serialized_data.data(),
                          serialized_data.size());
}

void ServiceWorkerScriptCachedMetadataHandler::ClearCachedMetadata(
    CacheType type) {
  if (type != kSendToPlatform)
    return;
  cached_metadata_ = nullptr;
  ServiceWorkerGlobalScopeClient::From(worker_global_scope_)
      ->ClearCachedMetadata(script_url_);
}

scoped_refptr<CachedMetadata>
ServiceWorkerScriptCachedMetadataHandler::GetCachedMetadata(
    uint32_t data_type_id) const {
  if (!cached_metadata_ || cached_metadata_->DataTypeID() != data_type_id)
    return nullptr;
  return cached_metadata_;
}

String ServiceWorkerScriptCachedMetadataHandler::Encoding() const {
  return g_empty_string;
}

bool ServiceWorkerScriptCachedMetadataHandler::IsServedFromCacheStorage()
    const {
  return false;
}

void ServiceWorkerScriptCachedMetadataHandler::OnMemoryDump(
    WebProcessMemoryDump* pmd,
    const String& dump_prefix) const {
  if (!cached_metadata_)
    return;
  const String dump_name = dump_prefix + "/service_worker";
  auto* dump = pmd->CreateMemoryAllocatorDump(dump_name);
  dump->AddScalar("size", "bytes", GetCodeCacheSize());
  pmd->AddSuballocation(dump->Guid(),
                        String(WTF::Partitions::kAllocatedObjectPoolName));
}

size_t ServiceWorkerScriptCachedMetadataHandler::GetCodeCacheSize() const {
  return (cached_metadata_) ? cached_metadata_->SerializedData().size() : 0;
}

}  // namespace blink
