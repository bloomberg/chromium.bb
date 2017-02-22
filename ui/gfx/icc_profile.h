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
class GFX_EXPORT ICCProfile {
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

  // This will perform a potentially-lossy conversion to a more compact color
  // space representation.
  const ColorSpace& GetColorSpace() const;

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
  static const uint64_t test_id_adobe_rgb_;
  static const uint64_t test_id_color_spin_;
  static const uint64_t test_id_generic_rgb_;
  static const uint64_t test_id_srgb_;
  static const uint64_t test_id_no_analytic_tr_fn_;

  // Populate |icc_profile| with the ICCProfile corresponding to id |id|. Return
  // false if |id| is not in the cache. If |only_if_needed| is true, then return
  // false if |color_space_is_accurate_| is true for this profile (that is, if
  // the ICCProfile is needed to know the space precisely).
  static bool FromId(uint64_t id, bool only_if_needed, ICCProfile* icc_profile);

  // This method is used to hard-code the |id_| to a specific value, and is
  // used by test methods to ensure that they don't conflict with the values
  // generated in the browser.
  static ICCProfile FromDataWithId(const void* icc_profile,
                                   size_t size,
                                   uint64_t id);

  static bool IsValidProfileLength(size_t length);
  void ComputeColorSpaceAndCache();

  // This globally identifies this ICC profile. It is used to look up this ICC
  // profile from a ColorSpace object created from it. The object is invalid if
  // |id_| is zero.
  uint64_t id_ = 0;
  std::vector<char> data_;

  gfx::ColorSpace color_space_;

  // True if |color_space_| accurately represents this color space (this is
  // false e.g, for lookup-based profiles).
  bool color_space_is_accurate_ = false;

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
