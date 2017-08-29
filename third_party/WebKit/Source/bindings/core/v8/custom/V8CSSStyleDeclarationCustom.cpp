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

#include "bindings/core/v8/V8CSSStyleDeclaration.h"

#include <algorithm>
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/CSSPropertyNames.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSPropertyIDTemplates.h"
#include "core/css/CSSPropertyMetadata.h"
#include "core/css/CSSStyleDeclaration.h"
#include "core/css/CSSValue.h"
#include "core/css/parser/CSSParser.h"
#include "core/dom/events/EventTarget.h"
#include "core/html/custom/CEReactionsScope.h"
#include "platform/wtf/ASCIICType.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/StringBuilder.h"
#include "platform/wtf/text/StringConcatenate.h"

namespace blink {

// Check for a CSS prefix.
// Passed prefix is all lowercase.
// First character of the prefix within the property name may be upper or
// lowercase.
// Other characters in the prefix within the property name must be lowercase.
// The prefix within the property name must be followed by a capital letter.
static bool HasCSSPropertyNamePrefix(const String& property_name,
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

static CSSPropertyID ParseCSSPropertyID(const String& property_name) {
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
static CSSPropertyID CssPropertyInfo(const AtomicString& name) {
  typedef HashMap<String, CSSPropertyID> CSSPropertyIDMap;
  DEFINE_STATIC_LOCAL(CSSPropertyIDMap, map, ());
  CSSPropertyIDMap::iterator iter = map.find(name);
  if (iter != map.end())
    return iter->value;

  CSSPropertyID unresolved_property = ParseCSSPropertyID(name);
  if (unresolved_property == CSSPropertyVariable)
    unresolved_property = CSSPropertyInvalid;
  map.insert(name, unresolved_property);
  DCHECK(!unresolved_property ||
         CSSPropertyMetadata::IsEnabledProperty(unresolved_property));
  return unresolved_property;
}

void V8CSSStyleDeclaration::namedPropertyEnumeratorCustom(
    const v8::PropertyCallbackInfo<v8::Array>& info) {
  typedef Vector<String, numCSSProperties - 1> PreAllocatedPropertyVector;
  DEFINE_STATIC_LOCAL(PreAllocatedPropertyVector, property_names, ());
  static unsigned property_names_length = 0;

  if (property_names.IsEmpty()) {
    for (int id = firstCSSProperty; id <= lastCSSProperty; ++id) {
      CSSPropertyID property_id = static_cast<CSSPropertyID>(id);
      if (CSSPropertyMetadata::IsEnabledProperty(property_id))
        property_names.push_back(getJSPropertyName(property_id));
    }
    std::sort(property_names.begin(), property_names.end(),
              WTF::CodePointCompareLessThan);
    property_names_length = property_names.size();
  }

  v8::Local<v8::Context> context = info.GetIsolate()->GetCurrentContext();
  v8::Local<v8::Array> properties =
      v8::Array::New(info.GetIsolate(), property_names_length);
  for (unsigned i = 0; i < property_names_length; ++i) {
    String key = property_names.at(i);
    DCHECK(!key.IsNull());
    if (!V8CallBoolean(properties->CreateDataProperty(
            context, i, V8String(info.GetIsolate(), key))))
      return;
  }

  V8SetReturnValue(info, properties);
}

void V8CSSStyleDeclaration::namedPropertyQueryCustom(
    const AtomicString& name,
    const v8::PropertyCallbackInfo<v8::Integer>& info) {
  // NOTE: cssPropertyInfo lookups incur several mallocs.
  // Successful lookups have the same cost the first time, but are cached.
  if (CssPropertyInfo(name)) {
    V8SetReturnValueInt(info, 0);
    return;
  }
}

void V8CSSStyleDeclaration::namedPropertyGetterCustom(
    const AtomicString& name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  // Search the style declaration.
  CSSPropertyID unresolved_property = CssPropertyInfo(name);

  // Do not handle non-property names.
  if (!unresolved_property)
    return;
  CSSPropertyID resolved_property = resolveCSSPropertyID(unresolved_property);

  CSSStyleDeclaration* impl = V8CSSStyleDeclaration::toImpl(info.Holder());
  const CSSValue* css_value =
      impl->GetPropertyCSSValueInternal(resolved_property);
  if (css_value) {
    V8SetReturnValueStringOrNull(info, css_value->CssText(), info.GetIsolate());
    return;
  }

  String result = impl->GetPropertyValueInternal(resolved_property);
  V8SetReturnValueString(info, result, info.GetIsolate());
}

void V8CSSStyleDeclaration::namedPropertySetterCustom(
    const AtomicString& name,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  CSSStyleDeclaration* impl = V8CSSStyleDeclaration::toImpl(info.Holder());
  CSSPropertyID unresolved_property = CssPropertyInfo(name);
  if (!unresolved_property)
    return;

  CEReactionsScope ce_reactions_scope;

  TOSTRING_VOID(V8StringResource<kTreatNullAsNullString>, property_value,
                value);
  ExceptionState exception_state(
      info.GetIsolate(), ExceptionState::kSetterContext, "CSSStyleDeclaration",
      getPropertyName(resolveCSSPropertyID(unresolved_property)));
  impl->SetPropertyInternal(unresolved_property, String(), property_value,
                            false, exception_state);

  V8SetReturnValue(info, value);
}

}  // namespace blink
