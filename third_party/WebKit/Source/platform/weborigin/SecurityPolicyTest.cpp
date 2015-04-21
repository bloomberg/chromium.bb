/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"

#include "platform/weborigin/KURL.h"
#include <gtest/gtest.h>

using blink::KURL;
using blink::SecurityOrigin;
using blink::SecurityPolicy;

namespace {

TEST(SecurityPolicyTest, ReferrerIsAlwaysAWebURL)
{
    EXPECT_TRUE(String() == SecurityPolicy::generateReferrer(blink::ReferrerPolicyAlways, KURL(blink::ParsedURLString, "http://example.com/"), String::fromUTF8("chrome://somepage/")).referrer);
}

TEST(SecurityPolicyTest, GenerateReferrer)
{
    struct TestCase {
        blink::ReferrerPolicy policy;
        const char* referrer;
        const char* destination;
        const char* expected;
    };

    const char insecureURLA[] = "http://a.test/path/to/file.html";
    const char insecureURLB[] = "http://b.test/path/to/file.html";
    const char insecureOriginA[] = "http://a.test/";

    const char secureURLA[] = "https://a.test/path/to/file.html";
    const char secureURLB[] = "https://b.test/path/to/file.html";
    const char secureOriginA[] = "https://a.test/";

    const char blobURL[] = "blob:http%3A//a.test/b3aae9c8-7f90-440d-8d7c-43aa20d72fde";
    const char filesystemURL[] = "filesystem:http%3A//a.test/path/t/file.html";

    TestCase inputs[] = {
        // HTTP -> HTTP: Same Origin
        { blink::ReferrerPolicyAlways, insecureURLA, insecureURLA, insecureURLA },
        { blink::ReferrerPolicyDefault, insecureURLA, insecureURLA, insecureURLA },
        { blink::ReferrerPolicyNoReferrerWhenDowngrade, insecureURLA, insecureURLA, insecureURLA },
        { blink::ReferrerPolicyNever, insecureURLA, insecureURLA, 0 },
        { blink::ReferrerPolicyOrigin, insecureURLA, insecureURLA, insecureOriginA },
        { blink::ReferrerPolicyOriginWhenCrossOrigin, insecureURLA, insecureURLA, insecureURLA },

        // HTTP -> HTTP: Cross Origin
        { blink::ReferrerPolicyAlways, insecureURLA, insecureURLB, insecureURLA },
        { blink::ReferrerPolicyDefault, insecureURLA, insecureURLB, insecureURLA },
        { blink::ReferrerPolicyNoReferrerWhenDowngrade, insecureURLA, insecureURLB, insecureURLA },
        { blink::ReferrerPolicyNever, insecureURLA, insecureURLB, 0 },
        { blink::ReferrerPolicyOrigin, insecureURLA, insecureURLB, insecureOriginA },
        { blink::ReferrerPolicyOriginWhenCrossOrigin, insecureURLA, insecureURLB, insecureOriginA },

        // HTTPS -> HTTPS: Same Origin
        { blink::ReferrerPolicyAlways, secureURLA, secureURLA, secureURLA },
        { blink::ReferrerPolicyDefault, secureURLA, secureURLA, secureURLA },
        { blink::ReferrerPolicyNoReferrerWhenDowngrade, secureURLA, secureURLA, secureURLA },
        { blink::ReferrerPolicyNever, secureURLA, secureURLA, 0 },
        { blink::ReferrerPolicyOrigin, secureURLA, secureURLA, secureOriginA },
        { blink::ReferrerPolicyOriginWhenCrossOrigin, secureURLA, secureURLA, secureURLA },

        // HTTPS -> HTTPS: Cross Origin
        { blink::ReferrerPolicyAlways, secureURLA, secureURLB, secureURLA },
        { blink::ReferrerPolicyDefault, secureURLA, secureURLB, secureURLA },
        { blink::ReferrerPolicyNoReferrerWhenDowngrade, secureURLA, secureURLB, secureURLA },
        { blink::ReferrerPolicyNever, secureURLA, secureURLB, 0 },
        { blink::ReferrerPolicyOrigin, secureURLA, secureURLB, secureOriginA },
        { blink::ReferrerPolicyOriginWhenCrossOrigin, secureURLA, secureURLB, secureOriginA },

        // HTTP -> HTTPS
        { blink::ReferrerPolicyAlways, insecureURLA, secureURLB, insecureURLA },
        { blink::ReferrerPolicyDefault, insecureURLA, secureURLB, insecureURLA },
        { blink::ReferrerPolicyNoReferrerWhenDowngrade, insecureURLA, secureURLB, insecureURLA },
        { blink::ReferrerPolicyNever, insecureURLA, secureURLB, 0 },
        { blink::ReferrerPolicyOrigin, insecureURLA, secureURLB, insecureOriginA },
        { blink::ReferrerPolicyOriginWhenCrossOrigin, insecureURLA, secureURLB, insecureOriginA },

        // HTTPS -> HTTP
        { blink::ReferrerPolicyAlways, secureURLA, insecureURLB, secureURLA },
        { blink::ReferrerPolicyDefault, secureURLA, insecureURLB, 0 },
        { blink::ReferrerPolicyNoReferrerWhenDowngrade, secureURLA, insecureURLB, 0 },
        { blink::ReferrerPolicyNever, secureURLA, insecureURLB, 0 },
        { blink::ReferrerPolicyOrigin, secureURLA, insecureURLB, secureOriginA },
        { blink::ReferrerPolicyOriginWhenCrossOrigin, secureURLA, secureURLB, secureOriginA },

        // blob and filesystem URL handling
        { blink::ReferrerPolicyAlways, insecureURLA, blobURL, 0 },
        { blink::ReferrerPolicyAlways, blobURL, insecureURLA, 0 },
        { blink::ReferrerPolicyAlways, insecureURLA, filesystemURL, 0 },
        { blink::ReferrerPolicyAlways, filesystemURL, insecureURLA, 0 },
    };

    for (TestCase test : inputs) {
        KURL destination(blink::ParsedURLString, test.destination);
        blink::Referrer result = SecurityPolicy::generateReferrer(test.policy, destination, String::fromUTF8(test.referrer));
        if (test.expected) {
            EXPECT_EQ(String::fromUTF8(test.expected), result.referrer)
                << "'" << test.referrer << "' to '" << test.destination
                << "' should have been '" << test.expected << "': was '"
                << result.referrer.utf8().data() << "'.";
        } else {
            EXPECT_TRUE(result.referrer.isEmpty())
                << "'" << test.referrer << "' to '" << test.destination
                << "' should have been empty: was '" << result.referrer.utf8().data() << "'.";
        }
        EXPECT_EQ(test.policy, result.referrerPolicy);
    }
}

TEST(SecurityPolicyTest, TrustworthyWhiteList)
{
    const char* insecureURLs[] = {
        "http://a.test/path/to/file.html",
        "http://b.test/path/to/file.html",
        "blob:http://c.test/b3aae9c8-7f90-440d-8d7c-43aa20d72fde",
        "filesystem:http://d.test/path/t/file.html",
    };

    for (const char* url : insecureURLs) {
        String errorMessage;
        RefPtr<SecurityOrigin> origin = SecurityOrigin::createFromString(url);
        EXPECT_FALSE(origin->isPotentiallyTrustworthy(errorMessage));
        SecurityPolicy::addOriginTrustworthyWhiteList(origin);
        EXPECT_TRUE(origin->isPotentiallyTrustworthy(errorMessage));
    }

    // Tests that adding URLs that have inner-urls to the whitelist
    // takes effect on the origins of the inner-urls (and vice versa).
    struct TestCase {
        const char* url;
        const char* anotherUrlInOrigin;
    };
    TestCase insecureURLsWithInnerOrigin[] = {
        {
            "blob:http://e.test/b3aae9c8-7f90-440d-8d7c-43aa20d72fde",
            "http://e.test/foo.html"
        }, {
            "filesystem:http://f.test/path/t/file.html",
            "http://f.test/bar.html"
        }, {
            "http://g.test/foo.html",
            "blob:http://g.test/b3aae9c8-7f90-440d-8d7c-43aa20d72fde"
        }, {
            "http://h.test/bar.html",
            "filesystem:http://h.test/path/t/file.html"
        },
    };
    for (const TestCase& test : insecureURLsWithInnerOrigin) {
        String errorMessage;

        // Actually origins of both URLs should be same.
        RefPtr<SecurityOrigin> origin1 = SecurityOrigin::createFromString(test.url);
        RefPtr<SecurityOrigin> origin2 = SecurityOrigin::createFromString(test.anotherUrlInOrigin);

        EXPECT_FALSE(origin1->isPotentiallyTrustworthy(errorMessage));
        EXPECT_FALSE(origin2->isPotentiallyTrustworthy(errorMessage));
        SecurityPolicy::addOriginTrustworthyWhiteList(origin1);
        EXPECT_TRUE(origin1->isPotentiallyTrustworthy(errorMessage));
        EXPECT_TRUE(origin2->isPotentiallyTrustworthy(errorMessage));
    }
}

} // namespace
