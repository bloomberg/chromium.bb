/*
 * Copyright (C) 2007-2011 Google Inc. All rights reserved.
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

#include "core/css/CSSStyleDeclaration.h"

#include <algorithm>

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/string_or_float.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSPropertyIDTemplates.h"
#include "core/css/CSSStyleDeclaration.h"
#include "core/css/CSSValue.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/properties/css_property.h"
#include "core/css_property_names.h"
#include "platform/wtf/ASCIICType.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

namespace {

// Check for a CSS prefix.
// Passed prefix is all lowercase.
// First character of the prefix within the property name may be upper or
// lowercase.
// Other characters in the prefix within the property name must be lowercase.
// The prefix within the property name must be followed by a capital letter.
bool HasCSSPropertyNamePrefix(const AtomicString& property_name,
                              const char* prefix) {
#if DCHECK_IS_ON()
  DCHECK(*prefix);
  for (const char* p = prefix; *p; ++p)
    DCHECK(IsASCIILower(*p));
  DCHECK(property_name.length());
#endif

  if (ToASCIILower(property_name[0]) != prefix[0])
    return false;

  unsigned length = property_name.length();
  for (unsigned i = 1; i < length; ++i) {
    if (!prefix[i])
      return IsASCIIUpper(property_name[i]);
    if (property_name[i] != prefix[i])
      return false;
  }
  return false;
}

CSSPropertyID ParseCSSPropertyID(const AtomicString& property_name) {
  unsigned length = property_name.length();
  if (!length)
    return CSSPropertyInvalid;

  StringBuilder builder;
  builder.ReserveCapacity(length);

  unsigned i = 0;
  bool has_seen_dash = false;

  if (HasCSSPropertyNamePrefix(property_name, "webkit"))
    builder.Append('-');
  else if (IsASCIIUpper(property_name[0]))
    return CSSPropertyInvalid;

  bool has_seen_upper = IsASCIIUpper(property_name[i]);

  builder.Append(ToASCIILower(property_name[i++]));

  for (; i < length; ++i) {
    UChar c = property_name[i];
    if (!IsASCIIUpper(c)) {
      if (c == '-')
        has_seen_dash = true;
      builder.Append(c);
    } else {
      has_seen_upper = true;
      builder.Append('-');
      builder.Append(ToASCIILower(c));
    }
  }

  // Reject names containing both dashes and upper-case characters, such as
  // "border-rightColor".
  if (has_seen_dash && has_seen_upper)
    return CSSPropertyInvalid;

  String prop_name = builder.ToString();
  return unresolvedCSSPropertyID(prop_name);
}

// When getting properties on CSSStyleDeclarations, the name used from
// Javascript and the actual name of the property are not the same, so
// we have to do the following translation. The translation turns upper
// case characters into lower case characters and inserts dashes to
// separate words.
//
// Example: 'backgroundPositionY' -> 'background-position-y'
//
// Also, certain prefixes such as 'css-' are stripped.
CSSPropertyID CssPropertyInfo(const AtomicString& name) {
  typedef HashMap<String, CSSPropertyID> CSSPropertyIDMap;
  DEFINE_STATIC_LOCAL(CSSPropertyIDMap, map, ());
  CSSPropertyIDMap::iterator iter = map.find(name);
  if (iter != map.end())
    return iter->value;

  CSSPropertyID unresolved_property = ParseCSSPropertyID(name);
  if (unresolved_property == CSSPropertyVariable)
    unresolved_property = CSSPropertyInvalid;
  map.insert(name, unresolved_property);
  DCHECK(
      !unresolved_property ||
      CSSProperty::Get(resolveCSSPropertyID(unresolved_property)).IsEnabled());
  return unresolved_property;
}

}  // namespace

void CSSStyleDeclaration::AnonymousNamedGetter(const AtomicString& name,
                                               StringOrFloat& result) {
  // Search the style declaration.
  CSSPropertyID unresolved_property = CssPropertyInfo(name);

  // Do not handle non-property names.
  if (!unresolved_property)
    return;
  CSSPropertyID resolved_property = resolveCSSPropertyID(unresolved_property);

  const CSSValue* css_value = GetPropertyCSSValueInternal(resolved_property);
  if (css_value) {
    const String css_text = css_value->CssText();
    if (!css_text.IsNull()) {
      result.SetString(css_text);
      return;
    }
  }

  result.SetString(GetPropertyValueInternal(resolved_property));
}

bool CSSStyleDeclaration::AnonymousNamedSetter(
    ScriptState* script_state,
    const AtomicString& name,
    const String& value,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid())
    return false;

  CSSPropertyID unresolved_property = CssPropertyInfo(name);
  if (!unresolved_property)
    return false;
  SetPropertyInternal(
      unresolved_property, String(), value, false,
      ExecutionContext::From(script_state)->GetSecureContextMode(),
      exception_state);
  return true;
}

void CSSStyleDeclaration::NamedPropertyEnumerator(Vector<String>& names,
                                                  ExceptionState&) {
  typedef Vector<String, numCSSProperties - 1> PreAllocatedPropertyVector;
  DEFINE_STATIC_LOCAL(PreAllocatedPropertyVector, property_names, ());

  if (property_names.IsEmpty()) {
    for (int id = firstCSSProperty; id <= lastCSSProperty; ++id) {
      CSSPropertyID property_id = static_cast<CSSPropertyID>(id);
      const CSSProperty& property_class =
          CSSProperty::Get(resolveCSSPropertyID(property_id));
      if (property_class.IsEnabled())
        property_names.push_back(property_class.GetJSPropertyName());
    }
    std::sort(property_names.begin(), property_names.end(),
              WTF::CodePointCompareLessThan);
  }
  names = property_names;
}

bool CSSStyleDeclaration::NamedPropertyQuery(const AtomicString& name,
                                             ExceptionState&) {
  return CssPropertyInfo(name);
}

}  // namespace blink
