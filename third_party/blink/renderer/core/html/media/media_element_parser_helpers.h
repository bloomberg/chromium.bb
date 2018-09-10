// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_MEDIA_ELEMENT_PARSER_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_MEDIA_ELEMENT_PARSER_HELPERS_H_

#include "third_party/blink/renderer/platform/geometry/int_size.h"

namespace blink {

namespace MediaElementParserHelpers {

// Parses the intrinsicSize attribute of HTMLImageElement, HTMLVideoElement, and
// SVGImageElement. Returns true if the value is updated.
// https://github.com/ojanvafai/intrinsicsize-attribute/blob/master/README.md
bool ParseIntrinsicSizeAttribute(const String& value,
                                 IntSize* intrinsic_size,
                                 String* message);

}  // namespace MediaElementParserHelpers

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_MEDIA_ELEMENT_PARSER_HELPERS_H_
