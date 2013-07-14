/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2009 Michelangelo De Simone <micdesim@gmail.com>
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "core/html/EmailInputType.h"

#include "core/html/HTMLInputElement.h"
#include "core/html/InputTypeNames.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/platform/LocalizedStrings.h"
#include "core/platform/text/RegularExpression.h"
#include "public/platform/Platform.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/StringBuilder.h"
#include <unicode/uidna.h>

namespace WebCore {

static const char emailPattern[] =
    "[a-z0-9!#$%&'*+/=?^_`{|}~.-]+" // local part
    "@"
    "[a-z0-9-]+(\\.[a-z0-9-]+)*"; // domain part

// RFC5321 says the maximum total length of a domain name is 255 octets.
static const size_t maximumDomainNameLength = 255;
static const int32_t idnaConversionOption = UIDNA_ALLOW_UNASSIGNED;

static String convertEmailAddressToASCII(const String& address)
{
    if (address.containsOnlyASCII())
        return address;

    size_t atPosition = address.find('@');
    if (atPosition == notFound)
        return address;

    UErrorCode error = U_ZERO_ERROR;
    UChar domainNameBuffer[maximumDomainNameLength];
    int32_t domainNameLength = uidna_IDNToASCII(address.charactersWithNullTermination().data() + atPosition + 1, address.length() - atPosition - 1, domainNameBuffer, WTF_ARRAY_LENGTH(domainNameBuffer), idnaConversionOption, 0, &error);
    if (error != U_ZERO_ERROR || domainNameLength <= 0)
        return address;

    StringBuilder builder;
    builder.append(address, 0, atPosition + 1);
    builder.append(domainNameBuffer, domainNameLength);
    return builder.toString();
}

String EmailInputType::convertEmailAddressToUnicode(const String& address) const
{
    if (!address.containsOnlyASCII())
        return address;

    size_t atPosition = address.find('@');
    if (atPosition == notFound)
        return address;

    if (address.find("xn--", atPosition + 1) == notFound)
        return address;

    ChromeClient* chromeClient = chrome() ? chrome()->client() : 0;
    if (!chromeClient)
        return address;

    String languages = chromeClient->acceptLanguages();
    String unicodeHost = WebKit::Platform::current()->convertIDNToUnicode(address.substring(atPosition + 1), languages);
    StringBuilder builder;
    builder.append(address, 0, atPosition + 1);
    builder.append(unicodeHost);
    return builder.toString();
}

static bool isValidEmailAddress(const String& address)
{
    int addressLength = address.length();
    if (!addressLength)
        return false;

    DEFINE_STATIC_LOCAL(const RegularExpression, regExp, (emailPattern, TextCaseInsensitive));

    int matchLength;
    int matchOffset = regExp.match(address, 0, &matchLength);

    return !matchOffset && matchLength == addressLength;
}

PassOwnPtr<InputType> EmailInputType::create(HTMLInputElement* element)
{
    return adoptPtr(new EmailInputType(element));
}

void EmailInputType::attach()
{
    TextFieldInputType::attach();
    observeFeatureIfVisible(UseCounter::InputTypeEmail);
}

const AtomicString& EmailInputType::formControlType() const
{
    return InputTypeNames::email();
}

bool EmailInputType::typeMismatchFor(const String& value) const
{
    if (value.isEmpty())
        return false;
    if (!element()->multiple())
        return !isValidEmailAddress(value);
    Vector<String> addresses;
    value.split(',', true, addresses);
    for (unsigned i = 0; i < addresses.size(); ++i) {
        if (!isValidEmailAddress(stripLeadingAndTrailingHTMLSpaces(addresses[i])))
            return true;
    }
    return false;
}

bool EmailInputType::typeMismatch() const
{
    return typeMismatchFor(element()->value());
}

String EmailInputType::typeMismatchText() const
{
    return element()->multiple() ? validationMessageTypeMismatchForMultipleEmailText() : validationMessageTypeMismatchForEmailText();
}

bool EmailInputType::isEmailField() const
{
    return true;
}

bool EmailInputType::supportsSelectionAPI() const
{
    return false;
}

String EmailInputType::sanitizeValue(const String& proposedValue) const
{
    String noLineBreakValue = proposedValue.removeCharacters(isHTMLLineBreak);
    if (!element()->multiple())
        return stripLeadingAndTrailingHTMLSpaces(noLineBreakValue);
    Vector<String> addresses;
    noLineBreakValue.split(',', true, addresses);
    StringBuilder strippedValue;
    for (size_t i = 0; i < addresses.size(); ++i) {
        if (i > 0)
            strippedValue.append(",");
        strippedValue.append(stripLeadingAndTrailingHTMLSpaces(addresses[i]));
    }
    return strippedValue.toString();
}

String EmailInputType::convertFromVisibleValue(const String& visibleValue) const
{
    String sanitizedValue = sanitizeValue(visibleValue);
    if (!element()->multiple())
        return convertEmailAddressToASCII(sanitizedValue);
    Vector<String> addresses;
    sanitizedValue.split(',', true, addresses);
    StringBuilder builder;
    builder.reserveCapacity(sanitizedValue.length());
    for (size_t i = 0; i < addresses.size(); ++i) {
        if (i > 0)
            builder.append(",");
        builder.append(convertEmailAddressToASCII(addresses[i]));
    }
    return builder.toString();
}

String EmailInputType::visibleValue() const
{
    String value = element()->value();
    if (!element()->multiple())
        return convertEmailAddressToUnicode(value);

    Vector<String> addresses;
    value.split(',', true, addresses);
    StringBuilder builder;
    builder.reserveCapacity(value.length());
    for (size_t i = 0; i < addresses.size(); ++i) {
        if (i > 0)
            builder.append(",");
        builder.append(convertEmailAddressToUnicode(addresses[i]));
    }
    return builder.toString();
}

} // namespace WebCore
