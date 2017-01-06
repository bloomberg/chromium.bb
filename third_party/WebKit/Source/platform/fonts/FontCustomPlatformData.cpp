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

#include "platform/LayoutTestSupport.h"
#include "platform/SharedBuffer.h"
#include "platform/fonts/FontCache.h"
#include "platform/fonts/FontPlatformData.h"
#include "platform/fonts/WebFontDecoder.h"
#include "platform/fonts/opentype/FontSettings.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

FontCustomPlatformData::FontCustomPlatformData(sk_sp<SkTypeface> typeface,
                                               size_t dataSize)
    : m_baseTypeface(typeface), m_dataSize(dataSize) {}

FontCustomPlatformData::~FontCustomPlatformData() {}

FontPlatformData FontCustomPlatformData::fontPlatformData(
    float size,
    bool bold,
    bool italic,
    FontOrientation orientation,
    const FontVariationSettings* variationSettings) {
  DCHECK(m_baseTypeface);

  sk_sp<SkTypeface> returnTypeface = m_baseTypeface;

  // Maximum axis count is maximum value for the OpenType USHORT, which is a
  // 16bit unsigned.  https://www.microsoft.com/typography/otspec/fvar.htm
  // Variation settings coming from CSS can have duplicate assignments and the
  // list can be longer than UINT16_MAX, but ignoring this for now, going with a
  // reasonable upper limit and leaving the deduplication for TODO(drott),
  // crbug.com/674878 second duplicate value should supersede first..
  if (variationSettings && variationSettings->size() < UINT16_MAX) {
    sk_sp<SkFontMgr> fm(SkFontMgr::RefDefault());
    Vector<SkFontMgr::FontParameters::Axis, 0> axes;
    axes.reserveCapacity(variationSettings->size());
    for (size_t i = 0; i < variationSettings->size(); ++i) {
      SkFontMgr::FontParameters::Axis axis = {
          atomicStringToFourByteTag(variationSettings->at(i).tag()),
          SkFloatToScalar(variationSettings->at(i).value())};
      axes.push_back(axis);
    }

    sk_sp<SkTypeface> skVariationFont(fm->createFromStream(
        m_baseTypeface->openStream(nullptr)->duplicate(),
        SkFontMgr::FontParameters().setAxes(axes.data(), axes.size())));

    if (skVariationFont) {
      returnTypeface = skVariationFont;
    } else {
      SkString familyName;
      m_baseTypeface->getFamilyName(&familyName);
      // TODO: Surface this as a console message?
      LOG(ERROR) << "Unable for apply variation axis properties for font: "
                 << familyName.c_str();
    }
  }

  return FontPlatformData(returnTypeface, "", size,
                          bold && !m_baseTypeface->isBold(),
                          italic && !m_baseTypeface->isItalic(), orientation);
}

std::unique_ptr<FontCustomPlatformData> FontCustomPlatformData::create(
    SharedBuffer* buffer,
    String& otsParseMessage) {
  DCHECK(buffer);
  WebFontDecoder decoder;
  sk_sp<SkTypeface> typeface = decoder.decode(buffer);
  if (!typeface) {
    otsParseMessage = decoder.getErrorString();
    return nullptr;
  }
  return WTF::wrapUnique(
      new FontCustomPlatformData(std::move(typeface), decoder.decodedSize()));
}

bool FontCustomPlatformData::supportsFormat(const String& format) {
  return equalIgnoringCase(format, "truetype") ||
         equalIgnoringCase(format, "opentype") ||
         WebFontDecoder::supportsFormat(format);
}

}  // namespace blink
