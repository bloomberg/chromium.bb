// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Suborigin.h"

#include "core/dom/Document.h"
#include "core/inspector/ConsoleMessage.h"
#include "platform/ParsingUtilities.h"
#include "wtf/ASCIICType.h"

namespace blink {

template<typename CharType> static inline bool isASCIIAlphanumericOrHyphen(CharType c)
{
    return isASCIIAlphanumeric(c) || c == '-';
}

void SuboriginPolicy::logSuboriginHeaderError(Document& document, const String& message)
{
    document.addConsoleMessage(ConsoleMessage::create(SecurityMessageSource, ErrorMessageLevel, "Error with Suborigin header: " + message));
}

String SuboriginPolicy::parseSuboriginName(Document& document, const String& header)
{
    Vector<String> headers;
    header.split(',', true, headers);

    if (headers.size() > 1)
        logSuboriginHeaderError(document, "Multiple Suborigin headers found. Ignoring all but the first.");

    Vector<UChar> characters;
    headers[0].appendTo(characters);

    const UChar* position = characters.data();
    const UChar* end = position + characters.size();

    // Parse the name of the suborigin (no spaces, single string)
    skipWhile<UChar, isASCIISpace>(position, end);
    if (position == end) {
        logSuboriginHeaderError(document, "No Suborigin name specified.");
        return String();
    }

    const UChar* begin = position;

    skipWhile<UChar, isASCIIAlphanumericOrHyphen>(position, end);
    if (position != end && !isASCIISpace(*position)) {
        logSuboriginHeaderError(document, "Invalid character \'" + String(position, 1) + "\' in suborigin.");
        return String();
    }
    size_t length = position - begin;
    skipWhile<UChar, isASCIISpace>(position, end);
    if (position != end) {
        logSuboriginHeaderError(document, "Whitespace is not allowed in suborigin names.");
        return String();
    }

    return String(begin, length);
}

} // namespace blink
