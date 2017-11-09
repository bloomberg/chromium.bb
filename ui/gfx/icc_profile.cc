// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/icc_profile.h"

#include <list>
#include <set>

#include "base/command_line.h"
#include "base/containers/mru_cache.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/lock.h"
#include "third_party/skia/include/core/SkColorSpaceXform.h"
#include "third_party/skia/include/core/SkICC.h"
#include "ui/gfx/skia_color_space_util.h"

namespace gfx {

const uint64_t ICCProfile::test_id_adobe_rgb_ = 1;
const uint64_t ICCProfile::test_id_color_spin_ = 2;
const uint64_t ICCProfile::test_id_generic_rgb_ = 3;
const uint64_t ICCProfile::test_id_srgb_ = 4;

// A MRU cache of ICC profiles. The cache key is a uin64_t which a
// gfx::ColorSpace may use to refer back to an ICC profile in the cache. The
// data cached for each profile is the gfx::ICCProfile structure (which includes
// the associated gfx::ColorSpace approximations and SkColorSpace structures)
// and whether or not the ICC profile has been histogrammed.
class ICCProfileCache {
 public:
  // Allow keeping around a maximum of 16 cached ICC profiles. Beware that
  // we will do a linear search thorugh currently-cached ICC profiles,
  // when creating a new ICC profile.
  static const size_t kMaxCachedICCProfiles = 16;

  ICCProfileCache() : id_to_icc_profile_mru_(kMaxCachedICCProfiles) {}
  ~ICCProfileCache() {}

  // Add |icc_profile| to the cache. If |icc_profile| does not have an id set
  // yet, assign an id to it.
  void InsertAndSetIdIfNeeded(ICCProfile* icc_profile) {
    base::AutoLock lock(lock_);

    if (FindByIdUnderLock(icc_profile->id_, icc_profile))
      return;

    if (FindByDataUnderLock(icc_profile->data_.data(),
                            icc_profile->data_.size(), icc_profile)) {
      return;
    }

    if (!icc_profile->id_)
      icc_profile->id_ = next_unused_id_++;

    Entry entry;
    entry.icc_profile = *icc_profile;
    id_to_icc_profile_mru_.Put(icc_profile->id_, entry);
  }

  // We maintain UMA histograms of display ICC profiles. Only histogram a
  // display once for each |display_id| (because we will re-read the same
  // ICC profile repeatedly when reading other display profiles, which will
  // skew samples). Return true if we need to histogram this profile for
  // |display_id|, and ensure that all future calls will return false for
  // |display_id|.
  bool GetAndSetNeedsHistogram(uint64_t display_id,
                               const ICCProfile& icc_profile) {
    base::AutoLock lock(lock_);

    // If we don't find the profile in the cache, don't histogram it.
    auto found = id_to_icc_profile_mru_.Get(icc_profile.id_);
    if (found == id_to_icc_profile_mru_.end())
      return false;

    // If we have already histogrammed this display, don't histogram it.
    std::set<int64_t>& histogrammed_display_ids =
        found->second.histogrammed_display_ids;
    if (histogrammed_display_ids.count(display_id))
      return false;

    // Histogram this display, and mark that we have done so.
    histogrammed_display_ids.insert(display_id);
    return true;
  }

  // Move this ICC profile to the most recently used end of the cache,
  // re-inserting if needed.
  void TouchEntry(const ICCProfile& icc_profile) {
    base::AutoLock lock(lock_);

    if (!icc_profile.id_)
      return;

    // Look up the profile by id to move it to the front of the MRU.
    auto found = id_to_icc_profile_mru_.Get(icc_profile.id_);
    if (found != id_to_icc_profile_mru_.end())
      return;

    // Look up the profile by its data. If there is a new entry for the same
    // data, don't add a duplicate.
    if (FindByDataUnderLock(icc_profile.data_.data(), icc_profile.data_.size(),
                            nullptr)) {
      return;
    }

    // If the entry was not found, insert it.
    Entry entry;
    entry.icc_profile = icc_profile;
    id_to_icc_profile_mru_.Put(icc_profile.id_, entry);
  }

  // Look up an ICC profile in the cache by its data (to ensure that the same
  // data gets the same id every time). On success, return true and populate
  // |icc_profile| with the associated profile.
  bool FindByData(const void* data, size_t size, ICCProfile* icc_profile) {
    base::AutoLock lock(lock_);
    return FindByDataUnderLock(data, size, icc_profile);
  }

  // Look up an ICC profile in the cache by its id. On success, return true and
  // populate |icc_profile| with the associated profile.
  bool FindById(uint64_t id, ICCProfile* icc_profile) {
    base::AutoLock lock(lock_);
    return FindByIdUnderLock(id, icc_profile);
  }

 private:
  struct Entry {
    ICCProfile icc_profile;

    // The set of display ids which have have caused this ICC profile to be
    // recorded in UMA histograms. Only record an ICC profile once per display
    // id (since the same profile will be re-read repeatedly, e.g, when displays
    // are resized).
    std::set<int64_t> histogrammed_display_ids;
  };

  // Body for FindById, executed when the cache lock is already held.
  bool FindByIdUnderLock(uint64_t id, ICCProfile* icc_profile) {
    lock_.AssertAcquired();
    if (!id)
      return false;

    auto found = id_to_icc_profile_mru_.Get(id);
    if (found == id_to_icc_profile_mru_.end())
      return false;

    *icc_profile = found->second.icc_profile;
    return true;
  }

  // Body for FindByData, executed when the cache lock is already held.
  bool FindByDataUnderLock(const void* data,
                           size_t size,
                           ICCProfile* icc_profile) {
    lock_.AssertAcquired();
    if (size == 0)
      return false;

    for (const auto& id_entry_pair : id_to_icc_profile_mru_) {
      const ICCProfile& cached_profile = id_entry_pair.second.icc_profile;
      const std::vector<char>& iter_data = cached_profile.data_;
      if (iter_data.size() != size || memcmp(data, iter_data.data(), size))
        continue;

      if (icc_profile) {
        *icc_profile = cached_profile;
        id_to_icc_profile_mru_.Get(cached_profile.id_);
      }
      return true;
    }
    return false;
  }

  // Start from-ICC-data IDs at the end of the hard-coded test id list above.
  uint64_t next_unused_id_ = 10;
  base::MRUCache<uint64_t, Entry> id_to_icc_profile_mru_;

  // Lock that must be held to access |id_to_icc_profile_mru_| and
  // |next_unused_id_|.
  base::Lock lock_;
};

namespace {

static base::LazyInstance<ICCProfileCache>::DestructorAtExit g_cache =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

ICCProfile::AnalyzeResult ICCProfile::Initialize() {
  // Start out with no parametric data.

  // Parse the profile and attempt to create a SkColorSpaceXform out of it.
  sk_sp<SkColorSpace> sk_srgb_color_space = SkColorSpace::MakeSRGB();
  sk_sp<SkICC> sk_icc = SkICC::Make(data_.data(), data_.size());
  if (!sk_icc) {
    DLOG(ERROR) << "Failed to parse ICC profile to SkICC.";
    return kICCFailedToParse;
  }
  sk_color_space_ = SkColorSpace::MakeICC(data_.data(), data_.size());
  if (!sk_color_space_) {
    DLOG(ERROR) << "Failed to parse ICC profile to SkColorSpace.";
    return kICCFailedToExtractSkColorSpace;
  }
  std::unique_ptr<SkColorSpaceXform> sk_color_space_xform =
      SkColorSpaceXform::New(sk_srgb_color_space.get(), sk_color_space_.get());
  if (!sk_color_space_xform) {
    DLOG(ERROR) << "Parsed ICC profile but can't create SkColorSpaceXform.";
    return kICCFailedToCreateXform;
  }

  // Because this SkColorSpace can be used to construct a transform, we can use
  // it to create a LUT based color transform, at the very least. If we fail to
  // get any better approximation, we'll use sRGB as our approximation.
  ColorSpace::CreateSRGB().GetPrimaryMatrix(&to_XYZD50_);
  ColorSpace::CreateSRGB().GetTransferFunction(&transfer_fn_);

  // If our SkColorSpace representation is sRGB then return that.
  if (sk_color_space_->isSRGB())
    return kICCExtractedSRGBColorSpace;

  // A primary matrix is required for our parametric representations. Use it if
  // it exists.
  SkMatrix44 to_XYZD50_matrix;
  if (!sk_icc->toXYZD50(&to_XYZD50_matrix)) {
    DLOG(ERROR) << "Failed to extract ICC profile primary matrix.";
    return kICCFailedToExtractMatrix;
  }
  to_XYZD50_ = to_XYZD50_matrix;

  // Try to directly extract a numerical transfer function. Use it if it
  // exists.
  SkColorSpaceTransferFn exact_tr_fn;
  if (sk_icc->isNumericalTransferFn(&exact_tr_fn)) {
    transfer_fn_ = exact_tr_fn;
    return kICCExtractedMatrixAndAnalyticTrFn;
  }

  // Attempt to fit a parametric transfer function to the table data in the
  // profile.
  SkColorSpaceTransferFn approx_tr_fn;
  if (!SkApproximateTransferFn(sk_icc, &transfer_fn_error_, &approx_tr_fn)) {
    DLOG(ERROR) << "Failed approximate transfer function.";
    return kICCFailedToConvergeToApproximateTrFn;
  }

  // If this converged, but has too high error, use the sRGB transfer function
  // from above.
  const float kMaxError = 2.f / 256.f;
  if (transfer_fn_error_ >= kMaxError) {
    DLOG(ERROR) << "Failed to accurately approximate transfer function, error: "
                << 256.f * transfer_fn_error_ << "/256";
    return kICCFailedToApproximateTrFnAccurately;
  };

  // If the error is sufficiently low, declare that the approximation is
  // accurate.
  transfer_fn_ = approx_tr_fn;
  return kICCExtractedMatrixAndApproximatedTrFn;
}

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

// static
ICCProfile ICCProfile::FromData(const void* data, size_t size) {
  return FromDataWithId(data, size, 0);
}

// static
ICCProfile ICCProfile::FromDataWithId(const void* data,
                                      size_t size,
                                      uint64_t new_profile_id) {
  ICCProfile icc_profile;

  if (!size)
    return icc_profile;

  // Create a new cached id and add it to the cache.
  icc_profile.id_ = new_profile_id;
  const char* data_as_char = reinterpret_cast<const char*>(data);
  icc_profile.data_.insert(icc_profile.data_.begin(), data_as_char,
                           data_as_char + size);
  icc_profile.ComputeColorSpaceAndCache();
  return icc_profile;
}

// static
const std::vector<char>& ICCProfile::GetData() const {
  return data_;
}

ColorSpace ICCProfile::GetColorSpace() const {
  g_cache.Get().TouchEntry(*this);

  if (!is_valid_)
    return ColorSpace();

  gfx::ColorSpace color_space;
  if (is_parametric_) {
    color_space = GetParametricColorSpace();
    color_space.icc_profile_sk_color_space_ = sk_color_space_;
  } else {
    color_space =
        ColorSpace::CreateCustom(to_XYZD50_, ColorSpace::TransferID::ICC_BASED);
    color_space.icc_profile_id_ = id_;
    color_space.icc_profile_sk_color_space_ = sk_color_space_;
  }
  return color_space;
}

ColorSpace ICCProfile::GetParametricColorSpace() const {
  g_cache.Get().TouchEntry(*this);

  if (!is_valid_)
    return ColorSpace();

  ColorSpace color_space =
      sk_color_space_->isSRGB()
          ? ColorSpace::CreateSRGB()
          : ColorSpace::CreateCustom(to_XYZD50_, transfer_fn_);
  if (is_parametric_)
    color_space.icc_profile_id_ = id_;
  return color_space;
}

// static
bool ICCProfile::FromId(uint64_t id,
                        ICCProfile* icc_profile) {
  return g_cache.Get().FindById(id, icc_profile);
}

void ICCProfile::ComputeColorSpaceAndCache() {
  // Early out for empty entries.
  if (data_.empty())
    return;

  // If this id already exists in the cache, copy |this| from the cache entry.
  if (g_cache.Get().FindById(id_, this))
    return;

  // If this data already exists in the cache, copy |this| from the cache entry.
  if (g_cache.Get().FindByData(data_.data(), data_.size(), this))
    return;

  // Parse the ICC profile
  analyze_result_ = Initialize();
  switch (analyze_result_) {
    case kICCExtractedSRGBColorSpace:
    case kICCExtractedMatrixAndAnalyticTrFn:
    case kICCExtractedMatrixAndApproximatedTrFn:
      // Successfully and accurately extracted color space.
      is_valid_ = true;
      is_parametric_ = true;
      break;
    case kICCFailedToConvergeToApproximateTrFn:
    case kICCFailedToApproximateTrFnAccurately:
      // Successfully but extracted a color space, but it isn't accurate enough.
      is_valid_ = true;
      is_parametric_ = false;
      break;
    case kICCFailedToExtractRawTrFn:
    case kICCFailedToExtractMatrix:
    case kICCFailedToParse:
    case kICCFailedToExtractSkColorSpace:
    case kICCFailedToCreateXform:
      // Can't even use this color space as a LUT.
      is_valid_ = false;
      is_parametric_ = false;
      break;
  }

  // Add to the cache.
  g_cache.Get().InsertAndSetIdIfNeeded(this);
}

void ICCProfile::HistogramDisplay(int64_t display_id) const {
  if (!g_cache.Get().GetAndSetNeedsHistogram(display_id, *this))
    return;

  UMA_HISTOGRAM_ENUMERATION("Blink.ColorSpace.Destination.ICCResult",
                            analyze_result_, kICCProfileAnalyzeLast);

  // Add histograms for numerical approximation.
  bool nonlinear_fit_converged =
      analyze_result_ == kICCExtractedMatrixAndApproximatedTrFn ||
      analyze_result_ == kICCFailedToApproximateTrFnAccurately;
  bool nonlinear_fit_did_not_converge =
      analyze_result_ == kICCFailedToConvergeToApproximateTrFn;
  if (nonlinear_fit_converged || nonlinear_fit_did_not_converge) {
    UMA_HISTOGRAM_BOOLEAN("Blink.ColorSpace.Destination.NonlinearFitConverged",
                          nonlinear_fit_converged);
  }
  if (nonlinear_fit_converged) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Blink.ColorSpace.Destination.NonlinearFitError",
        static_cast<int>(transfer_fn_error_ * 255), 0, 127, 16);
  }
}

}  // namespace gfx
