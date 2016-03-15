// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyParserHelpers_h
#define CSSPropertyParserHelpers_h

#include "core/css/CSSValuePool.h"
#include "core/css/parser/CSSParserMode.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "platform/Length.h" // For ValueRange
#include "platform/heap/Handle.h"

namespace blink {

class CSSStringValue;

// When these functions are successful, they will consume all the relevant
// tokens from the range and also consume any whitespace which follows. When
// the start of the range doesn't match the type we're looking for, the range
// will not be modified.
namespace CSSPropertyParserHelpers {

// TODO(timloh): These should probably just be consumeComma and consumeSlash.
bool consumeCommaIncludingWhitespace(CSSParserTokenRange&);
bool consumeSlashIncludingWhitespace(CSSParserTokenRange&);
// consumeFunction expects the range starts with a FunctionToken.
CSSParserTokenRange consumeFunction(CSSParserTokenRange&);

enum class UnitlessQuirk {
    Allow,
    Forbid
};

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeInteger(CSSParserTokenRange&, double minimumValue = -std::numeric_limits<double>::max());
PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumePositiveInteger(CSSParserTokenRange&);
bool consumeNumberRaw(CSSParserTokenRange&, double& result);
PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeNumber(CSSParserTokenRange&, ValueRange);
PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeLength(CSSParserTokenRange&, CSSParserMode, ValueRange, UnitlessQuirk = UnitlessQuirk::Forbid);
PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumePercent(CSSParserTokenRange&, ValueRange);
PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeLengthOrPercent(CSSParserTokenRange&, CSSParserMode, ValueRange, UnitlessQuirk = UnitlessQuirk::Forbid);
PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeAngle(CSSParserTokenRange&);
PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeTime(CSSParserTokenRange&, ValueRange);

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeIdent(CSSParserTokenRange&);
PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeIdentRange(CSSParserTokenRange&, CSSValueID lower, CSSValueID upper);
template<CSSValueID, CSSValueID...> inline bool identMatches(CSSValueID id);
template<CSSValueID... allowedIdents> PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeIdent(CSSParserTokenRange&);

PassRefPtrWillBeRawPtr<CSSCustomIdentValue> consumeCustomIdent(CSSParserTokenRange&);
PassRefPtrWillBeRawPtr<CSSStringValue> consumeString(CSSParserTokenRange&);
String consumeUrl(CSSParserTokenRange&);

// TODO(timloh): Move across consumeColor, consumeImage and consumePosition

// Template implementations are at the bottom of the file for readability.

template<typename... emptyBaseCase> inline bool identMatches(CSSValueID id) { return false; }
template<CSSValueID head, CSSValueID... tail> inline bool identMatches(CSSValueID id)
{
    return id == head || identMatches<tail...>(id);
}

template<CSSValueID... names> PassRefPtrWillBeRawPtr<CSSPrimitiveValue> consumeIdent(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken || !identMatches<names...>(range.peek().id()))
        return nullptr;
    return cssValuePool().createIdentifierValue(range.consumeIncludingWhitespace().id());
}

} // namespace CSSPropertyParserHelpers

} // namespace blink

#endif // CSSPropertyParserHelpers_h
