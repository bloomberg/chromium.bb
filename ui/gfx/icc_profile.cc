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

namespace {

static const size_t kMaxCachedICCProfiles = 16;

// An MRU cache mapping ColorSpace objects to the ICCProfile that created them.
// This cache is necessary only on macOS, for power consumption reasons. In
// particular:
//  * IOSurfaces specify their output color space by raw ICC profile data.
//  * If the IOSurface ICC profile does not exactly match the output monitor's
//    ICC profile, there is a substantial power cost.
//  * This structure allows us to retrieve the exact ICC profile data that
//    produced a given ColorSpace.
using SpaceToProfileCacheBase = base::MRUCache<ColorSpace, ICCProfile>;
class SpaceToProfileCache : public SpaceToProfileCacheBase {
 public:
  SpaceToProfileCache() : SpaceToProfileCacheBase(kMaxCachedICCProfiles) {}
};
base::LazyInstance<SpaceToProfileCache>::DestructorAtExit
    g_space_to_profile_cache_mac = LAZY_INSTANCE_INITIALIZER;

// An MRU cache mapping data to ICCProfile objects, to avoid re-parsing
// profiles every time they are read.
using DataToProfileCacheBase = base::MRUCache<std::vector<char>, ICCProfile>;
class DataToProfileCache : public DataToProfileCacheBase {
 public:
  DataToProfileCache() : DataToProfileCacheBase(kMaxCachedICCProfiles) {}
};
base::LazyInstance<DataToProfileCache>::DestructorAtExit
    g_data_to_profile_cache = LAZY_INSTANCE_INITIALIZER;

// An MRU cache mapping IDs to ICCProfile objects. This is necessary for
// constructing LUT-based color transforms. In particular, it is used to look
// up the SkColorSpace for ColorSpace objects that are not parametric, so that
// that SkColorSpace may be used to construct the LUT.
using IdToProfileCacheBase = base::MRUCache<uint64_t, ICCProfile>;
class IdToProfileCache : public IdToProfileCacheBase {
 public:
  IdToProfileCache() : IdToProfileCacheBase(kMaxCachedICCProfiles) {}
};
base::LazyInstance<IdToProfileCache>::DestructorAtExit g_id_to_profile_cache =
    LAZY_INSTANCE_INITIALIZER;

// The next id to assign to a color profile.
uint64_t g_next_unused_id = 1;

// Lock that must be held to access |g_space_to_profile_cache_mac| and
// |g_next_unused_id|.
base::LazyInstance<base::Lock>::DestructorAtExit g_lock =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

ICCProfile::Internals::AnalyzeResult ICCProfile::Internals::Initialize() {
  // Start out with no parametric data.
  if (data_.empty())
    return kICCNoProfile;

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
  if (internals_ && other.internals_) {
    return internals_->data_ == other.internals_->data_ &&
           internals_->id_ == other.internals_->id_;
  }
  return false;
}

bool ICCProfile::operator!=(const ICCProfile& other) const {
  return !(*this == other);
}

bool ICCProfile::IsValid() const {
  return internals_ ? internals_->is_valid_ : false;
}

std::vector<char> ICCProfile::GetData() const {
  return internals_ ? internals_->data_ : std::vector<char>();
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
  std::vector<char> data(data_as_byte, data_as_byte + size);

  base::AutoLock lock(g_lock.Get());

  // See if there is already an entry with the same data. If so, return that
  // entry. If not, parse the data.
  ICCProfile icc_profile;
  auto found_by_data = g_data_to_profile_cache.Get().Get(data);
  if (found_by_data != g_data_to_profile_cache.Get().end()) {
    icc_profile = found_by_data->second;
  } else {
    icc_profile.internals_ =
        base::MakeRefCounted<Internals>(std::move(data), new_profile_id);
  }

  // Insert the profile into all caches.
  ColorSpace color_space = icc_profile.GetColorSpace();
  if (color_space.IsValid())
    g_space_to_profile_cache_mac.Get().Put(color_space, icc_profile);
  if (icc_profile.internals_->id_)
    g_id_to_profile_cache.Get().Put(icc_profile.internals_->id_, icc_profile);
  g_data_to_profile_cache.Get().Put(icc_profile.internals_->data_, icc_profile);

  return icc_profile;
}

ColorSpace ICCProfile::GetColorSpace() const {
  if (!internals_)
    return ColorSpace();

  if (!internals_->is_valid_)
    return ColorSpace();

  if (internals_->is_parametric_)
    return GetParametricColorSpace();

  // TODO(ccameron): Compute a reasonable approximation instead of always
  // falling back to sRGB.
  ColorSpace color_space = ColorSpace::CreateCustom(
      internals_->to_XYZD50_, ColorSpace::TransferID::IEC61966_2_1);
  color_space.icc_profile_id_ = internals_->id_;
  return color_space;
}

ColorSpace ICCProfile::GetParametricColorSpace() const {
  if (!internals_)
    return ColorSpace();

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
ICCProfile ICCProfile::FromParametricColorSpace(const ColorSpace& color_space) {
  if (!color_space.IsValid()) {
    return ICCProfile();
  }
  if (color_space.matrix_ != ColorSpace::MatrixID::RGB) {
    DLOG(ERROR) << "Not creating non-RGB ICCProfile";
    return ICCProfile();
  }
  if (color_space.range_ != ColorSpace::RangeID::FULL) {
    DLOG(ERROR) << "Not creating non-full-range ICCProfile";
    return ICCProfile();
  }
  if (color_space.icc_profile_id_) {
    DLOG(ERROR) << "Not creating non-parametric ICCProfile";
    return ICCProfile();
  }

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
  return FromDataWithId(data->data(), data->size(), 0);
}

// static
ICCProfile ICCProfile::FromCacheMac(const ColorSpace& color_space) {
  base::AutoLock lock(g_lock.Get());
  auto found_by_space = g_space_to_profile_cache_mac.Get().Get(color_space);
  if (found_by_space != g_space_to_profile_cache_mac.Get().end())
    return found_by_space->second;

  if (color_space.icc_profile_id_) {
    DLOG(ERROR) << "Failed to find id-based ColorSpace in ICCProfile cache";
  }
  return ICCProfile();
}

// static
sk_sp<SkColorSpace> ICCProfile::GetSkColorSpaceFromId(uint64_t id) {
  base::AutoLock lock(g_lock.Get());
  auto found = g_id_to_profile_cache.Get().Get(id);
  if (found == g_id_to_profile_cache.Get().end()) {
    DLOG(ERROR) << "Failed to find ICC profile with SkColorSpace from id.";
    return nullptr;
  }
  return found->second.internals_->sk_color_space_;
}

ICCProfile::Internals::Internals(std::vector<char> data, uint64_t id)
    : data_(std::move(data)), id_(id) {
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
    case kICCNoProfile:
      // Can't even use this color space as a LUT.
      is_valid_ = false;
      is_parametric_ = false;
      break;
  }

  if (id_) {
    // If |id_| has been set here, then it was specified via sending an
    // ICCProfile over IPC. Ensure that the computation of |is_valid_| and
    // |is_parametric_| match the analysis done in the sending process.
    DCHECK(is_valid_ && !is_parametric_);
  } else {
    // If this profile is not parametric, assign it an id so that we can look it
    // up from a ColorSpace. This path should only be hit in the browser
    // process.
    if (is_valid_ && !is_parametric_) {
      id_ = g_next_unused_id++;
    }
  }
}

ICCProfile::Internals::~Internals() {}

void ICCProfile::HistogramDisplay(int64_t display_id) const {
  if (!internals_) {
    // If this is an uninitialized profile, histogram it using an empty profile,
    // so that we only histogram this display as empty once.
    FromData(nullptr, 0).HistogramDisplay(display_id);
  } else {
    internals_->HistogramDisplay(display_id);
  }
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
