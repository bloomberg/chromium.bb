/*
 * This file is part of the internal font implementation.
 *
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (c) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#import "third_party/blink/renderer/platform/fonts/mac/font_platform_data_mac.h"

#import <AppKit/NSFont.h>
#import <AvailabilityMacros.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/stl_util.h"
#import "third_party/blink/public/platform/mac/web_sandbox_support.h"
#import "third_party/blink/public/platform/platform.h"
#import "third_party/blink/renderer/platform/fonts/font.h"
#import "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#import "third_party/blink/renderer/platform/fonts/mac/core_text_font_format_support.h"
#import "third_party/blink/renderer/platform/fonts/opentype/font_settings.h"
#import "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"
#import "third_party/blink/renderer/platform/web_test_support.h"
#import "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#import "third_party/skia/include/core/SkFont.h"
#import "third_party/skia/include/core/SkStream.h"
#import "third_party/skia/include/core/SkTypeface.h"
#import "third_party/skia/include/core/SkTypes.h"
#import "third_party/skia/include/ports/SkTypeface_mac.h"

namespace {
constexpr SkFourByteTag kOpszTag = SkSetFourByteTag('o', 'p', 's', 'z');
}

namespace blink {

bool VariableAxisChangeEffective(SkTypeface* typeface,
                                 SkFourByteTag axis,
                                 float new_value) {
  // First clamp new value to within range of min and max of variable axis.
  int num_axes = typeface->getVariationDesignParameters(nullptr, 0);
  if (num_axes <= 0)
    return false;

  SkFontParameters::Variation::Axis axes_parameters[num_axes];
  int returned_axes =
      typeface->getVariationDesignParameters(axes_parameters, num_axes);
  DCHECK_EQ(num_axes, returned_axes);
  DCHECK_GE(num_axes, 0);

  float clamped_new_value = new_value;
  for (auto& axis_parameters : axes_parameters) {
    if (axis_parameters.tag == axis) {
      clamped_new_value = std::min(new_value, axis_parameters.max);
      clamped_new_value = std::max(clamped_new_value, axis_parameters.min);
    }
  }

  int num_coordinates = typeface->getVariationDesignPosition(nullptr, 0);
  if (num_coordinates <= 0)
    return true;  // Font has axes, but no positions, setting one would have an
                  // effect.

  // Then compare if clamped value differs from what is set on the font.
  SkFontArguments::VariationPosition::Coordinate coordinates[num_coordinates];
  int returned_coordinates =
      typeface->getVariationDesignPosition(coordinates, num_coordinates);

  if (returned_coordinates != num_coordinates)
    return false;  // Something went wrong in retrieving actual axis positions,
                   // font broken?

  for (auto& coordinate : coordinates) {
    if (coordinate.axis == axis)
      return coordinate.value != clamped_new_value;
  }
  return false;
}

static bool CanLoadInProcess(NSFont* ns_font) {
  base::ScopedCFTypeRef<CGFontRef> cg_font(
      CTFontCopyGraphicsFont(base::mac::NSToCFCast(ns_font), 0));
  // Toll-free bridged types CFStringRef and NSString*.
  base::scoped_nsobject<NSString> font_name(
      base::mac::CFToNSCast(CGFontCopyPostScriptName(cg_font)));
  return ![font_name isEqualToString:@"LastResort"];
}

static CFDictionaryRef CascadeToLastResortFontAttributes() {
  static CFDictionaryRef attributes;
  if (attributes)
    return attributes;

  base::ScopedCFTypeRef<CTFontDescriptorRef> last_resort(
      CTFontDescriptorCreateWithNameAndSize(CFSTR("LastResort"), 0));
  const void* descriptors[] = {last_resort};
  base::ScopedCFTypeRef<CFArrayRef> values_array(
      CFArrayCreate(kCFAllocatorDefault, descriptors, base::size(descriptors),
                    &kCFTypeArrayCallBacks));

  const void* keys[] = {kCTFontCascadeListAttribute};
  const void* values[] = {values_array};
  attributes = CFDictionaryCreate(
      kCFAllocatorDefault, keys, values, base::size(keys),
      &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  return attributes;
}

static sk_sp<SkTypeface> LoadFromBrowserProcess(NSFont* ns_font,
                                                float text_size) {
  // Send cross-process request to load font.
  WebSandboxSupport* sandbox_support = Platform::Current()->GetSandboxSupport();
  if (!sandbox_support) {
    // This function should only be called in response to an error loading a
    // font due to being blocked by the sandbox.
    // This by definition shouldn't happen if there is no sandbox support.
    NOTREACHED();
    return nullptr;
  }

  base::ScopedCFTypeRef<CTFontDescriptorRef> loaded_data_descriptor;
  uint32_t font_id;
  if (!sandbox_support->LoadFont(base::mac::NSToCFCast(ns_font),
                                 &loaded_data_descriptor, &font_id)) {
    // TODO crbug.com/461279: Make this appear in the inspector console?
    DLOG(ERROR)
        << "Loading user font \"" << [[ns_font familyName] UTF8String]
        << "\" from non system location failed. Corrupt or missing font file?";
    return nullptr;
  }

  base::ScopedCFTypeRef<CTFontDescriptorRef> data_descriptor_with_cascade(
      CTFontDescriptorCreateCopyWithAttributes(
          loaded_data_descriptor, CascadeToLastResortFontAttributes()));
  base::ScopedCFTypeRef<CTFontRef> ct_font(CTFontCreateWithFontDescriptor(
      data_descriptor_with_cascade.get(), text_size, 0));
  sk_sp<SkTypeface> return_font = SkMakeTypefaceFromCTFont(ct_font);

  if (!return_font.get())
    // TODO crbug.com/461279: Make this appear in the inspector console?
    DLOG(ERROR)
        << "Instantiating SkTypeface from user font failed for font family \""
        << [[ns_font familyName] UTF8String] << "\".";
  return return_font;
}

std::unique_ptr<FontPlatformData> FontPlatformDataFromNSFont(
    NSFont* ns_font,
    float size,
    bool synthetic_bold,
    bool synthetic_italic,
    FontOrientation orientation,
    OpticalSizing optical_sizing,
    FontVariationSettings* variation_settings) {
  DCHECK(ns_font);
  sk_sp<SkTypeface> typeface;
  if (CanLoadInProcess(ns_font)) {
    typeface = SkMakeTypefaceFromCTFont(base::mac::NSToCFCast(ns_font));
  } else {
    // In process loading fails for cases where third party font manager
    // software registers fonts in non system locations such as /Library/Fonts
    // and ~/Library Fonts, see crbug.com/72727 or crbug.com/108645.
    typeface = LoadFromBrowserProcess(ns_font, size);
  }

  auto make_typeface_fontplatformdata = [&typeface, &size, &synthetic_bold,
                                         &synthetic_italic, &orientation]() {
    return std::make_unique<FontPlatformData>(
        std::move(typeface), std::string(), size, synthetic_bold,
        synthetic_italic, orientation);
  };

  wtf_size_t valid_configured_axes =
      variation_settings && variation_settings->size() < UINT16_MAX
          ? variation_settings->size()
          : 0;

  // No variable font requested, return static font.
  if (!valid_configured_axes && optical_sizing == kNoneOpticalSizing)
    return make_typeface_fontplatformdata();

  if (!typeface)
    return nullptr;

  int existing_axes = typeface->getVariationDesignPosition(nullptr, 0);
  // Don't apply variation parameters if the font does not have axes or we
  // fail to retrieve the existing ones.
  if (existing_axes <= 0)
    return make_typeface_fontplatformdata();

  Vector<SkFontArguments::VariationPosition::Coordinate> coordinates_to_set;
  coordinates_to_set.resize(existing_axes);

  if (typeface->getVariationDesignPosition(coordinates_to_set.data(),
                                           existing_axes) != existing_axes) {
    return make_typeface_fontplatformdata();
  }

  // Iterate over the font's axes and find a missing tag from variation
  // settings, special case opsz, track the number of axes reconfigured.
  bool axes_reconfigured = false;
  for (auto& coordinate : coordinates_to_set) {
    // Set opsz to font size but allow having it overriden by
    // font-variation-settings in case it has 'opsz'.
    if (coordinate.axis == kOpszTag && optical_sizing == kAutoOpticalSizing) {
      if (VariableAxisChangeEffective(typeface.get(), coordinate.axis, size)) {
        coordinate.value = SkFloatToScalar(size);
        axes_reconfigured = true;
      }
    }
    FontVariationAxis found_variation_setting(AtomicString(), 0);
    if (variation_settings &&
        variation_settings->FindPair(FourByteTagToAtomicString(coordinate.axis),
                                     &found_variation_setting)) {
      if (VariableAxisChangeEffective(typeface.get(), coordinate.axis,
                                      found_variation_setting.Value())) {
        coordinate.value = found_variation_setting.Value();
        axes_reconfigured = true;
      }
    }
  }

  if (!axes_reconfigured) {
    // No variable axes touched, return the previous typeface.
    return make_typeface_fontplatformdata();
  }

  SkFontArguments::VariationPosition variation_design_position{
      coordinates_to_set.data(), coordinates_to_set.size()};

  sk_sp<SkTypeface> cloned_typeface(typeface->makeClone(
      SkFontArguments().setVariationDesignPosition(variation_design_position)));

  if (!cloned_typeface) {
    // Applying varition parameters failed, return original typeface.
    return make_typeface_fontplatformdata();
  }
  typeface = cloned_typeface;
  return make_typeface_fontplatformdata();
}

void FontPlatformData::SetupSkFont(SkFont* skfont,
                                   float,
                                   const Font* font) const {
  bool should_smooth_fonts = true;
  bool should_antialias = true;
  bool should_subpixel_position = true;

  if (font) {
    switch (font->GetFontDescription().FontSmoothing()) {
      case kAntialiased:
        should_smooth_fonts = false;
        break;
      case kSubpixelAntialiased:
        break;
      case kNoSmoothing:
        should_antialias = false;
        should_smooth_fonts = false;
        break;
      case kAutoSmoothing:
        // For the AutoSmooth case, don't do anything! Keep the default
        // settings.
        break;
    }
  }

  if (WebTestSupport::IsRunningWebTest()) {
    should_smooth_fonts = false;
    should_antialias =
        should_antialias && WebTestSupport::IsFontAntialiasingEnabledForTest();
    should_subpixel_position =
        WebTestSupport::IsTextSubpixelPositioningAllowedForTest();
  }

  if (should_antialias && should_smooth_fonts) {
    skfont->setEdging(SkFont::Edging::kSubpixelAntiAlias);
  } else if (should_antialias) {
    skfont->setEdging(SkFont::Edging::kAntiAlias);
  } else {
    skfont->setEdging(SkFont::Edging::kAlias);
  }
  skfont->setEmbeddedBitmaps(false);
  const float ts = text_size_ >= 0 ? text_size_ : 12;
  skfont->setSize(SkFloatToScalar(ts));
  skfont->setTypeface(typeface_);
  skfont->setEmbolden(synthetic_bold_);
  skfont->setSkewX(synthetic_italic_ ? -SK_Scalar1 / 4 : 0);
  skfont->setSubpixel(should_subpixel_position);

  // CoreText always provides linear metrics if it can, so the linear metrics
  // flag setting doesn't affect typefaces backed by CoreText. However, it
  // does affect FreeType backed typefaces, so set the flag for consistency.
  skfont->setLinearMetrics(should_subpixel_position);

  // When rendering using CoreGraphics, disable hinting when
  // webkit-font-smoothing:antialiased or text-rendering:geometricPrecision is
  // used.  See crbug.com/152304
  if (font &&
      (font->GetFontDescription().FontSmoothing() == kAntialiased ||
       font->GetFontDescription().TextRendering() == kGeometricPrecision))
    skfont->setHinting(SkFontHinting::kNone);
}

}  // namespace blink
