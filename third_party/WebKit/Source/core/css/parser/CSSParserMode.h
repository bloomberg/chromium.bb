/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef CSSParserMode_h
#define CSSParserMode_h

namespace blink {

// Must not grow beyond 3 bits, due to packing in StylePropertySet.
enum CSSParserMode {
  HTMLStandardMode,
  HTMLQuirksMode,
  // SVG attributes are parsed in quirks mode but rules differ slightly.
  SVGAttributeMode,
  // @viewport/@font-face rules are specially tagged in StylePropertySet so
  // CSSOM modifications don't treat them as style rules.
  CSSViewportRuleMode,
  CSSFontFaceRuleMode,
  // User agent stylesheets are parsed in standards mode but also allows
  // internal properties and values.
  UASheetMode
};

inline bool isQuirksModeBehavior(CSSParserMode mode) {
  return mode == HTMLQuirksMode;
}

inline bool isUASheetBehavior(CSSParserMode mode) {
  return mode == UASheetMode;
}

inline bool isCSSViewportParsingEnabledForMode(CSSParserMode mode) {
  return mode == CSSViewportRuleMode;
}

inline bool isUseCounterEnabledForMode(CSSParserMode mode) {
  // We don't count the UA style sheet in our statistics.
  return mode != UASheetMode;
}

}  // namespace blink

#endif  // CSSParserMode_h
