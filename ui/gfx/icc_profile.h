// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ICC_PROFILE_H_
#define UI_GFX_ICC_PROFILE_H_

#include <stdint.h>
#include <set>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/color_space.h"

#if defined(OS_MACOSX)
#include <CoreGraphics/CGColorSpace.h>
#endif

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

namespace mojo {
template <typename, typename> struct StructTraits;
}

namespace gfx {


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

  // Returns true if this profile was successfully parsed by SkICC and will
  // return a valid ColorSpace.
  bool IsValid() const;

#if defined(OS_MACOSX)
  static ICCProfile FromCGColorSpace(CGColorSpaceRef cg_color_space);
#endif

  // Create directly from profile data.
  static ICCProfile FromData(const void* icc_profile, size_t size);

  // Return a ColorSpace that references this ICCProfile. ColorTransforms
  // created using this ColorSpace will match this ICCProfile precisely.
  ColorSpace GetColorSpace() const;

  // Return a ColorSpace that is the best parametric approximation of this
  // ICCProfile. The resulting ColorSpace will reference this ICCProfile only
  // if the parametric approximation is almost exact.
  ColorSpace GetParametricColorSpace() const;

  // Histogram how we this was approximated by a gfx::ColorSpace. Only
  // histogram a given profile once per display.
  void HistogramDisplay(int64_t display_id) const;

 private:
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
  static ICCProfile FromDataWithId(const void* data, size_t size, uint64_t id);

  // Move the cache entry for this profile to the most recently used place.
  void TouchCacheEntry() const;

  class Internals : public base::RefCountedThreadSafe<ICCProfile::Internals> {
   public:
    Internals(std::vector<char>, uint64_t id);
    void HistogramDisplay(int64_t display_id);

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

    // This globally identifies this ICC profile. It is used to look up this ICC
    // profile from a ColorSpace object created from it. The object is invalid
    // if |id_| is zero.
    const uint64_t id_ = 0;
    const std::vector<char> data_;

    // The result of attepting to extract a color space from the color profile.
    AnalyzeResult analyze_result_ = kICCFailedToParse;

    // True iff we can create a valid ColorSpace (and ColorTransform) from this
    // object. The transform may be LUT-based (using an SkColorSpaceXform to
    // compute the lut).
    bool is_valid_ = false;

    // True iff |to_XYZD50_| and |transfer_fn_| are accurate representations of
    // the data in this profile. In this case ColorTransforms created from this
    // profile will be analytic and not LUT-based.
    bool is_parametric_ = false;

    // Results of Skia parsing the ICC profile data.
    sk_sp<SkColorSpace> sk_color_space_;

    // The best-fit parametric primaries and transfer function.
    SkMatrix44 to_XYZD50_;
    SkColorSpaceTransferFn transfer_fn_;

    // The L-infinity error of the parametric color space fit. This is undefined
    // unless |analyze_result_| is kICCFailedToApproximateTrFnAccurately or
    // kICCExtractedMatrixAndApproximatedTrFn.
    float transfer_fn_error_ = 0;

    // The set of display ids which have have caused this ICC profile to be
    // recorded in UMA histograms. Only record an ICC profile once per display
    // id (since the same profile will be re-read repeatedly, e.g, when displays
    // are resized).
    std::set<int64_t> histogrammed_display_ids_;

   protected:
    friend class base::RefCountedThreadSafe<ICCProfile::Internals>;
    AnalyzeResult Initialize();
    virtual ~Internals();
  };
  scoped_refptr<Internals> internals_;

  FRIEND_TEST_ALL_PREFIXES(SimpleColorSpace, BT709toSRGBICC);
  FRIEND_TEST_ALL_PREFIXES(SimpleColorSpace, GetColorSpace);
  friend int ::LLVMFuzzerTestOneInput(const uint8_t*, size_t);
  friend class ColorSpace;
  friend class ColorTransformInternal;
  friend struct IPC::ParamTraits<gfx::ICCProfile>;
};

}  // namespace gfx

#endif  // UI_GFX_ICC_PROFILE_H_
