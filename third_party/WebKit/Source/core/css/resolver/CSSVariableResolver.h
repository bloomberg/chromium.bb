// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSVariableResolver_h
#define CSSVariableResolver_h

#include "core/CSSPropertyNames.h"
#include "core/css/parser/CSSParserToken.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/AtomicStringHash.h"

namespace blink {

class CSSCustomPropertyDeclaration;
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
  CSSVariableResolver(const StyleResolverState&);

  scoped_refptr<CSSVariableData> ResolveCustomPropertyAnimationKeyframe(
      const CSSCustomPropertyDeclaration& keyframe,
      bool& cycle_detected);

  void ResolveVariableDefinitions();

  // Shorthand properties are not supported.
  const CSSValue* ResolveVariableReferences(CSSPropertyID,
                                            const CSSValue&,
                                            bool disallow_animation_tainted);

  void ComputeRegisteredVariables();

 private:
  const CSSValue* ResolvePendingSubstitutions(
      CSSPropertyID,
      const CSSPendingSubstitutionValue&,
      bool disallow_animation_tainted);
  const CSSValue* ResolveVariableReferences(CSSPropertyID,
                                            const CSSVariableReferenceValue&,
                                            bool disallow_animation_tainted);

  // These return false if we encounter a reference to an invalid variable with
  // no fallback.

  // Resolves a range which may contain var() references or @apply rules.
  bool ResolveTokenRange(CSSParserTokenRange,
                         bool disallow_animation_tainted,
                         Vector<CSSParserToken>& result,
                         Vector<String>& result_backing_strings,
                         bool& result_is_animation_tainted);
  // Resolves the fallback (if present) of a var() reference, starting from the
  // comma.
  bool ResolveFallback(CSSParserTokenRange,
                       bool disallow_animation_tainted,
                       Vector<CSSParserToken>& result,
                       Vector<String>& result_backing_strings,
                       bool& result_is_animation_tainted);
  // Resolves the contents of a var() reference.
  bool ResolveVariableReference(CSSParserTokenRange,
                                bool disallow_animation_tainted,
                                Vector<CSSParserToken>& result,
                                Vector<String>& result_backing_strings,
                                bool& result_is_animation_tainted);
  // Consumes and resolves an @apply rule.
  void ResolveApplyAtRule(CSSParserTokenRange&,
                          Vector<CSSParserToken>& result,
                          Vector<String>& result_backing_strings);

  // These return null if the custom property is invalid.

  // Returns the CSSVariableData for a custom property, resolving and storing it
  // if necessary.
  CSSVariableData* ValueForCustomProperty(AtomicString name);
  // Resolves the CSSVariableData from a custom property declaration.
  scoped_refptr<CSSVariableData> ResolveCustomProperty(AtomicString name,
                                                       const CSSVariableData&,
                                                       bool& cycle_detected);

  const StyleResolverState& state_;
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
