// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/frame/SubresourceIntegrity.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/html/HTMLScriptElement.h"
#include "platform/Crypto.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"
#include <gtest/gtest.h>

namespace blink {

static const char kBasicScript[] = "alert('test');";
static const char kSha256Integrity[] = "ni:///sha256;GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4=";
static const char kSha384Integrity[] = "ni:///sha384;nep3XpvhUxpCMOVXIFPecThAqdY/uVeiD4kXSqXpx0YJUWU4fTTaFgciTuZk7fmE";
static const char kSha512Integrity[] = "ni:///sha512;TXkJw18PqlVlEUXXjeXbGetop1TKB3wYQIp1/ihxCOFGUfG9TYOaA1MlkpTAqSV6yaevLO8Tj5pgH1JmZ++ItA==";
static const char kSha384IntegrityLabeledAs256[] = "ni:///sha256;nep3XpvhUxpCMOVXIFPecThAqdY/uVeiD4kXSqXpx0YJUWU4fTTaFgciTuZk7fmE";
static const char kUnsupportedHashFunctionIntegrity[] = "ni:///sha1;JfLW308qMPKfb4DaHpUBEESwuPc=";

class SubresourceIntegrityTest : public ::testing::Test {
public:
    SubresourceIntegrityTest()
        : secureURL(ParsedURLString, "https://example.test:443")
        , insecureURL(ParsedURLString, "http://example.test:80")
        , secureOrigin(SecurityOrigin::create(secureURL))
        , insecureOrigin(SecurityOrigin::create(insecureURL))
    {
    }

protected:
    virtual void SetUp()
    {
        document = Document::create();
        scriptElement = HTMLScriptElement::create(*document, true);
    }

    void expectAlgorithm(const String& text, HashAlgorithm expectedAlgorithm)
    {
        Vector<UChar> characters;
        text.appendTo(characters);
        const UChar* position = characters.data();
        const UChar* end = characters.end();
        HashAlgorithm algorithm;

        EXPECT_TRUE(SubresourceIntegrity::parseAlgorithm(position, end, algorithm));
        EXPECT_EQ(expectedAlgorithm, algorithm);
        EXPECT_EQ(';', *position);
    }

    void expectAlgorithmFailure(const String& text)
    {
        Vector<UChar> characters;
        text.appendTo(characters);
        const UChar* position = characters.data();
        const UChar* begin = characters.data();
        const UChar* end = characters.end();
        HashAlgorithm algorithm;

        EXPECT_FALSE(SubresourceIntegrity::parseAlgorithm(position, end, algorithm));
        EXPECT_EQ(begin, position);
    }

    void expectDigest(const String& text, const char* expectedDigest)
    {
        Vector<UChar> characters;
        text.appendTo(characters);
        const UChar* position = characters.data();
        const UChar* end = characters.end();
        String digest;

        EXPECT_TRUE(SubresourceIntegrity::parseDigest(position, end, digest));
        EXPECT_EQ(expectedDigest, digest);
    }

    void expectDigestFailure(const String& text)
    {
        Vector<UChar> characters;
        text.appendTo(characters);
        const UChar* position = characters.data();
        const UChar* end = characters.end();
        String digest;

        EXPECT_FALSE(SubresourceIntegrity::parseDigest(position, end, digest));
        EXPECT_TRUE(digest.isEmpty());
    }

    void expectParse(const char* integrityAttribute, const char* expectedDigest, HashAlgorithm expectedAlgorithm)
    {
        String digest;
        HashAlgorithm algorithm;

        EXPECT_TRUE(SubresourceIntegrity::parseIntegrityAttribute(integrityAttribute, digest, algorithm, *document));
        EXPECT_EQ(expectedDigest, digest);
        EXPECT_EQ(expectedAlgorithm, algorithm);
    }

    void expectParseFailure(const char* integrityAttribute)
    {
        String digest;
        HashAlgorithm algorithm;

        EXPECT_FALSE(SubresourceIntegrity::parseIntegrityAttribute(integrityAttribute, digest, algorithm, *document));
    }

    void expectIntegrity(const char* integrity, const char* script, const KURL& url)
    {
        scriptElement->setAttribute(HTMLNames::integrityAttr, integrity);
        EXPECT_TRUE(SubresourceIntegrity::CheckSubresourceIntegrity(*scriptElement, script, url));
    }

    void expectIntegrityFailure(const char* integrity, const char* script, const KURL& url)
    {
        scriptElement->setAttribute(HTMLNames::integrityAttr, integrity);
        EXPECT_FALSE(SubresourceIntegrity::CheckSubresourceIntegrity(*scriptElement, script, url));
    }

    KURL secureURL;
    KURL insecureURL;
    RefPtr<SecurityOrigin> secureOrigin;
    RefPtr<SecurityOrigin> insecureOrigin;

    RefPtrWillBePersistent<Document> document;
    RefPtrWillBePersistent<HTMLScriptElement> scriptElement;
};

TEST_F(SubresourceIntegrityTest, ParseAlgorithm)
{
    expectAlgorithm("sha256;", HashAlgorithmSha256);
    expectAlgorithm("sha384;", HashAlgorithmSha384);
    expectAlgorithm("sha512;", HashAlgorithmSha512);

    expectAlgorithmFailure("sha1;");
    expectAlgorithmFailure("sha-1;");
    expectAlgorithmFailure("sha-256;");
    expectAlgorithmFailure("sha-384;");
    expectAlgorithmFailure("sha-512;");
}

TEST_F(SubresourceIntegrityTest, ParseDigest)
{
    expectDigest("abcdefg", "abcdefg");
    expectDigest("abcdefg?", "abcdefg");

    expectDigestFailure("?");
    expectDigestFailure("&&&foobar&&&");
    expectDigestFailure("\x01\x02\x03\x04");
}

//
// End-to-end parsing tests.
//

TEST_F(SubresourceIntegrityTest, Parsing)
{
    expectParseFailure("");
    expectParseFailure("not/really/a/valid/anything");
    expectParseFailure("foobar:///sha256;abcdefg");
    expectParseFailure("ni://sha256;abcdefg");
    expectParseFailure("ni:///not-sha256-at-all;abcdefg");
    expectParseFailure("ni:///sha256;&&&foobar&&&");
    expectParseFailure("ni:///sha256;\x01\x02\x03\x04");

    expectParse(
        "ni:///sha256;BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
        "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
        HashAlgorithmSha256);

    expectParse(
        "     ni:///sha256;BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=     ",
        "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
        HashAlgorithmSha256);

    expectParse(
        "ni:///sha384;XVVXBGoYw6AJOh9J/Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr",
        "XVVXBGoYw6AJOh9J/Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr",
        HashAlgorithmSha384);

    expectParse(
        "ni:///sha512;tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
        "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
        HashAlgorithmSha512);
}

//
// End-to-end tests of ::CheckSubresourceIntegrity.
//

TEST_F(SubresourceIntegrityTest, CheckSubresourceIntegrityInSecureOrigin)
{
    document->updateSecurityOrigin(secureOrigin->isolatedCopy());

    // Verify basic sha256, sha384, and sha512 integrity checks.
    expectIntegrity(kSha256Integrity, kBasicScript, secureURL);
    expectIntegrity(kSha384Integrity, kBasicScript, secureURL);
    expectIntegrity(kSha512Integrity, kBasicScript, secureURL);

    // The hash label must match the hash value.
    expectIntegrityFailure(kSha384IntegrityLabeledAs256, kBasicScript, secureURL);

    // Unsupported hash functions should fail.
    expectIntegrityFailure(kUnsupportedHashFunctionIntegrity, kBasicScript, secureURL);
}

TEST_F(SubresourceIntegrityTest, CheckSubresourceIntegrityInInsecureOrigin)
{
    // The same checks as CheckSubresourceIntegrityInSecureOrigin should fail here.
    document->updateSecurityOrigin(insecureOrigin->isolatedCopy());

    expectIntegrityFailure(kSha256Integrity, kBasicScript, secureURL);
    expectIntegrityFailure(kSha384Integrity, kBasicScript, secureURL);
    expectIntegrityFailure(kSha512Integrity, kBasicScript, secureURL);
    expectIntegrityFailure(kSha384IntegrityLabeledAs256, kBasicScript, secureURL);
    expectIntegrityFailure(kUnsupportedHashFunctionIntegrity, kBasicScript, secureURL);
}

} // namespace blink
