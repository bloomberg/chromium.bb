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
static const char kSha256Integrity[] = "ni://sha256;GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4=";
static const char kSha384Intgrity[] = "ni://sha384;nep3XpvhUxpCMOVXIFPecThAqdY/uVeiD4kXSqXpx0YJUWU4fTTaFgciTuZk7fmE";
static const char kSha512Integrity[] = "ni://sha512;TXkJw18PqlVlEUXXjeXbGetop1TKB3wYQIp1/ihxCOFGUfG9TYOaA1MlkpTAqSV6yaevLO8Tj5pgH1JmZ++ItA==";
static const char kSha384IntegrityLabeledAs256[] = "ni://sha256;nep3XpvhUxpCMOVXIFPecThAqdY/uVeiD4kXSqXpx0YJUWU4fTTaFgciTuZk7fmE";
static const char kUnsupportedHashFunctionIntegrity[] = "ni://sha1;JfLW308qMPKfb4DaHpUBEESwuPc=";

TEST(SubresourceIntegrityTest, CheckSubresourceIntegrity)
{
    KURL secureKURL(KURL(), "https://foobar.com:443");
    KURL insecureKURL(KURL(), "http://foobar.com:80");
    RefPtr<SecurityOrigin> secureOrigin = SecurityOrigin::create(secureKURL);
    RefPtr<SecurityOrigin> insecureOrigin = SecurityOrigin::create(insecureKURL);
    RefPtrWillBeRawPtr<Document> document = Document::create();
    RefPtrWillBeRawPtr<HTMLScriptElement> scriptElement = HTMLScriptElement::create(*document, true);

    // Verify basic sha256, sha384, and sha512 integrity checks.
    document->updateSecurityOrigin(secureOrigin->isolatedCopy());
    scriptElement->setAttribute(HTMLNames::integrityAttr, kSha256Integrity);
    EXPECT_TRUE(SubresourceIntegrity::CheckSubresourceIntegrity(*scriptElement, kBasicScript, secureKURL));
    scriptElement->setAttribute(HTMLNames::integrityAttr, kSha384Intgrity);
    EXPECT_TRUE(SubresourceIntegrity::CheckSubresourceIntegrity(*scriptElement, kBasicScript, secureKURL));
    scriptElement->setAttribute(HTMLNames::integrityAttr, kSha512Integrity);
    EXPECT_TRUE(SubresourceIntegrity::CheckSubresourceIntegrity(*scriptElement, kBasicScript, secureKURL));

    // The hash label must match the hash value.
    scriptElement->setAttribute(HTMLNames::integrityAttr, kSha384IntegrityLabeledAs256);
    EXPECT_FALSE(SubresourceIntegrity::CheckSubresourceIntegrity(*scriptElement, kBasicScript, secureKURL));

    scriptElement->setAttribute(HTMLNames::integrityAttr, kSha256Integrity);
    // Check should fail if the document is not on an authenticated origin or
    // if the resource is not on an authenticated origin.
    document->updateSecurityOrigin(insecureOrigin->isolatedCopy());
    EXPECT_FALSE(SubresourceIntegrity::CheckSubresourceIntegrity(*scriptElement, kBasicScript, secureKURL));
    document->updateSecurityOrigin(secureOrigin->isolatedCopy());
    EXPECT_FALSE(SubresourceIntegrity::CheckSubresourceIntegrity(*scriptElement, kBasicScript, insecureKURL));

    scriptElement->setAttribute(HTMLNames::integrityAttr, kUnsupportedHashFunctionIntegrity);
    EXPECT_FALSE(SubresourceIntegrity::CheckSubresourceIntegrity(*scriptElement, kBasicScript, insecureKURL));
}

TEST(SubresourceIntegrityTest, Parsing)
{
    String attribute;
    String integrity;
    HashAlgorithm algorithm;

    // Verify that empty attribute is not valid.
    EXPECT_FALSE(SubresourceIntegrity::parseIntegrityAttribute(attribute, integrity, algorithm));

    // Valid sha256 attribute
    attribute = "ni://sha256;BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=";
    EXPECT_TRUE(SubresourceIntegrity::parseIntegrityAttribute(attribute, integrity, algorithm));
    EXPECT_EQ(integrity, "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=");
    EXPECT_EQ(algorithm, HashAlgorithmSha256);

    // Another valid sha256 attribute, but this time with whitespace
    attribute = "    ni://sha256;BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=    ";
    EXPECT_TRUE(SubresourceIntegrity::parseIntegrityAttribute(attribute, integrity, algorithm));
    EXPECT_EQ(integrity, "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=");
    EXPECT_EQ(algorithm, HashAlgorithmSha256);

    // Valid sha384 attribute
    attribute = "ni://sha384;XVVXBGoYw6AJOh9J/Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr";
    EXPECT_TRUE(SubresourceIntegrity::parseIntegrityAttribute(attribute, integrity, algorithm));
    EXPECT_EQ(integrity, "XVVXBGoYw6AJOh9J/Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr");
    EXPECT_EQ(algorithm, HashAlgorithmSha384);

    // Valid sha512 attribute
    attribute = "ni://sha512;tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==";
    EXPECT_TRUE(SubresourceIntegrity::parseIntegrityAttribute(attribute, integrity, algorithm));
    EXPECT_EQ(integrity, "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==");
    EXPECT_EQ(algorithm, HashAlgorithmSha512);

    // Invalid prefix
    attribute = "foobar://sha256;BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=";
    EXPECT_FALSE(SubresourceIntegrity::parseIntegrityAttribute(attribute, integrity, algorithm));

    // Invalid hash function
    attribute = "ni://not_a_hash_function;BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=";
    EXPECT_FALSE(SubresourceIntegrity::parseIntegrityAttribute(attribute, integrity, algorithm));

    // Invalid integrity (not base64)
    attribute = "ni://sha256;&&&foobar&&&";
    EXPECT_FALSE(SubresourceIntegrity::parseIntegrityAttribute(attribute, integrity, algorithm));
    attribute = "ni://sha256;\x01\x02\x03\x04";
    EXPECT_FALSE(SubresourceIntegrity::parseIntegrityAttribute(attribute, integrity, algorithm));

    // Just integrity
    attribute = "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=";
    EXPECT_FALSE(SubresourceIntegrity::parseIntegrityAttribute(attribute, integrity, algorithm));
}

} // namespace blink
