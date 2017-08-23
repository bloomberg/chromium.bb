// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPI.h"

#include "core/CSSPropertyNames.h"
#include "core/StylePropertyShorthand.h"
#include "core/css/CSSValue.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserLocalContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyAnimationTimingFunctionUtils.h"
#include "core/css/properties/CSSPropertyBackgroundUtils.h"
#include "core/css/properties/CSSPropertyBorderImageUtils.h"
#include "core/css/properties/CSSPropertyBoxShadowUtils.h"
#include "core/css/properties/CSSPropertyGridUtils.h"
#include "core/css/properties/CSSPropertyLengthUtils.h"
#include "core/css/properties/CSSPropertyMarginUtils.h"
#include "core/css/properties/CSSPropertyPositionUtils.h"
#include "core/css/properties/CSSPropertyTextDecorationLineUtils.h"
#include "core/css/properties/CSSPropertyTransitionPropertyUtils.h"

namespace blink {

using namespace CSSPropertyParserHelpers;

const CSSValue* CSSPropertyAPI::ParseSingleValue(
    CSSPropertyID property,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  // This is the legacy ParseSingleValue code.
  // TODO(bugsnash): Move all of this to individual CSSPropertyAPI subclasses.
  switch (property) {
    case CSSPropertyTextDecoration:
      DCHECK(!RuntimeEnabledFeatures::CSS3TextDecorationsEnabled());
      return CSSPropertyTextDecorationLineUtils::ConsumeTextDecorationLine(
          range);
    default:
      return nullptr;
  }
}

bool CSSPropertyAPI::ParseShorthand(
    CSSPropertyID property,
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSProperty, 256>& properties) const {
  return false;
}

}  // namespace blink
