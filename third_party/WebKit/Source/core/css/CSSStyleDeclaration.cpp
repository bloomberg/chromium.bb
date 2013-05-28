/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "config.h"
#include "core/css/CSSStyleDeclaration.h"

#include "core/css/CSSParser.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/CSSValue.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentStyleSheetCollection.h"
#include "core/dom/EventTarget.h"
#include "core/html/HTMLStyleElement.h"
#include "core/page/RuntimeCSSEnabled.h"

namespace WebCore {

// FIXME: Next two functions look lifted verbatim from JSCSSStyleDeclarationCustom. Please remove duplication.

// Check for a CSS prefix.
// Passed prefix is all lowercase.
// First character of the prefix within the property name may be upper or lowercase.
// Other characters in the prefix within the property name must be lowercase.
// The prefix within the property name must be followed by a capital letter.
static bool hasCSSPropertyNamePrefix(const String& propertyName, const char* prefix)
{
#ifndef NDEBUG
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

// When getting properties on CSSStyleDeclarations, the name used from
// Javascript and the actual name of the property are not the same, so
// we have to do the following translation. The translation turns upper
// case characters into lower case characters and inserts dashes to
// separate words.
//
// Example: 'backgroundPositionY' -> 'background-position-y'
//
// Also, certain prefixes such as 'pos', 'css-' and 'pixel-' are stripped
// and the hadPixelOrPosPrefix out parameter is used to indicate whether or
// not the property name was prefixed with 'pos-' or 'pixel-'.
CSSPropertyInfo* CSSStyleDeclaration::cssPropertyInfo(const String& propertyName)
{
    typedef HashMap<String, CSSPropertyInfo*> CSSPropertyInfoMap;
    DEFINE_STATIC_LOCAL(CSSPropertyInfoMap, map, ());
    CSSPropertyInfo* propInfo = map.get(propertyName);
    if (!propInfo) {
        unsigned length = propertyName.length();
        bool hadPixelOrPosPrefix = false;
        if (!length)
            return 0;

        StringBuilder builder;
        builder.reserveCapacity(length);

        unsigned i = 0;

        if (hasCSSPropertyNamePrefix(propertyName, "css"))
            i += 3;
        else if (hasCSSPropertyNamePrefix(propertyName, "pixel")) {
            i += 5;
            hadPixelOrPosPrefix = true;
        } else if (hasCSSPropertyNamePrefix(propertyName, "pos")) {
            i += 3;
            hadPixelOrPosPrefix = true;
        } else if (hasCSSPropertyNamePrefix(propertyName, "webkit"))
            builder.append('-');
        else if (isASCIIUpper(propertyName[0]))
            return 0;

        builder.append(toASCIILower(propertyName[i++]));

        for (; i < length; ++i) {
            UChar c = propertyName[i];
            if (!isASCIIUpper(c))
                builder.append(c);
            else {
                builder.append('-');
                builder.append(toASCIILower(c));
            }
        }

        String propName = builder.toString();
        CSSPropertyID propertyID = cssPropertyID(propName);
        if (propertyID && RuntimeCSSEnabled::isCSSPropertyEnabled(propertyID)) {
            propInfo = new CSSPropertyInfo();
            propInfo->hadPixelOrPosPrefix = hadPixelOrPosPrefix;
            propInfo->propID = propertyID;
            map.add(propertyName, propInfo);
        }
    }
    return propInfo;
}

void CSSStyleDeclaration::anonymousNamedGetter(const AtomicString& name, bool& returnValue1Enabled, String& returnValue1, bool& returnValue2Enabled, float& returnValue2)
{
    // Search the style declaration.
    CSSPropertyInfo* propInfo = cssPropertyInfo(name);

    // Do not handle non-property names.
    if (!propInfo)
        return;

    RefPtr<CSSValue> cssValue = getPropertyCSSValueInternal(static_cast<CSSPropertyID>(propInfo->propID));
    if (cssValue) {
        if (propInfo->hadPixelOrPosPrefix && cssValue->isPrimitiveValue()) {
            returnValue2Enabled = true;
            returnValue2 = static_cast<CSSPrimitiveValue*>(cssValue.get())->getFloatValue(CSSPrimitiveValue::CSS_PX);
            return;
        }
        returnValue1Enabled = true;
        returnValue1 = cssValue->cssText();
        return;
    }

    String result = getPropertyValueInternal(static_cast<CSSPropertyID>(propInfo->propID));
    if (result.isNull())
        result = ""; // convert null to empty string.

    returnValue1 = result;
    returnValue1Enabled = true;
}

bool CSSStyleDeclaration::anonymousNamedSetter(const AtomicString& propertyName, const String& value, ExceptionCode& ec)
{
    String propertyValue = value;
    CSSPropertyInfo* propInfo = CSSStyleDeclaration::cssPropertyInfo(propertyName);
    if (!propInfo)
        return false;

    if (propInfo->hadPixelOrPosPrefix)
        propertyValue.append("px");

    this->setPropertyInternal(static_cast<CSSPropertyID>(propInfo->propID), propertyValue, false, ec);

    return true;
}

} // namespace WebCore
