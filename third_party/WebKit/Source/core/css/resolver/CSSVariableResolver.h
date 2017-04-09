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
class CSSPendingSubstitutionValue;
class CSSVariableData;
class CSSVariableReferenceValue;
class PropertyRegistry;
class StyleResolverState;
class StyleInheritedVariables;
class StyleNonInheritedVariables;

class CSSVariableResolver {
  STACK_ALLOCATED();

 public:
  static void ResolveVariableDefinitions(const StyleResolverState&);

  // Shorthand properties are not supported.
  static const CSSValue* ResolveVariableReferences(
      const StyleResolverState&,
      CSSPropertyID,
      const CSSValue&,
      bool disallow_animation_tainted);

  static void ComputeRegisteredVariables(const StyleResolverState&);

 private:
  CSSVariableResolver(const StyleResolverState&);

  static const CSSValue* ResolvePendingSubstitutions(
      const StyleResolverState&,
      CSSPropertyID,
      const CSSPendingSubstitutionValue&,
      bool disallow_animation_tainted);
  static const CSSValue* ResolveVariableReferences(
      const StyleResolverState&,
      CSSPropertyID,
      const CSSVariableReferenceValue&,
      bool disallow_animation_tainted);

  // These return false if we encounter a reference to an invalid variable with
  // no fallback.

  // Resolves a range which may contain var() references or @apply rules.
  bool ResolveTokenRange(CSSParserTokenRange,
                         bool disallow_animation_tainted,
                         Vector<CSSParserToken>& result,
                         bool& result_is_animation_tainted);
  // Resolves the fallback (if present) of a var() reference, starting from the
  // comma.
  bool ResolveFallback(CSSParserTokenRange,
                       bool disallow_animation_tainted,
                       Vector<CSSParserToken>& result,
                       bool& result_is_animation_tainted);
  // Resolves the contents of a var() reference.
  bool ResolveVariableReference(CSSParserTokenRange,
                                bool disallow_animation_tainted,
                                Vector<CSSParserToken>& result,
                                bool& result_is_animation_tainted);
  // Consumes and resolves an @apply rule.
  void ResolveApplyAtRule(CSSParserTokenRange&, Vector<CSSParserToken>& result);

  // These return null if the custom property is invalid.

  // Returns the CSSVariableData for a custom property, resolving and storing it
  // if necessary.
  CSSVariableData* ValueForCustomProperty(AtomicString name);
  // Resolves the CSSVariableData from a custom property declaration.
  PassRefPtr<CSSVariableData> ResolveCustomProperty(AtomicString name,
                                                    const CSSVariableData&);

  StyleInheritedVariables* inherited_variables_;
  StyleNonInheritedVariables* non_inherited_variables_;
  Member<const PropertyRegistry> registry_;
  HashSet<AtomicString> variables_seen_;
  // Resolution doesn't finish when a cycle is detected. Fallbacks still
  // need to be tracked for additional cycles, and invalidation only
  // applies back to cycle starts.
  HashSet<AtomicString> cycle_start_points_;
};

}  // namespace blink

#endif  // CSSVariableResolver
