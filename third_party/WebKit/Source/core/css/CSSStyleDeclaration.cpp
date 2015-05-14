// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/CSSStyleDeclaration.h"

#include "bindings/core/v8/V8Binding.h"
#include "core/css/CSSPropertyMetadata.h"
#include "core/css/parser/CSSParser.h"
#include "core/frame/UseCounter.h"
#include "wtf/text/WTFString.h"

using namespace WTF;

namespace blink {

// Check for a CSS prefix.
// Passed prefix is all lowercase.
// First character of the prefix within the property name may be upper or lowercase.
// Other characters in the prefix within the property name must be lowercase.
// The prefix within the property name must be followed by a capital letter.
static bool hasCSSPropertyNamePrefix(const String& propertyName, const char* prefix)
{
#if ENABLE(ASSERT)
    ASSERT(*prefix);
    for (const char* p = prefix; *p; ++p)
        ASSERT(isASCIILower(*p));
    ASSERT(propertyName.length());
#endif

    if (toASCIILower(propertyName[0]) != prefix[0])
        return false;

    unsigned length = propertyName.length();
    for (unsigned i = 1; i < length; ++i) {
        if (!prefix[i])
            return isASCIIUpper(propertyName[i]);
        if (propertyName[i] != prefix[i])
            return false;
    }
    return false;
}

static CSSPropertyID parseCSSPropertyID(v8::Isolate* isolate, const String& propertyName)
{
    unsigned length = propertyName.length();
    if (!length)
        return CSSPropertyInvalid;

    StringBuilder builder;
    builder.reserveCapacity(length);

    unsigned i = 0;
    bool hasSeenDash = false;

    if (hasCSSPropertyNamePrefix(propertyName, "css")) {
        i += 3;
        // getComputedStyle(elem).cssX is a non-standard behaviour
        // Measure this behaviour as CSSXGetComputedStyleQueries.
        UseCounter::countIfNotPrivateScript(isolate, callingExecutionContext(isolate), UseCounter::CSSXGetComputedStyleQueries);
    } else if (hasCSSPropertyNamePrefix(propertyName, "webkit")) {
        builder.append('-');
    } else if (isASCIIUpper(propertyName[0])) {
        return CSSPropertyInvalid;
    }

    bool hasSeenUpper = isASCIIUpper(propertyName[i]);

    builder.append(toASCIILower(propertyName[i++]));

    for (; i < length; ++i) {
        UChar c = propertyName[i];
        if (!isASCIIUpper(c)) {
            if (c == '-')
                hasSeenDash = true;
            builder.append(c);
        } else {
            hasSeenUpper = true;
            builder.append('-');
            builder.append(toASCIILower(c));
        }
    }

    // Reject names containing both dashes and upper-case characters, such as "border-rightColor".
    if (hasSeenDash && hasSeenUpper)
        return CSSPropertyInvalid;

    String propName = builder.toString();
    return unresolvedCSSPropertyID(propName);
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
static CSSPropertyID cssPropertyInfo(const String& propertyName, v8::Isolate* isolate)
{
    typedef HashMap<String, CSSPropertyID> CSSPropertyIDMap;
    DEFINE_STATIC_LOCAL(CSSPropertyIDMap, map, ());
    CSSPropertyIDMap::iterator iter = map.find(propertyName);
    if (iter != map.end())
        return iter->value;

    CSSPropertyID unresolvedProperty = parseCSSPropertyID(isolate, propertyName);
    map.add(propertyName, unresolvedProperty);
    ASSERT(!unresolvedProperty || CSSPropertyMetadata::isEnabledProperty(unresolvedProperty));
    return unresolvedProperty;
}

bool CSSStyleDeclaration::anonymousNamedSetter(ScriptState* scriptState, const String& name, const String& propertyValue, ExceptionState& exceptionState)
{
    CSSPropertyID unresolvedProperty = cssPropertyInfo(name, scriptState->isolate());
    if (!unresolvedProperty)
        return false;

    setPropertyInternal(unresolvedProperty, propertyValue, false, exceptionState);
    if (exceptionState.throwIfNeeded())
        return false;
    return true;
}

void CSSStyleDeclaration::namedPropertyEnumerator(Vector<String>& names, ExceptionState&)
{
    typedef Vector<String, numCSSProperties - 1> PreAllocatedPropertyVector;
    DEFINE_STATIC_LOCAL(PreAllocatedPropertyVector, propertyNames, ());
    static unsigned propertyNamesLength = 0;
    if (propertyNames.isEmpty()) {
        for (int id = firstCSSProperty; id <= lastCSSProperty; ++id) {
            CSSPropertyID propertyId = static_cast<CSSPropertyID>(id);
            if (CSSPropertyMetadata::isEnabledProperty(propertyId))
                propertyNames.append(getJSPropertyName(propertyId));
        }
        std::sort(propertyNames.begin(), propertyNames.end(), codePointCompareLessThan);
        propertyNamesLength = propertyNames.size();
    }
    for (unsigned i = 0; i < propertyNamesLength; ++i) {
        String key = propertyNames.at(i);
        ASSERT(!key.isNull());
        names.append(key);
    }
}

}
