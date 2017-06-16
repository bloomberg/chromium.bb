// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/SubresourceIntegrity.h"

#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/html/HTMLScriptElement.h"
#include "platform/Crypto.h"
#include "platform/loader/fetch/IntegrityMetadata.h"
#include "platform/loader/fetch/RawResource.h"
#include "platform/loader/fetch/Resource.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/dtoa/utils.h"
#include "platform/wtf/text/WTFString.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

static const char kBasicScript[] = "alert('test');";
static const char kSha256Integrity[] =
    "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4=";
static const char kSha256IntegrityLenientSyntax[] =
    "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4=";
static const char kSha256IntegrityWithEmptyOption[] =
    "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4=?";
static const char kSha256IntegrityWithOption[] =
    "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4=?foo=bar";
static const char kSha256IntegrityWithOptions[] =
    "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4=?foo=bar?baz=foz";
static const char kSha256IntegrityWithMimeOption[] =
    "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4=?ct=application/"
    "javascript";
static const char kSha384Integrity[] =
    "sha384-nep3XpvhUxpCMOVXIFPecThAqdY_uVeiD4kXSqXpx0YJUWU4fTTaFgciTuZk7fmE";
static const char kSha512Integrity[] =
    "sha512-TXkJw18PqlVlEUXXjeXbGetop1TKB3wYQIp1_"
    "ihxCOFGUfG9TYOaA1MlkpTAqSV6yaevLO8Tj5pgH1JmZ--ItA==";
static const char kSha384IntegrityLabeledAs256[] =
    "sha256-nep3XpvhUxpCMOVXIFPecThAqdY_uVeiD4kXSqXpx0YJUWU4fTTaFgciTuZk7fmE";
static const char kSha256AndSha384Integrities[] =
    "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4= "
    "sha384-nep3XpvhUxpCMOVXIFPecThAqdY_uVeiD4kXSqXpx0YJUWU4fTTaFgciTuZk7fmE";
static const char kBadSha256AndGoodSha384Integrities[] =
    "sha256-deadbeef "
    "sha384-nep3XpvhUxpCMOVXIFPecThAqdY_uVeiD4kXSqXpx0YJUWU4fTTaFgciTuZk7fmE";
static const char kGoodSha256AndBadSha384Integrities[] =
    "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4= sha384-deadbeef";
static const char kBadSha256AndBadSha384Integrities[] =
    "sha256-deadbeef sha384-deadbeef";
static const char kUnsupportedHashFunctionIntegrity[] =
    "sha1-JfLW308qMPKfb4DaHpUBEESwuPc=";

class SubresourceIntegrityTest : public ::testing::Test {
 public:
  SubresourceIntegrityTest()
      : secure_url(kParsedURLString, "https://example.test:443"),
        insecure_url(kParsedURLString, "http://example.test:80"),
        secure_origin(SecurityOrigin::Create(secure_url)),
        insecure_origin(SecurityOrigin::Create(insecure_url)) {}

 protected:
  virtual void SetUp() {
    document = Document::Create();
    script_element = HTMLScriptElement::Create(*document, true);
  }

  void ExpectAlgorithm(const String& text, HashAlgorithm expected_algorithm) {
    Vector<UChar> characters;
    text.AppendTo(characters);
    const UChar* position = characters.data();
    const UChar* end = characters.end();
    HashAlgorithm algorithm;

    EXPECT_EQ(SubresourceIntegrity::kAlgorithmValid,
              SubresourceIntegrity::ParseAlgorithm(position, end, algorithm));
    EXPECT_EQ(expected_algorithm, algorithm);
    EXPECT_EQ(end, position);
  }

  void ExpectAlgorithmFailure(
      const String& text,
      SubresourceIntegrity::AlgorithmParseResult expected_result) {
    Vector<UChar> characters;
    text.AppendTo(characters);
    const UChar* position = characters.data();
    const UChar* begin = characters.data();
    const UChar* end = characters.end();
    HashAlgorithm algorithm;

    EXPECT_EQ(expected_result,
              SubresourceIntegrity::ParseAlgorithm(position, end, algorithm));
    EXPECT_EQ(begin, position);
  }

  void ExpectDigest(const String& text, const char* expected_digest) {
    Vector<UChar> characters;
    text.AppendTo(characters);
    const UChar* position = characters.data();
    const UChar* end = characters.end();
    String digest;

    EXPECT_TRUE(SubresourceIntegrity::ParseDigest(position, end, digest));
    EXPECT_EQ(expected_digest, digest);
  }

  void ExpectDigestFailure(const String& text) {
    Vector<UChar> characters;
    text.AppendTo(characters);
    const UChar* position = characters.data();
    const UChar* end = characters.end();
    String digest;

    EXPECT_FALSE(SubresourceIntegrity::ParseDigest(position, end, digest));
    EXPECT_TRUE(digest.IsEmpty());
  }

  void ExpectParse(const char* integrity_attribute,
                   const char* expected_digest,
                   HashAlgorithm expected_algorithm) {
    IntegrityMetadataSet metadata_set;

    EXPECT_EQ(SubresourceIntegrity::kIntegrityParseValidResult,
              SubresourceIntegrity::ParseIntegrityAttribute(integrity_attribute,
                                                            metadata_set));
    EXPECT_EQ(1u, metadata_set.size());
    if (metadata_set.size() > 0) {
      IntegrityMetadata metadata = *metadata_set.begin();
      EXPECT_EQ(expected_digest, metadata.Digest());
      EXPECT_EQ(expected_algorithm, metadata.Algorithm());
    }
  }

  void ExpectParseMultipleHashes(
      const char* integrity_attribute,
      const IntegrityMetadata expected_metadata_array[],
      size_t expected_metadata_array_size) {
    IntegrityMetadataSet expected_metadata_set;
    for (size_t i = 0; i < expected_metadata_array_size; i++) {
      expected_metadata_set.insert(expected_metadata_array[i].ToPair());
    }
    IntegrityMetadataSet metadata_set;
    EXPECT_EQ(SubresourceIntegrity::kIntegrityParseValidResult,
              SubresourceIntegrity::ParseIntegrityAttribute(integrity_attribute,
                                                            metadata_set));
    EXPECT_TRUE(
        IntegrityMetadata::SetsEqual(expected_metadata_set, metadata_set));
  }

  void ExpectParseFailure(const char* integrity_attribute) {
    IntegrityMetadataSet metadata_set;

    EXPECT_EQ(SubresourceIntegrity::kIntegrityParseNoValidResult,
              SubresourceIntegrity::ParseIntegrityAttribute(integrity_attribute,
                                                            metadata_set));
  }

  void ExpectEmptyParseResult(const char* integrity_attribute) {
    IntegrityMetadataSet metadata_set;

    EXPECT_EQ(SubresourceIntegrity::kIntegrityParseValidResult,
              SubresourceIntegrity::ParseIntegrityAttribute(integrity_attribute,
                                                            metadata_set));
    EXPECT_EQ(0u, metadata_set.size());
  }

  enum CorsStatus { kWithCors, kNoCors };

  void ExpectIntegrity(const char* integrity,
                       const char* script,
                       size_t size,
                       const KURL& url,
                       const KURL& requestor_url,
                       CorsStatus cors_status = kWithCors) {
    script_element->setAttribute(HTMLNames::integrityAttr, integrity);
    EXPECT_TRUE(SubresourceIntegrity::CheckSubresourceIntegrity(
        String(integrity), script_element->GetDocument(), script, size, url,
        *CreateTestResource(url, requestor_url, cors_status)));
  }

  void ExpectIntegrityFailure(const char* integrity,
                              const char* script,
                              size_t size,
                              const KURL& url,
                              const KURL& requestor_url,
                              CorsStatus cors_status = kWithCors) {
    script_element->setAttribute(HTMLNames::integrityAttr, integrity);
    EXPECT_FALSE(SubresourceIntegrity::CheckSubresourceIntegrity(
        String(integrity), script_element->GetDocument(), script, size, url,
        *CreateTestResource(url, requestor_url, cors_status)));
  }

  Resource* CreateTestResource(const KURL& url,
                               const KURL& allow_origin_url,
                               CorsStatus cors_status) {
    ResourceResponse response;
    response.SetURL(url);
    response.SetHTTPStatusCode(200);
    if (cors_status == kWithCors) {
      response.SetHTTPHeaderField(
          "access-control-allow-origin",
          SecurityOrigin::Create(allow_origin_url)->ToAtomicString());
      response.SetHTTPHeaderField("access-control-allow-credentials", "true");
    }
    Resource* resource =
        RawResource::CreateForTest(response.Url(), Resource::kRaw);
    resource->SetResponse(response);
    return resource;
  }

  KURL secure_url;
  KURL insecure_url;
  RefPtr<SecurityOrigin> secure_origin;
  RefPtr<SecurityOrigin> insecure_origin;

  Persistent<Document> document;
  Persistent<HTMLScriptElement> script_element;
};

TEST_F(SubresourceIntegrityTest, Prioritization) {
  EXPECT_EQ(kHashAlgorithmSha256,
            SubresourceIntegrity::GetPrioritizedHashFunction(
                kHashAlgorithmSha256, kHashAlgorithmSha256));
  EXPECT_EQ(kHashAlgorithmSha384,
            SubresourceIntegrity::GetPrioritizedHashFunction(
                kHashAlgorithmSha384, kHashAlgorithmSha384));
  EXPECT_EQ(kHashAlgorithmSha512,
            SubresourceIntegrity::GetPrioritizedHashFunction(
                kHashAlgorithmSha512, kHashAlgorithmSha512));

  EXPECT_EQ(kHashAlgorithmSha384,
            SubresourceIntegrity::GetPrioritizedHashFunction(
                kHashAlgorithmSha384, kHashAlgorithmSha256));
  EXPECT_EQ(kHashAlgorithmSha512,
            SubresourceIntegrity::GetPrioritizedHashFunction(
                kHashAlgorithmSha512, kHashAlgorithmSha256));
  EXPECT_EQ(kHashAlgorithmSha512,
            SubresourceIntegrity::GetPrioritizedHashFunction(
                kHashAlgorithmSha512, kHashAlgorithmSha384));

  EXPECT_EQ(kHashAlgorithmSha384,
            SubresourceIntegrity::GetPrioritizedHashFunction(
                kHashAlgorithmSha256, kHashAlgorithmSha384));
  EXPECT_EQ(kHashAlgorithmSha512,
            SubresourceIntegrity::GetPrioritizedHashFunction(
                kHashAlgorithmSha256, kHashAlgorithmSha512));
  EXPECT_EQ(kHashAlgorithmSha512,
            SubresourceIntegrity::GetPrioritizedHashFunction(
                kHashAlgorithmSha384, kHashAlgorithmSha512));
}

TEST_F(SubresourceIntegrityTest, ParseAlgorithm) {
  ExpectAlgorithm("sha256-", kHashAlgorithmSha256);
  ExpectAlgorithm("sha384-", kHashAlgorithmSha384);
  ExpectAlgorithm("sha512-", kHashAlgorithmSha512);
  ExpectAlgorithm("sha-256-", kHashAlgorithmSha256);
  ExpectAlgorithm("sha-384-", kHashAlgorithmSha384);
  ExpectAlgorithm("sha-512-", kHashAlgorithmSha512);

  ExpectAlgorithmFailure("sha1-", SubresourceIntegrity::kAlgorithmUnknown);
  ExpectAlgorithmFailure("sha-1-", SubresourceIntegrity::kAlgorithmUnknown);
  ExpectAlgorithmFailure("foobarsha256-",
                         SubresourceIntegrity::kAlgorithmUnknown);
  ExpectAlgorithmFailure("foobar-", SubresourceIntegrity::kAlgorithmUnknown);
  ExpectAlgorithmFailure("-", SubresourceIntegrity::kAlgorithmUnknown);

  ExpectAlgorithmFailure("sha256", SubresourceIntegrity::kAlgorithmUnparsable);
  ExpectAlgorithmFailure("", SubresourceIntegrity::kAlgorithmUnparsable);
}

TEST_F(SubresourceIntegrityTest, ParseDigest) {
  ExpectDigest("abcdefg", "abcdefg");
  ExpectDigest("abcdefg?", "abcdefg");
  ExpectDigest("ab+de/g", "ab+de/g");
  ExpectDigest("ab-de_g", "ab+de/g");

  ExpectDigestFailure("?");
  ExpectDigestFailure("&&&foobar&&&");
  ExpectDigestFailure("\x01\x02\x03\x04");
}

//
// End-to-end parsing tests.
//

TEST_F(SubresourceIntegrityTest, Parsing) {
  ExpectParseFailure("not_really_a_valid_anything");
  ExpectParseFailure("sha256-&&&foobar&&&");
  ExpectParseFailure("sha256-\x01\x02\x03\x04");
  ExpectParseFailure("sha256-!!! sha256-!!!");

  ExpectEmptyParseResult("foobar:///sha256-abcdefg");
  ExpectEmptyParseResult("ni://sha256-abcdefg");
  ExpectEmptyParseResult("ni:///sha256-abcdefg");
  ExpectEmptyParseResult("notsha256atall-abcdefg");

  ExpectParse(
      "sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
      "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=", kHashAlgorithmSha256);

  ExpectParse(
      "sha-256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
      "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=", kHashAlgorithmSha256);

  ExpectParse(
      "     sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=     ",
      "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=", kHashAlgorithmSha256);

  ExpectParse(
      "sha384-XVVXBGoYw6AJOh9J-Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup_tA1v5GPr",
      "XVVXBGoYw6AJOh9J+Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr",
      kHashAlgorithmSha384);

  ExpectParse(
      "sha-384-XVVXBGoYw6AJOh9J_Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup_"
      "tA1v5GPr",
      "XVVXBGoYw6AJOh9J/Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr",
      kHashAlgorithmSha384);

  ExpectParse(
      "sha512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      kHashAlgorithmSha512);

  ExpectParse(
      "sha-512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      kHashAlgorithmSha512);

  ExpectParse(
      "sha-512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==?ct=application/javascript",
      "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      kHashAlgorithmSha512);

  ExpectParse(
      "sha-512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==?ct=application/xhtml+xml",
      "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      kHashAlgorithmSha512);

  ExpectParse(
      "sha-512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==?foo=bar?ct=application/xhtml+xml",
      "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      kHashAlgorithmSha512);

  ExpectParse(
      "sha-512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==?ct=application/xhtml+xml?foo=bar",
      "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      kHashAlgorithmSha512);

  ExpectParse(
      "sha-512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==?baz=foz?ct=application/"
      "xhtml+xml?foo=bar",
      "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      kHashAlgorithmSha512);

  ExpectParseMultipleHashes("", 0, 0);
  ExpectParseMultipleHashes("    ", 0, 0);

  const IntegrityMetadata valid_sha384_and_sha512[] = {
      IntegrityMetadata(
          "XVVXBGoYw6AJOh9J+Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr",
          kHashAlgorithmSha384),
      IntegrityMetadata("tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+"
                        "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
                        kHashAlgorithmSha512),
  };
  ExpectParseMultipleHashes(
      "sha384-XVVXBGoYw6AJOh9J+Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr "
      "sha512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      valid_sha384_and_sha512, WTF_ARRAY_LENGTH(valid_sha384_and_sha512));

  const IntegrityMetadata valid_sha256_and_sha256[] = {
      IntegrityMetadata("BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
                        kHashAlgorithmSha256),
      IntegrityMetadata("deadbeef", kHashAlgorithmSha256),
  };
  ExpectParseMultipleHashes(
      "sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE= sha256-deadbeef",
      valid_sha256_and_sha256, WTF_ARRAY_LENGTH(valid_sha256_and_sha256));

  const IntegrityMetadata valid_sha256_and_invalid_sha256[] = {
      IntegrityMetadata("BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
                        kHashAlgorithmSha256),
  };
  ExpectParseMultipleHashes(
      "sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE= sha256-!!!!",
      valid_sha256_and_invalid_sha256,
      WTF_ARRAY_LENGTH(valid_sha256_and_invalid_sha256));

  const IntegrityMetadata invalid_sha256_and_valid_sha256[] = {
      IntegrityMetadata("BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
                        kHashAlgorithmSha256),
  };
  ExpectParseMultipleHashes(
      "sha256-!!! sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
      invalid_sha256_and_valid_sha256,
      WTF_ARRAY_LENGTH(invalid_sha256_and_valid_sha256));

  ExpectParse(
      "sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo=bar",
      "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=", kHashAlgorithmSha256);

  ExpectParse(
      "sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo=bar?baz=foz",
      "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=", kHashAlgorithmSha256);

  ExpectParse(
      "sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?",
      "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=", kHashAlgorithmSha256);
  ExpectParse(
      "sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo=bar",
      "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=", kHashAlgorithmSha256);
  ExpectParse(
      "sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo=bar?baz=foz",
      "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=", kHashAlgorithmSha256);
  ExpectParse(
      "sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo",
      "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=", kHashAlgorithmSha256);
  ExpectParse(
      "sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo=bar?",
      "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=", kHashAlgorithmSha256);
  ExpectParse(
      "sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo:bar",
      "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=", kHashAlgorithmSha256);
}

TEST_F(SubresourceIntegrityTest, ParsingBase64) {
  ExpectParse(
      "sha384-XVVXBGoYw6AJOh9J+Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr",
      "XVVXBGoYw6AJOh9J+Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr",
      kHashAlgorithmSha384);
}

//
// End-to-end tests of ::CheckSubresourceIntegrity.
//

TEST_F(SubresourceIntegrityTest, CheckSubresourceIntegrityInSecureOrigin) {
  document->UpdateSecurityOrigin(secure_origin->IsolatedCopy());

  // Verify basic sha256, sha384, and sha512 integrity checks.
  ExpectIntegrity(kSha256Integrity, kBasicScript, strlen(kBasicScript),
                  secure_url, secure_url);
  ExpectIntegrity(kSha256IntegrityLenientSyntax, kBasicScript,
                  strlen(kBasicScript), secure_url, secure_url);
  ExpectIntegrity(kSha384Integrity, kBasicScript, strlen(kBasicScript),
                  secure_url, secure_url);
  ExpectIntegrity(kSha512Integrity, kBasicScript, strlen(kBasicScript),
                  secure_url, secure_url);

  // Verify multiple hashes in an attribute.
  ExpectIntegrity(kSha256AndSha384Integrities, kBasicScript,
                  strlen(kBasicScript), secure_url, secure_url);
  ExpectIntegrity(kBadSha256AndGoodSha384Integrities, kBasicScript,
                  strlen(kBasicScript), secure_url, secure_url);

  // The hash label must match the hash value.
  ExpectIntegrityFailure(kSha384IntegrityLabeledAs256, kBasicScript,
                         strlen(kBasicScript), secure_url, secure_url);

  // With multiple values, at least one must match, and it must be the
  // strongest hash algorithm.
  ExpectIntegrityFailure(kGoodSha256AndBadSha384Integrities, kBasicScript,
                         strlen(kBasicScript), secure_url, secure_url);
  ExpectIntegrityFailure(kBadSha256AndBadSha384Integrities, kBasicScript,
                         strlen(kBasicScript), secure_url, secure_url);

  // Unsupported hash functions should succeed.
  ExpectIntegrity(kUnsupportedHashFunctionIntegrity, kBasicScript,
                  strlen(kBasicScript), secure_url, secure_url);

  // All parameters are fine, and because this is not cross origin, CORS is
  // not needed.
  ExpectIntegrity(kSha256Integrity, kBasicScript, strlen(kBasicScript),
                  secure_url, secure_url, kNoCors);

  // Options should be ignored
  ExpectIntegrity(kSha256IntegrityWithEmptyOption, kBasicScript,
                  strlen(kBasicScript), secure_url, secure_url, kNoCors);
  ExpectIntegrity(kSha256IntegrityWithOption, kBasicScript,
                  strlen(kBasicScript), secure_url, secure_url, kNoCors);
  ExpectIntegrity(kSha256IntegrityWithOptions, kBasicScript,
                  strlen(kBasicScript), secure_url, secure_url, kNoCors);
  ExpectIntegrity(kSha256IntegrityWithMimeOption, kBasicScript,
                  strlen(kBasicScript), secure_url, secure_url, kNoCors);
}

TEST_F(SubresourceIntegrityTest, CheckSubresourceIntegrityInInsecureOrigin) {
  // The same checks as CheckSubresourceIntegrityInSecureOrigin should pass
  // here, with the expection of the NoCors check at the end.
  document->UpdateSecurityOrigin(insecure_origin->IsolatedCopy());

  ExpectIntegrity(kSha256Integrity, kBasicScript, strlen(kBasicScript),
                  secure_url, insecure_url);
  ExpectIntegrity(kSha256IntegrityLenientSyntax, kBasicScript,
                  strlen(kBasicScript), secure_url, insecure_url);
  ExpectIntegrity(kSha384Integrity, kBasicScript, strlen(kBasicScript),
                  secure_url, insecure_url);
  ExpectIntegrity(kSha512Integrity, kBasicScript, strlen(kBasicScript),
                  secure_url, insecure_url);
  ExpectIntegrityFailure(kSha384IntegrityLabeledAs256, kBasicScript,
                         strlen(kBasicScript), secure_url, insecure_url);
  ExpectIntegrity(kUnsupportedHashFunctionIntegrity, kBasicScript,
                  strlen(kBasicScript), secure_url, insecure_url);

  ExpectIntegrity(kSha256AndSha384Integrities, kBasicScript,
                  strlen(kBasicScript), secure_url, insecure_url);
  ExpectIntegrity(kBadSha256AndGoodSha384Integrities, kBasicScript,
                  strlen(kBasicScript), secure_url, insecure_url);

  ExpectIntegrityFailure(kSha256Integrity, kBasicScript, strlen(kBasicScript),
                         secure_url, insecure_url, kNoCors);
  ExpectIntegrityFailure(kGoodSha256AndBadSha384Integrities, kBasicScript,
                         strlen(kBasicScript), secure_url, insecure_url);
}

}  // namespace blink
