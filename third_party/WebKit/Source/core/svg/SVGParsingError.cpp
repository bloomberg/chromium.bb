// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/svg/SVGParsingError.h"

#include "core/dom/QualifiedName.h"
#include "platform/JSONValues.h"
#include "wtf/text/CharacterNames.h"
#include "wtf/text/StringBuilder.h"

#include <utility>

namespace blink {

namespace {

void appendErrorContextInfo(StringBuilder& builder, const String& tagName, const QualifiedName& name)
{
    builder.append('<');
    builder.append(tagName);
    builder.appendLiteral("> attribute ");
    builder.append(name.toString());
}

std::pair<const char*, const char*> messageForStatus(SVGParseStatus status)
{
    switch (status) {
    case SVGParseStatus::TrailingGarbage:
        return std::make_pair("Trailing garbage, ", ".");
    case SVGParseStatus::ExpectedBoolean:
        return std::make_pair("Expected 'true' or 'false', ", ".");
    case SVGParseStatus::ExpectedEnumeration:
        return std::make_pair("Unrecognized enumerated value, ", ".");
    case SVGParseStatus::ExpectedNumber:
        return std::make_pair("Expected number, ", ".");
    case SVGParseStatus::NegativeValue:
        return std::make_pair("A negative value is not valid. (", ")");
    case SVGParseStatus::ParsingFailed:
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return std::make_pair("", "");
}

bool disableLocus(SVGParseStatus status)
{
    // Disable locus for semantic errors and generic errors (see TODO below).
    return status == SVGParseStatus::NegativeValue
        || status == SVGParseStatus::ParsingFailed;
}

void appendValue(StringBuilder& builder, SVGParsingError error, const AtomicString& value)
{
    builder.append('"');
    if (!error.hasLocus() || disableLocus(error.status())) {
        escapeStringForJSON(value.string(), &builder);
    } else {
        // Emit a string on the form: '"[...]<before><after>[...]"'
        unsigned locus = error.locus();
        ASSERT(locus <= value.length());

        // Amount of context to show before/after the error.
        const unsigned kContext = 16;

        unsigned contextStart = std::max(locus, kContext) - kContext;
        unsigned contextEnd = std::min(locus + kContext, value.length());
        ASSERT(contextStart <= contextEnd);
        ASSERT(contextEnd <= value.length());
        if (contextStart != 0)
            builder.append(horizontalEllipsisCharacter);
        escapeStringForJSON(value.string().substring(contextStart, contextEnd - contextStart), &builder);
        if (contextEnd != value.length())
            builder.append(horizontalEllipsisCharacter);
    }
    builder.append('"');
}

} // namespace

String SVGParsingError::format(const String& tagName, const QualifiedName& name, const AtomicString& value) const
{
    StringBuilder builder;

    // TODO(fs): Remove this case once enough specific errors have been added.
    if (status() == SVGParseStatus::ParsingFailed) {
        builder.appendLiteral("Invalid value for ");
        appendErrorContextInfo(builder, tagName, name);
        builder.append('=');
        appendValue(builder, *this, value);
    } else {
        appendErrorContextInfo(builder, tagName, name);
        builder.appendLiteral(": ");

        if (hasLocus() && locus() == value.length())
            builder.appendLiteral("Unexpected end of attribute. ");

        auto message = messageForStatus(status());
        builder.append(message.first);
        appendValue(builder, *this, value);
        builder.append(message.second);
    }
    return builder.toString();
}

} // namespace blink
