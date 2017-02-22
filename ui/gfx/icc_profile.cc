// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/icc_profile.h"

#include <list>

#include "base/containers/mru_cache.h"
#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"
#include "third_party/skia/include/core/SkICC.h"
#include "ui/gfx/color_transform.h"

namespace gfx {

const uint64_t ICCProfile::test_id_adobe_rgb_ = 1;
const uint64_t ICCProfile::test_id_color_spin_ = 2;
const uint64_t ICCProfile::test_id_generic_rgb_ = 3;
const uint64_t ICCProfile::test_id_srgb_ = 4;
const uint64_t ICCProfile::test_id_no_analytic_tr_fn_ = 5;

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

  // Start from-ICC-data IDs at the end of the hard-coded test id list above.
  uint64_t next_unused_id = 10;
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
  return data_ == other.data_;
}

bool ICCProfile::operator!=(const ICCProfile& other) const {
  return !(*this == other);
}

bool ICCProfile::IsValid() const {
  return successfully_parsed_by_sk_icc_;
}

// static
ICCProfile ICCProfile::FromData(const void* data, size_t size) {
  return FromDataWithId(data, size, 0);
}

// static
ICCProfile ICCProfile::FromDataWithId(const void* data,
                                      size_t size,
                                      uint64_t new_profile_id) {
  if (!IsValidProfileLength(size)) {
    if (size != 0)
      DLOG(ERROR) << "Invalid ICC profile length: " << size << ".";
    return ICCProfile();
  }

  const char* data_as_char = reinterpret_cast<const char*>(data);
  {
    // Linearly search the cached ICC profiles to find one with the same data.
    // If it exists, re-use its id and touch it in the cache.
    Cache& cache = g_cache.Get();
    base::AutoLock lock(cache.lock);
    for (auto iter = cache.id_to_icc_profile_mru.begin();
         iter != cache.id_to_icc_profile_mru.end(); ++iter) {
      const std::vector<char>& iter_data = iter->second.data_;
      if (iter_data.size() != size || memcmp(data, iter_data.data(), size))
        continue;
      auto found = cache.id_to_icc_profile_mru.Get(iter->second.id_);
      return found->second;
    }
    if (!new_profile_id)
      new_profile_id = cache.next_unused_id++;
  }

  // Create a new cached id and add it to the cache.
  ICCProfile icc_profile;
  icc_profile.id_ = new_profile_id;
  icc_profile.data_.insert(icc_profile.data_.begin(), data_as_char,
                           data_as_char + size);
  icc_profile.ComputeColorSpaceAndCache();
  return icc_profile;
}

#if !defined(OS_WIN) && !defined(OS_MACOSX) && !defined(USE_X11)
// static
ICCProfile ICCProfile::FromBestMonitor() {
  return ICCProfile();
}
#endif

// static
const std::vector<char>& ICCProfile::GetData() const {
  return data_;
}

const ColorSpace& ICCProfile::GetColorSpace() const {
  // Move this ICC profile to the most recently used end of the cache,
  // inserting if needed.
  if (id_) {
    Cache& cache = g_cache.Get();
    base::AutoLock lock(cache.lock);
    auto found = cache.id_to_icc_profile_mru.Get(id_);
    if (found == cache.id_to_icc_profile_mru.end())
      found = cache.id_to_icc_profile_mru.Put(id_, *this);
  }
  return color_space_;
}

// static
bool ICCProfile::FromId(uint64_t id,
                        bool only_if_needed,
                        ICCProfile* icc_profile) {
  if (!id)
    return false;

  Cache& cache = g_cache.Get();
  base::AutoLock lock(cache.lock);

  auto found = cache.id_to_icc_profile_mru.Get(id);
  if (found == cache.id_to_icc_profile_mru.end())
    return false;

  const ICCProfile& found_icc_profile = found->second;
  if (found_icc_profile.color_space_is_accurate_ && only_if_needed)
    return false;

  *icc_profile = found_icc_profile;
  return true;
}

void ICCProfile::ComputeColorSpaceAndCache() {
  if (!id_)
    return;

  // If this already exists in the cache, just update its |color_space_|.
  {
    Cache& cache = g_cache.Get();
    base::AutoLock lock(cache.lock);
    auto found = cache.id_to_icc_profile_mru.Get(id_);
    if (found != cache.id_to_icc_profile_mru.end()) {
      color_space_ = found->second.color_space_;
      successfully_parsed_by_sk_icc_ =
          found->second.successfully_parsed_by_sk_icc_;
      return;
    }
  }

  color_space_is_accurate_ = true;
  SkMatrix44 to_XYZD50_matrix;
  SkColorSpaceTransferFn fn;
  sk_sp<SkICC> sk_icc = SkICC::Make(data_.data(), data_.size());
  if (sk_icc) {
    successfully_parsed_by_sk_icc_ = true;
    if (!sk_icc->toXYZD50(&to_XYZD50_matrix)) {
      // Just say that the primaries were the sRGB primaries if we can't
      // extract them.
      gfx::ColorSpace::CreateSRGB().GetPrimaryMatrix(&to_XYZD50_matrix);
      color_space_is_accurate_ = false;
      DLOG(ERROR) << "Unable to handle ICCProfile primaries.";
    }
    if (!sk_icc->isNumericalTransferFn(&fn)) {
      // Just say that the transfer function was sRGB if we cannot read it.
      // TODO(ccameron): Use a least squares approximation of the transfer
      // function when it is not numerical.
      gfx::ColorSpace::CreateSRGB().GetTransferFunction(&fn);
      color_space_is_accurate_ = false;
      DLOG(ERROR) << "Unable to handle ICCProfile transfer function.";
    }
  } else {
    successfully_parsed_by_sk_icc_ = false;
    gfx::ColorSpace::CreateSRGB().GetPrimaryMatrix(&to_XYZD50_matrix);
    gfx::ColorSpace::CreateSRGB().GetTransferFunction(&fn);
    color_space_is_accurate_ = false;
    DLOG(ERROR) << "Unable parse ICCProfile.";
  }

  // Compute the color space.
  color_space_ = gfx::ColorSpace::CreateCustom(to_XYZD50_matrix, fn);
  color_space_.icc_profile_id_ = id_;
  color_space_.icc_profile_sk_color_space_ =
      SkColorSpace::MakeICC(data_.data(), data_.size());

  // Add to the cache.
  {
    Cache& cache = g_cache.Get();
    base::AutoLock lock(cache.lock);
    cache.id_to_icc_profile_mru.Put(id_, *this);
  }
}

// static
bool ICCProfile::IsValidProfileLength(size_t length) {
  return length >= kMinProfileLength && length <= kMaxProfileLength;
}

}  // namespace gfx
