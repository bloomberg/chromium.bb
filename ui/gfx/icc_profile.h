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

  // Returns the color profile of the monitor that can best represent color.
  // This profile should be used for creating content that does not know on
  // which monitor it will be displayed.
  static ICCProfile FromBestMonitor();
#if defined(OS_MACOSX)
  static ICCProfile FromCGColorSpace(CGColorSpaceRef cg_color_space);
#endif

  // This will recover a ICCProfile from a compact ColorSpace representation.
  // Internally, this will make an effort to create an identical ICCProfile
  // to the one that created |color_space|, but this is not guaranteed.
  static ICCProfile FromColorSpace(const gfx::ColorSpace& color_space);

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

#if defined(OS_WIN)
  // This will read monitor ICC profiles from disk and cache the results for the
  // other functions to read. This should not be called on the UI or IO thread.
  static void UpdateCachedProfilesOnBackgroundThread();
  static bool CachedProfilesNeedUpdate();
#endif

 private:
  friend ICCProfile ICCProfileForTestingAdobeRGB();
  friend ICCProfile ICCProfileForTestingColorSpin();
  friend ICCProfile ICCProfileForTestingGenericRGB();
  friend ICCProfile ICCProfileForTestingSRGB();
  friend ICCProfile ICCProfileForTestingNoAnalyticTrFn();
  friend ICCProfile ICCProfileForTestingA2BOnly();
  friend ICCProfile ICCProfileForTestingOvershoot();
  friend ICCProfile ICCProfileForLayoutTests();
  static const uint64_t test_id_adobe_rgb_;
  static const uint64_t test_id_color_spin_;
  static const uint64_t test_id_generic_rgb_;
  static const uint64_t test_id_srgb_;
  static const uint64_t test_id_no_analytic_tr_fn_;
  static const uint64_t test_id_a2b_only_;
  static const uint64_t test_id_overshoot_;

  // Populate |icc_profile| with the ICCProfile corresponding to id |id|. Return
  // false if |id| is not in the cache.
  static bool FromId(uint64_t id, ICCProfile* icc_profile);

  // This method is used to hard-code the |id_| to a specific value, and is
  // used by test methods to ensure that they don't conflict with the values
  // generated in the browser.
  static ICCProfile FromDataWithId(const void* icc_profile,
                                   size_t size,
                                   uint64_t id);

  static bool HasForcedProfile();
  static ICCProfile GetForcedProfile();

  void ComputeColorSpaceAndCache();

  // This globally identifies this ICC profile. It is used to look up this ICC
  // profile from a ColorSpace object created from it. The object is invalid if
  // |id_| is zero.
  uint64_t id_ = 0;
  std::vector<char> data_;

  // |color_space| always links back to this ICC profile, and its SkColorSpace
  // is always equal to the SkColorSpace created from this ICCProfile.
  gfx::ColorSpace color_space_;

  // |parametric_color_space_| will only link back to this ICC profile if it
  // is accurate, and its SkColorSpace will always be parametrically created.
  gfx::ColorSpace parametric_color_space_;

  // This is set to true if SkICC successfully parsed this profile.
  bool successfully_parsed_by_sk_icc_ = false;

  FRIEND_TEST_ALL_PREFIXES(SimpleColorSpace, BT709toSRGBICC);
  FRIEND_TEST_ALL_PREFIXES(SimpleColorSpace, GetColorSpace);
  friend int ::LLVMFuzzerTestOneInput(const uint8_t*, size_t);
  friend class ColorSpace;
  friend class ColorTransformInternal;
  friend struct IPC::ParamTraits<gfx::ICCProfile>;
};

}  // namespace gfx

#endif  // UI_GFX_ICC_PROFILE_H_
