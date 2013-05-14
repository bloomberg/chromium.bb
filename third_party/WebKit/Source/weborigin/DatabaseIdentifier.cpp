/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "weborigin/DatabaseIdentifier.h"

#include "weborigin/KURL.h"
#include "weborigin/KnownPorts.h"
#include "weborigin/SchemeRegistry.h"
#include "weborigin/SecurityOriginCache.h"
#include "weborigin/SecurityPolicy.h"
#include "wtf/HexNumber.h"
#include "wtf/MainThread.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

const int maxAllowedPort = 65535;

static const char separatorCharacter = '_';

// The following lower-ASCII characters need escaping to be used in a filename
// across all systems, including Windows:
//     - Unprintable ASCII (00-1F)
//     - Space             (20)
//     - Double quote      (22)
//     - Percent           (25) (escaped because it is our escape character)
//     - Asterisk          (2A)
//     - Slash             (2F)
//     - Colon             (3A)
//     - Less-than         (3C)
//     - Greater-than      (3E)
//     - Question Mark     (3F)
//     - Backslash         (5C)
//     - Pipe              (7C)
//     - Delete            (7F)

static const bool needsEscaping[128] = {
    /* 00-07 */ true,  true,  true,  true,  true,  true,  true,  true,
    /* 08-0F */ true,  true,  true,  true,  true,  true,  true,  true,

    /* 10-17 */ true,  true,  true,  true,  true,  true,  true,  true,
    /* 18-1F */ true,  true,  true,  true,  true,  true,  true,  true,

    /* 20-27 */ true,  false, true,  false, false, true,  false, false,
    /* 28-2F */ false, false, true,  false, false, false, false, true,

    /* 30-37 */ false, false, false, false, false, false, false, false,
    /* 38-3F */ false, false, true,  false, true,  false, true,  true,

    /* 40-47 */ false, false, false, false, false, false, false, false,
    /* 48-4F */ false, false, false, false, false, false, false, false,

    /* 50-57 */ false, false, false, false, false, false, false, false,
    /* 58-5F */ false, false, false, false, true,  false, false, false,

    /* 60-67 */ false, false, false, false, false, false, false, false,
    /* 68-6F */ false, false, false, false, false, false, false, false,

    /* 70-77 */ false, false, false, false, false, false, false, false,
    /* 78-7F */ false, false, false, false, true,  false, false, true,
};

static inline bool shouldEscapeUChar(UChar c)
{
    return c > 127 ? false : needsEscaping[c];
}

// FIXME: Move this function to another compilation unit.
static String encodeForFileName(const String& inputStr)
{
    unsigned length = inputStr.length();
    Vector<UChar, 512> buffer(length * 3 + 1);
    UChar* p = buffer.data();

    const UChar* str = inputStr.characters();
    const UChar* strEnd = str + length;

    while (str < strEnd) {
        UChar c = *str++;
        if (shouldEscapeUChar(c)) {
            *p++ = '%';
            placeByteAsHex(c, p);
        } else
            *p++ = c;
    }

    ASSERT(p - buffer.data() <= static_cast<int>(buffer.size()));

    return String(buffer.data(), p - buffer.data());
}

PassRefPtr<SecurityOrigin> createSecurityOriginFromDatabaseIdentifier(const String& databaseIdentifier)
{
    // Make sure there's a first separator
    size_t separator1 = databaseIdentifier.find(separatorCharacter);
    if (separator1 == notFound)
        return SecurityOrigin::createUnique();

    // Make sure there's a second separator
    size_t separator2 = databaseIdentifier.reverseFind(separatorCharacter);
    if (separator2 == notFound)
        return SecurityOrigin::createUnique();

    // Ensure there were at least 2 separator characters. Some hostnames on intranets have
    // underscores in them, so we'll assume that any additional underscores are part of the host.
    if (separator1 == separator2)
        return SecurityOrigin::createUnique();

    // Make sure the port section is a valid port number or doesn't exist
    bool portOkay;
    int port = databaseIdentifier.right(databaseIdentifier.length() - separator2 - 1).toInt(&portOkay);
    bool portAbsent = (separator2 == databaseIdentifier.length() - 1);
    if (!(portOkay || portAbsent))
        return SecurityOrigin::createUnique();

    if (port < 0 || port > maxAllowedPort)
        return SecurityOrigin::createUnique();

    // Split out the 3 sections of data
    String protocol = databaseIdentifier.substring(0, separator1);
    String host = databaseIdentifier.substring(separator1 + 1, separator2 - separator1 - 1);

    host = decodeURLEscapeSequences(host);
    return SecurityOrigin::create(KURL(KURL(), protocol + "://" + host + ":" + String::number(port) + "/"));
}

String createDatabaseIdentifierFromSecurityOrigin(const SecurityOrigin* securityOrigin)
{
    // Historically, we've used the following (somewhat non-sensical) string
    // for the databaseIdentifier of local files. We used to compute this
    // string because of a bug in how we handled the scheme for file URLs.
    // Now that we've fixed that bug, we still need to produce this string
    // to avoid breaking existing persistent state.
    if (securityOrigin->needsDatabaseIdentifierQuirkForFiles())
        return "file__0";

    String separatorString(&separatorCharacter, 1);

    return securityOrigin->protocol() + separatorString + encodeForFileName(securityOrigin->host()) + separatorString + String::number(securityOrigin->port());
}

} // namespace WebCore
