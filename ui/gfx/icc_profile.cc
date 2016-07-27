// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/icc_profile.h"

#include <list>

#include "base/containers/mru_cache.h"
#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"

namespace gfx {

namespace {
const size_t kMinProfileLength = 128;
const size_t kMaxProfileLength = 4 * 1024 * 1024;

// Allow keeping around a maximum of 8 cached ICC profiles. Beware that
// we will do a linear search thorugh currently-cached ICC profiles,
// when creating a new ICC profile.
const size_t kMaxCachedICCProfiles = 8;

struct Cache {
  Cache() : id_to_icc_profile_mru(kMaxCachedICCProfiles) {}
  ~Cache() {}

  // Start from-ICC-data IDs at the end of the hard-coded list.
  uint64_t next_unused_id = 5;
  base::MRUCache<uint64_t, ICCProfile> id_to_icc_profile_mru;
  base::Lock lock;
};
static base::LazyInstance<Cache> g_cache;

}  // namespace

ICCProfile::ICCProfile() = default;
ICCProfile::ICCProfile(ICCProfile&& other) = default;
ICCProfile::ICCProfile(const ICCProfile& other) = default;
ICCProfile& ICCProfile::operator=(ICCProfile&& other) = default;
ICCProfile& ICCProfile::operator=(const ICCProfile& other) = default;
ICCProfile::~ICCProfile() = default;

bool ICCProfile::operator==(const ICCProfile& other) const {
  return valid_ == other.valid_ && data_ == other.data_;
}

// static
ICCProfile ICCProfile::FromData(const std::vector<char>& icc_profile_data) {
  ICCProfile icc_profile;
  if (IsValidProfileLength(icc_profile_data.size())) {
    icc_profile.valid_ = true;
    icc_profile.data_ = icc_profile_data;
  }
  if (!icc_profile.valid_)
    return icc_profile;

  Cache& cache = g_cache.Get();
  base::AutoLock lock(cache.lock);

  // Linearly search the cached ICC profiles to find one with the same data.
  // If it exists, re-use its id and touch it in the cache.
  for (auto iter = cache.id_to_icc_profile_mru.begin();
       iter != cache.id_to_icc_profile_mru.end(); ++iter) {
    if (icc_profile.data_ == iter->second.data_) {
      icc_profile.id_ = iter->second.id_;
      cache.id_to_icc_profile_mru.Get(icc_profile.id_);
      return icc_profile;
    }
  }

  // Create a new cached id and add it to the cache.
  icc_profile.id_ = cache.next_unused_id++;
  cache.id_to_icc_profile_mru.Put(icc_profile.id_, icc_profile);
  return icc_profile;
}

#if !defined(OS_WIN) && !defined(OS_MACOSX)
// static
ICCProfile ICCProfile::FromBestMonitor() {
  return ICCProfile();
}
#endif

// static
ICCProfile ICCProfile::FromColorSpace(const gfx::ColorSpace& color_space) {
  // Retrieve ICC profiles from the cache.
  if (color_space.icc_profile_id_) {
    Cache& cache = g_cache.Get();
    base::AutoLock lock(cache.lock);

    auto found = cache.id_to_icc_profile_mru.Get(color_space.icc_profile_id_);
    if (found != cache.id_to_icc_profile_mru.end())
      return found->second;
  }
  // TODO(ccameron): Support constructing ICC profiles from arbitrary ColorSpace
  // objects.
  return ICCProfile();
}

const std::vector<char>& ICCProfile::GetData() const {
  return data_;
}

ColorSpace ICCProfile::GetColorSpace() const {
  ColorSpace color_space(ColorSpace::PrimaryID::CUSTOM,
                         ColorSpace::TransferID::CUSTOM,
                         ColorSpace::MatrixID::RGB, ColorSpace::RangeID::FULL);
  color_space.icc_profile_id_ = id_;

  // Move this ICC profile to the most recently used end of the cache.
  {
    Cache& cache = g_cache.Get();
    base::AutoLock lock(cache.lock);

    auto found = cache.id_to_icc_profile_mru.Get(id_);
    if (found == cache.id_to_icc_profile_mru.end())
      cache.id_to_icc_profile_mru.Put(id_, *this);
  }
  return color_space;
}

// static
bool ICCProfile::IsValidProfileLength(size_t length) {
  return length >= kMinProfileLength && length <= kMaxProfileLength;
}

}  // namespace gfx
