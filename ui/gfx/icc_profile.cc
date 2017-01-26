// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/icc_profile.h"

#include <list>

#include "base/containers/mru_cache.h"
#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkICC.h"
#include "ui/gfx/color_transform.h"

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
  return data_ == data_;
}

bool ICCProfile::operator!=(const ICCProfile& other) const {
  return !(*this == other);
}

// static
ICCProfile ICCProfile::FromData(const void* data, size_t size) {
  if (!IsValidProfileLength(size)) {
    if (size != 0)
      DLOG(ERROR) << "Invalid ICC profile length: " << size << ".";
    return ICCProfile();
  }

  uint64_t new_profile_id = 0;
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
ICCProfile ICCProfile::FromColorSpace(const gfx::ColorSpace& color_space) {
  if (color_space == gfx::ColorSpace())
    return ICCProfile();

  // If |color_space| was created from an ICC profile, retrieve that exact
  // profile.
  if (color_space.icc_profile_id_) {
    Cache& cache = g_cache.Get();
    base::AutoLock lock(cache.lock);
    auto found = cache.id_to_icc_profile_mru.Get(color_space.icc_profile_id_);
    if (found != cache.id_to_icc_profile_mru.end())
      return found->second;
  }

  // Otherwise, construct an ICC profile based on the best approximated
  // primaries and matrix.
  SkMatrix44 to_XYZD50_matrix;
  color_space.GetPrimaryMatrix(&to_XYZD50_matrix);
  SkColorSpaceTransferFn fn;
  if (!color_space.GetTransferFunction(&fn)) {
    DLOG(ERROR) << "Failed to get ColorSpace transfer function for ICCProfile.";
    return ICCProfile();
  }

  sk_sp<SkData> data = SkICC::WriteToICC(fn, to_XYZD50_matrix);
  if (!data) {
    DLOG(ERROR) << "Failed to create SkICC.";
    return ICCProfile();
  }

  // gfx::ColorTransform assumes that this will return an empty profile for any
  // color space that was not constructed from an ICC profile.
  // TODO(ccameron): Fix this assumption.
  // return FromData(data->data(), data->size());
  return ICCProfile();
}

ICCProfile ICCProfile::FromSkColorSpace(sk_sp<SkColorSpace> color_space) {
  ICCProfile icc_profile;

  Cache& cache = g_cache.Get();
  base::AutoLock lock(cache.lock);

  // Linearly search the cached ICC profiles to find one with the same data.
  // If it exists, re-use its id and touch it in the cache.
  for (auto iter = cache.id_to_icc_profile_mru.begin();
       iter != cache.id_to_icc_profile_mru.end(); ++iter) {
    sk_sp<SkColorSpace> iter_color_space =
        iter->second.color_space_.ToSkColorSpace();
    if (SkColorSpace::Equals(color_space.get(), iter_color_space.get())) {
      icc_profile = iter->second;
      cache.id_to_icc_profile_mru.Get(icc_profile.id_);
      return icc_profile;
    }
  }

  // TODO(ccameron): Support constructing ICC profiles from arbitrary
  // SkColorSpace objects.
  DLOG(ERROR) << "Failed to find ICC profile matching SkColorSpace.";
  return icc_profile;
}

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
      return;
    }
  }

  // Compute the color space.
  color_space_ = gfx::ColorSpace(
      ColorSpace::PrimaryID::CUSTOM, ColorSpace::TransferID::CUSTOM,
      ColorSpace::MatrixID::RGB, ColorSpace::RangeID::FULL);
  color_space_.icc_profile_id_ = id_;
  color_space_.sk_color_space_ =
      SkColorSpace::MakeICC(data_.data(), data_.size());

  sk_sp<SkICC> sk_icc = SkICC::Make(data_.data(), data_.size());
  if (sk_icc) {
    bool result;
    SkMatrix44 to_XYZD50_matrix;
    result = sk_icc->toXYZD50(&to_XYZD50_matrix);
    if (result) {
      for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
          color_space_.custom_primary_matrix_[3 * row + col] =
              to_XYZD50_matrix.get(row, col);
        }
      }
    } else {
      // Just say that the primaries were the sRGB primaries if we can't
      // extract them.
      color_space_.primaries_ = ColorSpace::PrimaryID::BT709;
      DLOG(ERROR) << "Unable to handle ICCProfile primaries.";
    }
    SkColorSpaceTransferFn fn;
    result = sk_icc->isNumericalTransferFn(&fn);
    if (result) {
      color_space_.custom_transfer_params_[0] = fn.fA;
      color_space_.custom_transfer_params_[1] = fn.fB;
      color_space_.custom_transfer_params_[2] = fn.fC;
      color_space_.custom_transfer_params_[3] = fn.fD;
      color_space_.custom_transfer_params_[4] = fn.fE;
      color_space_.custom_transfer_params_[5] = fn.fF;
      color_space_.custom_transfer_params_[6] = fn.fG;
    } else {
      // Just say that the transfer function was sRGB if we cannot read it.
      // TODO(ccameron): Use a least squares approximation of the transfer
      // function when it is not numerical.
      color_space_.transfer_ = ColorSpace::TransferID::IEC61966_2_1;
      DLOG(ERROR) << "Unable to handle ICCProfile transfer function.";
    }
  }

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
