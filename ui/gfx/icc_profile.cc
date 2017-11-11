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

namespace {

const uint64_t kEmptyProfileId = 5;

// Start from-ICC-data IDs at the end of the hard-coded test id list above.
uint64_t g_next_unused_id = 10;

using ProfileCacheBase = base::MRUCache<uint64_t, ICCProfile>;
class ProfileCache : public ProfileCacheBase {
 public:
  static const size_t kMaxCachedICCProfiles = 16;
  ProfileCache() : ProfileCacheBase(kMaxCachedICCProfiles) {}
};
base::LazyInstance<ProfileCache>::DestructorAtExit g_cache =
    LAZY_INSTANCE_INITIALIZER;

// Lock that must be held to access |g_cache| and |g_next_unused_id|.
base::LazyInstance<base::Lock>::DestructorAtExit g_lock =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

ICCProfile::Internals::AnalyzeResult ICCProfile::Internals::Initialize() {
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
  if (!internals_ && !other.internals_)
    return true;
  if (internals_ && other.internals_)
    return internals_->data_ == other.internals_->data_;
  return false;
}

bool ICCProfile::operator!=(const ICCProfile& other) const {
  return !(*this == other);
}

bool ICCProfile::IsValid() const {
  return internals_ ? internals_->is_valid_ : false;
}

// static
ICCProfile ICCProfile::FromData(const void* data, size_t size) {
  return FromDataWithId(data, size, 0);
}

// static
ICCProfile ICCProfile::FromDataWithId(const void* data_as_void,
                                      size_t size,
                                      uint64_t new_profile_id) {
  const char* data_as_byte = reinterpret_cast<const char*>(data_as_void);
  std::vector<char> new_profile_data(data_as_byte, data_as_byte + size);

  base::AutoLock lock(g_lock.Get());

  if (new_profile_id) {
    // If this has specified an id, see if we have an entry for it (and ensure
    // that that entry have the same data).
    auto found = g_cache.Get().Get(new_profile_id);
    if (found != g_cache.Get().end()) {
      const ICCProfile& cached_profile = found->second;
      DCHECK(new_profile_data == cached_profile.internals_->data_);
      return cached_profile;
    }
  }

  // See if there is already an entry with the same data. If so, return that
  // entry (even if that means ignoring the profile id that we were provided).
  for (const auto& iter : g_cache.Get()) {
    const ICCProfile& iter_profile = iter.second;
    if (new_profile_data == iter_profile.internals_->data_)
      return iter_profile;
  }

  // Create a new id for this data if one was not specified. Always ensure that
  // the emptry profile have the same id.
  if (!new_profile_id) {
    if (size == 0)
      new_profile_id = kEmptyProfileId;
    else
      new_profile_id = g_next_unused_id++;
  }

  // Create a new profile for this data.
  ICCProfile new_profile;
  new_profile.internals_ = base::MakeRefCounted<Internals>(
      std::move(new_profile_data), new_profile_id);
  g_cache.Get().Put(new_profile_id, new_profile);
  return new_profile;
}

ColorSpace ICCProfile::GetColorSpace() const {
  if (!internals_)
    return ColorSpace();

  TouchCacheEntry();

  if (!internals_->is_valid_)
    return ColorSpace();

  gfx::ColorSpace color_space;
  if (internals_->is_parametric_) {
    color_space = GetParametricColorSpace();
    color_space.icc_profile_sk_color_space_ = internals_->sk_color_space_;
  } else {
    color_space = ColorSpace::CreateCustom(internals_->to_XYZD50_,
                                           ColorSpace::TransferID::ICC_BASED);
    color_space.icc_profile_id_ = internals_->id_;
    color_space.icc_profile_sk_color_space_ = internals_->sk_color_space_;
  }
  return color_space;
}

ColorSpace ICCProfile::GetParametricColorSpace() const {
  if (!internals_)
    return ColorSpace();

  TouchCacheEntry();

  if (!internals_->is_valid_)
    return ColorSpace();

  ColorSpace color_space =
      internals_->sk_color_space_->isSRGB()
          ? ColorSpace::CreateSRGB()
          : ColorSpace::CreateCustom(internals_->to_XYZD50_,
                                     internals_->transfer_fn_);
  if (internals_->is_parametric_)
    color_space.icc_profile_id_ = internals_->id_;
  return color_space;
}

// static
bool ICCProfile::FromId(uint64_t id,
                        ICCProfile* icc_profile) {
  base::AutoLock lock(g_lock.Get());

  auto found = g_cache.Get().Get(id);
  if (found != g_cache.Get().end()) {
    *icc_profile = found->second;
    return true;
  }
  *icc_profile = ICCProfile();
  return false;
}

void ICCProfile::TouchCacheEntry() const {
  if (!internals_)
    return;
  base::AutoLock lock(g_lock.Get());

  // Query for an existing cache entry.
  auto found = g_cache.Get().Get(internals_->id_);
  if (found != g_cache.Get().end())
    return;

  // Check if there are any cache entries with the same data, and refuse to add
  // a duplicate entry with the same data but a different id.
  // TODO(ccameron): This is a bit odd, but this preserves existing behavior.
  for (const auto& iter : g_cache.Get()) {
    const ICCProfile& iter_profile = iter.second;
    if (internals_->data_ == iter_profile.internals_->data_)
      return;
  }

  // Insert a new cache entry if none existed.
  g_cache.Get().Put(internals_->id_, *this);
}

ICCProfile::Internals::Internals(std::vector<char> data, uint64_t id)
    : id_(id), data_(std::move(data)) {
  // Early out for empty entries.
  if (data_.empty())
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
}

ICCProfile::Internals::~Internals() {}

void ICCProfile::HistogramDisplay(int64_t display_id) const {
  if (!internals_)
    return;
  internals_->HistogramDisplay(display_id);
}

void ICCProfile::Internals::HistogramDisplay(int64_t display_id) {
  // Ensure that we histogram this profile only once per display id.
  if (histogrammed_display_ids_.count(display_id))
    return;
  histogrammed_display_ids_.insert(display_id);

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
