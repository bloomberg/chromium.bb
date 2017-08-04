/*
 * Copyright (C) 2007 Apple Computer, Inc.
 * Copyright (c) 2007, 2008, 2009, Google Inc. All rights reserved.
 * Copyright (C) 2010 Company 100, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/fonts/FontCustomPlatformData.h"

#include "build/build_config.h"
#include "platform/LayoutTestSupport.h"
#include "platform/SharedBuffer.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontPlatformData.h"
#include "platform/fonts/WebFontDecoder.h"
#if defined(OS_MACOSX)
#include "platform/fonts/mac/CoreTextVariationsSupport.h"
#endif
#include "platform/fonts/opentype/FontSettings.h"
#include "platform/fonts/opentype/VariableFontCheck.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"
#if defined(OS_WIN) || defined(OS_MACOSX)
#include "third_party/skia/include/ports/SkFontMgr_empty.h"
#endif
#include "platform/wtf/PtrUtil.h"

namespace blink {

FontCustomPlatformData::FontCustomPlatformData(sk_sp<SkTypeface> typeface,
                                               size_t data_size)
    : base_typeface_(std::move(typeface)), data_size_(data_size) {}

FontCustomPlatformData::~FontCustomPlatformData() {}

FontPlatformData FontCustomPlatformData::GetFontPlatformData(
    float size,
    bool bold,
    bool italic,
    const FontSelectionRequest& selection_request,
    const FontSelectionCapabilities& selection_capabilities,
    FontOrientation orientation,
    const FontVariationSettings* variation_settings) {
  DCHECK(base_typeface_);

  sk_sp<SkTypeface> return_typeface = base_typeface_;

  // Maximum axis count is maximum value for the OpenType USHORT,
  // which is a 16bit unsigned.
  // https://www.microsoft.com/typography/otspec/fvar.htm Variation
  // settings coming from CSS can have duplicate assignments and the
  // list can be longer than UINT16_MAX, but ignoring the length for
  // now, going with a reasonable upper limit. Deduplication is
  // handled by Skia with priority given to the last occuring
  // assignment.
  if (VariableFontCheck::IsVariableFont(base_typeface_.get())) {
#if defined(OS_WIN)
    sk_sp<SkFontMgr> fm(SkFontMgr_New_Custom_Empty());
#elif defined(OS_MACOSX)
    sk_sp<SkFontMgr> fm;
    if (CoreTextVersionSupportsVariations())
      fm = SkFontMgr::RefDefault();
    else
      fm = SkFontMgr_New_Custom_Empty();
#else
    sk_sp<SkFontMgr> fm(SkFontMgr::RefDefault());
#endif
    Vector<SkFontMgr::FontParameters::Axis, 0> axes;

    SkFontMgr::FontParameters::Axis weight_axis = {
        SkSetFourByteTag('w', 'g', 'h', 't'),
        SkFloatToScalar(selection_capabilities.weight.clampToRange(
            selection_request.weight))};
    SkFontMgr::FontParameters::Axis width_axis = {
        SkSetFourByteTag('w', 'd', 't', 'h'),
        SkFloatToScalar(selection_capabilities.width.clampToRange(
            selection_request.width))};
    SkFontMgr::FontParameters::Axis slant_axis = {
        SkSetFourByteTag('s', 'l', 'n', 't'),
        SkFloatToScalar(selection_capabilities.slope.clampToRange(
            selection_request.slope))};

    axes.push_back(weight_axis);
    axes.push_back(width_axis);
    axes.push_back(slant_axis);

    if (variation_settings && variation_settings->size() < UINT16_MAX) {
      axes.ReserveCapacity(variation_settings->size() + axes.size());
      for (size_t i = 0; i < variation_settings->size(); ++i) {
        SkFontMgr::FontParameters::Axis axis = {
            AtomicStringToFourByteTag(variation_settings->at(i).Tag()),
            SkFloatToScalar(variation_settings->at(i).Value())};
        axes.push_back(axis);
      }
    }

    sk_sp<SkTypeface> sk_variation_font(fm->createFromStream(
        base_typeface_->openStream(nullptr)->duplicate(),
        SkFontMgr::FontParameters().setAxes(axes.data(), axes.size())));

    if (sk_variation_font) {
      return_typeface = sk_variation_font;
    } else {
      SkString family_name;
      base_typeface_->getFamilyName(&family_name);
      // TODO: Surface this as a console message?
      LOG(ERROR) << "Unable for apply variation axis properties for font: "
                 << family_name.c_str();
    }
  }

  return FontPlatformData(return_typeface, "", size,
                          bold && !base_typeface_->isBold(),
                          italic && !base_typeface_->isItalic(), orientation);
}

PassRefPtr<FontCustomPlatformData> FontCustomPlatformData::Create(
    SharedBuffer* buffer,
    String& ots_parse_message) {
  DCHECK(buffer);
  WebFontDecoder decoder;
  sk_sp<SkTypeface> typeface = decoder.Decode(buffer);
  if (!typeface) {
    ots_parse_message = decoder.GetErrorString();
    return nullptr;
  }
  return AdoptRef(
      new FontCustomPlatformData(std::move(typeface), decoder.DecodedSize()));
}

bool FontCustomPlatformData::SupportsFormat(const String& format) {
  return DeprecatedEqualIgnoringCase(format, "truetype") ||
         DeprecatedEqualIgnoringCase(format, "opentype") ||
         WebFontDecoder::SupportsFormat(format);
}

}  // namespace blink
