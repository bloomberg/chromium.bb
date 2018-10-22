// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gr_shader_cache.h"

#include "base/trace_event/trace_event.h"

namespace gpu {
namespace raster {
namespace {

std::string MakeString(const SkData* data) {
  return std::string(static_cast<const char*>(data->data()), data->size());
}

sk_sp<SkData> MakeData(const std::string& str) {
  return SkData::MakeWithCopy(str.c_str(), str.length());
}

}  // namespace

GrShaderCache::GrShaderCache(size_t max_cache_size_bytes, Client* client)
    : cache_size_limit_(max_cache_size_bytes),
      store_(Store::NO_AUTO_EVICT),
      client_(client) {}

GrShaderCache::~GrShaderCache() = default;

sk_sp<SkData> GrShaderCache::load(const SkData& key) {
  TRACE_EVENT0("gpu", "GrShaderCache::load");
  DCHECK_NE(current_client_id_, kInvalidClientId);

  CacheKey cache_key(SkData::MakeWithoutCopy(key.data(), key.size()));
  auto it = store_.Get(cache_key);
  if (it == store_.end())
    return nullptr;

  WriteToDisk(it->first, &it->second);
  return it->second.data;
}

void GrShaderCache::store(const SkData& key, const SkData& data) {
  TRACE_EVENT0("gpu", "GrShaderCache::store");
  DCHECK_NE(current_client_id_, kInvalidClientId);

  if (data.size() > cache_size_limit_)
    return;
  EnforceLimits(data.size());

  CacheKey cache_key(SkData::MakeWithCopy(key.data(), key.size()));
  auto existing_it = store_.Get(cache_key);
  if (existing_it != store_.end()) {
    // Skia may ignore the cached entry and regenerate a shader if it fails to
    // link, in which case replace the current version with the latest one.
    EraseFromCache(existing_it);
  }

  CacheData cache_data(SkData::MakeWithCopy(data.data(), data.size()));
  auto it = AddToCache(cache_key, std::move(cache_data));

  WriteToDisk(it->first, &it->second);
}

void GrShaderCache::PopulateCache(const std::string& key,
                                  const std::string& data) {
  if (data.length() > cache_size_limit_)
    return;

  EnforceLimits(data.size());

  // If we already have this in the cache, skia may have populated it before it
  // was loaded off the disk cache. Its better to keep the latest version
  // generated version than overwriting it here.
  CacheKey cache_key(MakeData(key));
  if (store_.Get(cache_key) != store_.end())
    return;

  CacheData cache_data(MakeData(data));
  auto it = AddToCache(cache_key, std::move(cache_data));

  // This was loaded off the disk cache, no need to push this back for disk
  // write.
  it->second.pending_disk_write = false;
}

GrShaderCache::Store::iterator GrShaderCache::AddToCache(CacheKey key,
                                                         CacheData data) {
  auto it = store_.Put(key, std::move(data));
  curr_size_bytes_ += it->second.data->size();
  return it;
}

template <typename Iterator>
void GrShaderCache::EraseFromCache(Iterator it) {
  DCHECK_GE(curr_size_bytes_, it->second.data->size());

  curr_size_bytes_ -= it->second.data->size();
  store_.Erase(it);
}

void GrShaderCache::CacheClientIdOnDisk(int32_t client_id) {
  client_ids_to_cache_on_disk_.insert(client_id);
}

void GrShaderCache::PurgeMemory(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  size_t original_limit = cache_size_limit_;

  switch (memory_pressure_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      // This function is only called with moderate or critical pressure.
      NOTREACHED();
      return;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      cache_size_limit_ = cache_size_limit_ / 4;
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      cache_size_limit_ = 0;
      break;
  }

  EnforceLimits(0u);
  cache_size_limit_ = original_limit;
}

void GrShaderCache::WriteToDisk(const CacheKey& key, CacheData* data) {
  DCHECK_NE(current_client_id_, kInvalidClientId);

  if (!data->pending_disk_write)
    return;

  // Only cache the shader on disk if this client id is permitted.
  if (client_ids_to_cache_on_disk_.count(current_client_id_) == 0)
    return;

  data->pending_disk_write = false;
  client_->StoreShader(MakeString(key.data.get()),
                       MakeString(data->data.get()));
}

void GrShaderCache::EnforceLimits(size_t size_needed) {
  DCHECK_LE(size_needed, cache_size_limit_);

  while (size_needed + curr_size_bytes_ > cache_size_limit_)
    EraseFromCache(store_.rbegin());
}

GrShaderCache::ScopedCacheUse::ScopedCacheUse(GrShaderCache* cache,
                                              int32_t client_id)
    : cache_(cache) {
  cache_->current_client_id_ = client_id;
}

GrShaderCache::ScopedCacheUse::~ScopedCacheUse() {
  cache_->current_client_id_ = kInvalidClientId;
}

GrShaderCache::CacheKey::CacheKey(sk_sp<SkData> data) : data(std::move(data)) {
  hash = base::Hash(this->data->data(), this->data->size());
}
GrShaderCache::CacheKey::CacheKey(const CacheKey& other) = default;
GrShaderCache::CacheKey::CacheKey(CacheKey&& other) = default;
GrShaderCache::CacheKey& GrShaderCache::CacheKey::operator=(
    const CacheKey& other) = default;
GrShaderCache::CacheKey& GrShaderCache::CacheKey::operator=(CacheKey&& other) =
    default;
GrShaderCache::CacheKey::~CacheKey() = default;

bool GrShaderCache::CacheKey::operator==(const CacheKey& other) const {
  return data->equals(other.data.get());
}

GrShaderCache::CacheData::CacheData(sk_sp<SkData> data)
    : data(std::move(data)) {}
GrShaderCache::CacheData::CacheData(CacheData&& other) = default;
GrShaderCache::CacheData& GrShaderCache::CacheData::operator=(
    CacheData&& other) = default;
GrShaderCache::CacheData::~CacheData() = default;

bool GrShaderCache::CacheData::operator==(const CacheData& other) const {
  return data->equals(other.data.get());
}

}  // namespace raster
}  // namespace gpu
