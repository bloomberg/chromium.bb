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
#include "weborigin/DatabaseIdentifier.h"

#include "weborigin/KURL.h"
#include "weborigin/SecurityOrigin.h"
#include "wtf/testing/WTFTestHelpers.h"

#include <gtest/gtest.h>

using WebCore::SecurityOrigin;
using WebCore::createDatabaseIdentifierFromSecurityOrigin;
using WebCore::createSecurityOriginFromDatabaseIdentifier;
using WebCore::encodeWithURLEscapeSequences;

namespace {

TEST(DatabaseIdentifierTest, CreateIdentifierFromSecurityOrigin)
{
    struct OriginTestCase {
        String protocol;
        String host;
        int port;
        String expectedIdentifier;
    } cases[] = {
        {"http", "google.com", 80, "http_google.com_0"},
        {"https", "www.google.com", 443, "https_www.google.com_0"},
        {"http", "foo_bar_baz.org", 80, "http_foo_bar_baz.org_0"},
        {"http", "nondefaultport.net", 8001, "http_nondefaultport.net_8001"},
        {"http", "invalidportnumber.org", 70000, "__0"},
        {"http", "invalidportnumber.org", -5, "__0"},
        {"http", "%E2%98%83.unicode.com", 80, "http_xn--n3h.unicode.com_0"},
        {"http", String::fromUTF8("\xe2\x98\x83.unicode.com"), 80, "http_xn--n3h.unicode.com_0"},
        {"http", String::fromUTF8("\xf0\x9f\x92\xa9.unicode.com"), 80, "http_xn--ls8h.unicode.com_0"},
        {"file", "", 0, "file__0"},
        {"data", "", 0, "__0"},
        {"about", "blank", 0, "__0"},
    };

    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
        RefPtr<SecurityOrigin> origin = SecurityOrigin::create(cases[i].protocol, cases[i].host, cases[i].port);
        String identifier = createDatabaseIdentifierFromSecurityOrigin(origin.get());
        EXPECT_EQ(cases[i].expectedIdentifier, identifier) << "test case " << i;
    }
}

TEST(DatabaseIdentifierTest, CreateSecurityOriginFromIdentifier)
{
    struct IdentifierTestCase {
        String identifier;
        String expectedProtocol;
        String expectedHost;
        int expectedPort;
        String expectedStringRepresentation;
        bool expectedUnique;
    };

    IdentifierTestCase validCases[] = {
        {"http_google.com_0", "http", "google.com", 0, "http://google.com", false},
        {"https_google.com_0", "https", "google.com", 0, "https://google.com", false},
        {"ftp_google.com_0", "ftp", "google.com", 0, "ftp://google.com", false},
        {"unknown_google.com_0", "unknown", "", 0, "unknown://", false},
        {"http_nondefaultport.net_8001", "http", "nondefaultport.net", 8001, "http://nondefaultport.net:8001", false},
        {"file__0", "", "", 0, "null", true},
        {"__0", "", "", 0, "null", true},
        {"http_foo_bar_baz.org_0", "http", "foo_bar_baz.org", 0, "http://foo_bar_baz.org", false},
        {"http_xn--n3h.unicode.com_0", "http", "xn--n3h.unicode.com", 0, "http://xn--n3h.unicode.com", false},
    };

    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(validCases); ++i) {
        RefPtr<SecurityOrigin> origin = createSecurityOriginFromDatabaseIdentifier(validCases[i].identifier);
        EXPECT_EQ(validCases[i].expectedProtocol, origin->protocol()) << "test case " << i;
        EXPECT_EQ(validCases[i].expectedHost, origin->host()) << "test case " << i;
        EXPECT_EQ(validCases[i].expectedPort, origin->port()) << "test case " << i;
        EXPECT_EQ(validCases[i].expectedStringRepresentation, origin->toString()) << "test case " << i;
        EXPECT_EQ(validCases[i].expectedUnique, origin->isUnique()) << "test case " << i;
    }

    String bogusIdentifiers[] = {
        "", "_", "__",
        String("\x00", 1),
        String("http_\x00_0", 8),
        "ht\x7ctp_badprotocol.com_0",
        "http_unescaped_percent_%.com_0",
        "http_port_too_big.net_75000",
        "http_port_too_small.net_-25",
        "http_shouldbeescaped\x7c.com_0",
        "http_latin1\x8a.org_8001",
        String::fromUTF8("http_\xe2\x98\x83.unicode.com_0"),
    };

    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(bogusIdentifiers); ++i) {
        RefPtr<SecurityOrigin> origin = createSecurityOriginFromDatabaseIdentifier(bogusIdentifiers[i]);
        EXPECT_EQ("null", origin->toString()) << "test case " << i;
        EXPECT_EQ(true, origin->isUnique()) << "test case " << i;
    }
}

} // namespace

