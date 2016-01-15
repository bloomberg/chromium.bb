// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSVariableResolver_h
#define CSSVariableResolver_h

#include "core/CSSPropertyNames.h"
#include "core/css/parser/CSSParserToken.h"
#include "wtf/HashSet.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

class CSSParserTokenRange;
class CSSVariableReferenceValue;
class StyleResolverState;
class StyleVariableData;

class CSSVariableResolver {
public:
    static void resolveVariableDefinitions(StyleVariableData*);
    static void resolveAndApplyVariableReferences(StyleResolverState&, CSSPropertyID, const CSSVariableReferenceValue&);

    // Shorthand properties are not supported.
    static PassRefPtrWillBeRawPtr<CSSValue> resolveVariableReferences(StyleVariableData*, CSSPropertyID, const CSSVariableReferenceValue&);

private:
    CSSVariableResolver(StyleVariableData*);
    CSSVariableResolver(StyleVariableData*, AtomicString& variable);

    struct ResolutionState {
        bool success;
        // Resolution doesn't finish when a cycle is detected. Fallbacks still
        // need to be tracked for additional cycles, and invalidation only
        // applies back to cycle starts. This context member tracks all
        // detected cycle start points.
        HashSet<AtomicString> cycleStartPoints;

        ResolutionState()
            : success(true)
        { };
    };

    void resolveFallback(CSSParserTokenRange, Vector<CSSParserToken>& result, ResolutionState& context);
    void resolveVariableTokensRecursive(CSSParserTokenRange, Vector<CSSParserToken>& result, ResolutionState& context);
    void resolveVariableReferencesFromTokens(CSSParserTokenRange tokens, Vector<CSSParserToken>& result, ResolutionState& context);

    StyleVariableData* m_styleVariableData;
    HashSet<AtomicString> m_variablesSeen;
};

} // namespace blink

#endif // CSSVariableResolver
