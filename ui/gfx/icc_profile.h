// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ICC_PROFILE_H_
#define UI_GFX_ICC_PROFILE_H_

#include <stdint.h>
#include <vector>

#include "base/gtest_prod_util.h"
#include "ui/gfx/color_space.h"

#if defined(OS_MACOSX)
#include <CoreGraphics/CGColorSpace.h>
#endif

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

namespace mojo {
template <typename, typename> struct StructTraits;
}

namespace gfx {

class ICCProfileCache;

// Used to represent a full ICC profile, usually retrieved from a monitor. It
// can be lossily compressed into a ColorSpace object. This structure should
// only be sent from higher-privilege processes to lower-privilege processes,
// as parsing this structure is not secure.
class COLOR_SPACE_EXPORT ICCProfile {
 public:
  ICCProfile();
  ICCProfile(ICCProfile&& other);
  ICCProfile(const ICCProfile& other);
  ICCProfile& operator=(ICCProfile&& other);
  ICCProfile& operator=(const ICCProfile& other);
  ~ICCProfile();
  bool operator==(const ICCProfile& other) const;
  bool operator!=(const ICCProfile& other) const;

  // Returns true if this profile was successfully parsed by SkICC.
  bool IsValid() const;

#if defined(OS_MACOSX)
  static ICCProfile FromCGColorSpace(CGColorSpaceRef cg_color_space);
#endif

  // Create directly from profile data.
  static ICCProfile FromData(const void* icc_profile, size_t size);

  // Return a ColorSpace that references this ICCProfile. ColorTransforms
  // created using this ColorSpace will match this ICCProfile precisely.
  const ColorSpace& GetColorSpace() const;

  // Return a ColorSpace that is the best parametric approximation of this
  // ICCProfile. The resulting ColorSpace will reference this ICCProfile only
  // if the parametric approximation is almost exact.
  const ColorSpace& GetParametricColorSpace() const;

  const std::vector<char>& GetData() const;

  // Histogram how we this was approximated by a gfx::ColorSpace. Only
  // histogram a given profile once per display.
  void HistogramDisplay(int64_t display_id) const;

 private:
  // This must match ICCProfileAnalyzeResult enum in histograms.xml.
  enum AnalyzeResult {
    kICCExtractedMatrixAndAnalyticTrFn = 0,
    kICCExtractedMatrixAndApproximatedTrFn = 1,
    kICCFailedToConvergeToApproximateTrFn = 2,
    kICCFailedToExtractRawTrFn = 3,
    kICCFailedToExtractMatrix = 4,
    kICCFailedToParse = 5,
    kICCFailedToExtractSkColorSpace = 6,
    kICCFailedToCreateXform = 7,
    kICCFailedToApproximateTrFnAccurately = 8,
    kICCExtractedSRGBColorSpace = 9,
    kICCProfileAnalyzeLast = kICCExtractedSRGBColorSpace,
  };

  friend class ICCProfileCache;
  friend ICCProfile ICCProfileForTestingAdobeRGB();
  friend ICCProfile ICCProfileForTestingColorSpin();
  friend ICCProfile ICCProfileForTestingGenericRGB();
  friend ICCProfile ICCProfileForTestingSRGB();
  friend ICCProfile ICCProfileForLayoutTests();
  static const uint64_t test_id_adobe_rgb_;
  static const uint64_t test_id_color_spin_;
  static const uint64_t test_id_generic_rgb_;
  static const uint64_t test_id_srgb_;

  // Populate |icc_profile| with the ICCProfile corresponding to id |id|. Return
  // false if |id| is not in the cache.
  static bool FromId(uint64_t id, ICCProfile* icc_profile);

  // This method is used to hard-code the |id_| to a specific value, and is
  // used by layout test methods to ensure that they don't conflict with the
  // values generated in the browser.
  static ICCProfile FromDataWithId(const void* icc_profile,
                                   size_t size,
                                   uint64_t id);

  static AnalyzeResult ExtractColorSpaces(
      const std::vector<char>& data,
      gfx::ColorSpace* parametric_color_space,
      float* parametric_tr_fn_max_error,
      sk_sp<SkColorSpace>* useable_sk_color_space);

  void ComputeColorSpaceAndCache();

  // This globally identifies this ICC profile. It is used to look up this ICC
  // profile from a ColorSpace object created from it. The object is invalid if
  // |id_| is zero.
  uint64_t id_ = 0;
  std::vector<char> data_;

  // The result of attepting to extract a color space from the color profile.
  AnalyzeResult analyze_result_ = kICCFailedToParse;

  // |color_space| always links back to this ICC profile, and its SkColorSpace
  // is always equal to the SkColorSpace created from this ICCProfile.
  gfx::ColorSpace color_space_;

  // |parametric_color_space_| will only link back to this ICC profile if it
  // is accurate, and its SkColorSpace will always be parametrically created.
  gfx::ColorSpace parametric_color_space_;

  // The L-infinity error of the parametric color space fit. This is undefined
  // unless |analyze_result_| is kICCFailedToApproximateTrFnAccurately or
  // kICCExtractedMatrixAndApproximatedTrFn.
  float parametric_tr_fn_error_ = -1;

  FRIEND_TEST_ALL_PREFIXES(SimpleColorSpace, BT709toSRGBICC);
  FRIEND_TEST_ALL_PREFIXES(SimpleColorSpace, GetColorSpace);
  friend int ::LLVMFuzzerTestOneInput(const uint8_t*, size_t);
  friend class ColorSpace;
  friend class ColorTransformInternal;
  friend struct IPC::ParamTraits<gfx::ICCProfile>;
};

}  // namespace gfx

#endif  // UI_GFX_ICC_PROFILE_H_
