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

#include "platform/weborigin/SecurityOrigin.h"

#include "platform/blob/BlobURL.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "platform/weborigin/Suborigin.h"
#include "platform/wtf/text/StringBuilder.h"
#include "platform/wtf/text/WTFString.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_util.h"

namespace blink {

const int kMaxAllowedPort = 65535;

class SecurityOriginTest : public ::testing::Test {
 public:
  void SetUp() override {
    url::AddStandardScheme("http-so", url::SCHEME_WITH_PORT);
    url::AddStandardScheme("https-so", url::SCHEME_WITH_PORT);
  }
};

TEST_F(SecurityOriginTest, InvalidPortsCreateUniqueOrigins) {
  int ports[] = {-100, -1, kMaxAllowedPort + 1, 1000000};

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(ports); ++i) {
    RefPtr<SecurityOrigin> origin =
        SecurityOrigin::Create("http", "example.com", ports[i]);
    EXPECT_TRUE(origin->IsUnique())
        << "Port " << ports[i] << " should have generated a unique origin.";
  }
}

TEST_F(SecurityOriginTest, ValidPortsCreateNonUniqueOrigins) {
  int ports[] = {0, 80, 443, 5000, kMaxAllowedPort};

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(ports); ++i) {
    RefPtr<SecurityOrigin> origin =
        SecurityOrigin::Create("http", "example.com", ports[i]);
    EXPECT_FALSE(origin->IsUnique())
        << "Port " << ports[i] << " should not have generated a unique origin.";
  }
}

TEST_F(SecurityOriginTest, LocalAccess) {
  RefPtr<SecurityOrigin> file1 =
      SecurityOrigin::CreateFromString("file:///etc/passwd");
  RefPtr<SecurityOrigin> file2 =
      SecurityOrigin::CreateFromString("file:///etc/shadow");

  EXPECT_TRUE(file1->IsSameSchemeHostPort(file1.get()));
  EXPECT_TRUE(file1->IsSameSchemeHostPort(file2.get()));
  EXPECT_TRUE(file2->IsSameSchemeHostPort(file1.get()));

  EXPECT_TRUE(file1->CanAccess(file1.get()));
  EXPECT_TRUE(file1->CanAccess(file2.get()));
  EXPECT_TRUE(file2->CanAccess(file1.get()));

  // Block |file1|'s access to local origins. It should now be same-origin
  // with itself, but shouldn't have access to |file2|.
  file1->BlockLocalAccessFromLocalOrigin();
  EXPECT_TRUE(file1->IsSameSchemeHostPort(file1.get()));
  EXPECT_FALSE(file1->IsSameSchemeHostPort(file2.get()));
  EXPECT_FALSE(file2->IsSameSchemeHostPort(file1.get()));

  EXPECT_TRUE(file1->CanAccess(file1.get()));
  EXPECT_FALSE(file1->CanAccess(file2.get()));
  EXPECT_FALSE(file2->CanAccess(file1.get()));
}

TEST_F(SecurityOriginTest, IsPotentiallyTrustworthy) {
  struct TestCase {
    bool access_granted;
    const char* url;
  };

  TestCase inputs[] = {
      // Access is granted to webservers running on localhost.
      {true, "http://localhost"},
      {true, "http://LOCALHOST"},
      {true, "http://localhost:100"},
      {true, "http://127.0.0.1"},
      {true, "http://127.0.0.2"},
      {true, "http://127.1.0.2"},
      {true, "http://0177.00.00.01"},
      {true, "http://[::1]"},
      {true, "http://[0:0::1]"},
      {true, "http://[0:0:0:0:0:0:0:1]"},
      {true, "http://[::1]:21"},
      {true, "http://127.0.0.1:8080"},
      {true, "ftp://127.0.0.1"},
      {true, "ftp://127.0.0.1:443"},
      {true, "ws://127.0.0.1"},

      // Access is denied to non-localhost over HTTP
      {false, "http://[1::]"},
      {false, "http://[::2]"},
      {false, "http://[1::1]"},
      {false, "http://[1:2::3]"},
      {false, "http://[::127.0.0.1]"},
      {false, "http://a.127.0.0.1"},
      {false, "http://127.0.0.1.b"},
      {false, "http://localhost.a"},
      {false, "http://a.localhost"},

      // Access is granted to all secure transports.
      {true, "https://foobar.com"},
      {true, "wss://foobar.com"},

      // Access is denied to insecure transports.
      {false, "ftp://foobar.com"},
      {false, "http://foobar.com"},
      {false, "http://foobar.com:443"},
      {false, "ws://foobar.com"},

      // Access is granted to local files
      {true, "file:///home/foobar/index.html"},

      // blob: URLs must look to the inner URL's origin, and apply the same
      // rules as above. Spot check some of them
      {true, "blob:http://localhost:1000/578223a1-8c13-17b3-84d5-eca045ae384a"},
      {true, "blob:https://foopy:99/578223a1-8c13-17b3-84d5-eca045ae384a"},
      {false, "blob:http://baz:99/578223a1-8c13-17b3-84d5-eca045ae384a"},
      {false, "blob:ftp://evil:99/578223a1-8c13-17b3-84d5-eca045ae384a"},

      // filesystem: URLs work the same as blob: URLs, and look to the inner
      // URL for security origin.
      {true, "filesystem:http://localhost:1000/foo"},
      {true, "filesystem:https://foopy:99/foo"},
      {false, "filesystem:http://baz:99/foo"},
      {false, "filesystem:ftp://evil:99/foo"},
  };

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(inputs); ++i) {
    SCOPED_TRACE(i);
    RefPtr<SecurityOrigin> origin =
        SecurityOrigin::CreateFromString(inputs[i].url);
    String error_message;
    EXPECT_EQ(inputs[i].access_granted, origin->IsPotentiallyTrustworthy());
  }

  // Unique origins are not considered secure.
  RefPtr<SecurityOrigin> unique_origin = SecurityOrigin::CreateUnique();
  EXPECT_FALSE(unique_origin->IsPotentiallyTrustworthy());
  // ... unless they are specially marked as such.
  unique_origin->SetUniqueOriginIsPotentiallyTrustworthy(true);
  EXPECT_TRUE(unique_origin->IsPotentiallyTrustworthy());
  unique_origin->SetUniqueOriginIsPotentiallyTrustworthy(false);
  EXPECT_FALSE(unique_origin->IsPotentiallyTrustworthy());
}

TEST_F(SecurityOriginTest, IsSecure) {
  struct TestCase {
    bool is_secure;
    const char* url;
  } inputs[] = {
      {false, "blob:ftp://evil:99/578223a1-8c13-17b3-84d5-eca045ae384a"},
      {false, "blob:http://example.com/578223a1-8c13-17b3-84d5-eca045ae384a"},
      {false, "file:///etc/passwd"},
      {false, "ftp://example.com/"},
      {false, "http://example.com/"},
      {false, "ws://example.com/"},
      {true, "blob:https://example.com/578223a1-8c13-17b3-84d5-eca045ae384a"},
      {true, "https://example.com/"},
      {true, "wss://example.com/"},

      {true, "about:blank"},
      {false, ""},
      {false, "\0"},
  };

  for (auto test : inputs)
    EXPECT_EQ(test.is_secure,
              SecurityOrigin::IsSecure(KURL(kParsedURLString, test.url)))
        << "URL: '" << test.url << "'";

  EXPECT_FALSE(SecurityOrigin::IsSecure(NullURL()));
}

TEST_F(SecurityOriginTest, IsSecureViaTrustworthy) {
  const char* urls[] = {"http://localhost/", "http://localhost:8080/",
                        "http://127.0.0.1/", "http://127.0.0.1:8080/",
                        "http://[::1]/"};

  for (const char* test : urls) {
    KURL url(kParsedURLString, test);
    EXPECT_FALSE(SecurityOrigin::IsSecure(url));
    SecurityPolicy::AddOriginTrustworthyWhiteList(*SecurityOrigin::Create(url));
    EXPECT_TRUE(SecurityOrigin::IsSecure(url));
  }
}

TEST_F(SecurityOriginTest, Suborigins) {
  ScopedSuboriginsForTest suborigins(true);

  RefPtr<SecurityOrigin> origin =
      SecurityOrigin::CreateFromString("https://test.com");
  Suborigin suborigin;
  suborigin.SetName("foobar");
  EXPECT_FALSE(origin->HasSuborigin());
  origin->AddSuborigin(suborigin);
  EXPECT_TRUE(origin->HasSuborigin());
  EXPECT_EQ("foobar", origin->GetSuborigin()->GetName());

  origin = SecurityOrigin::CreateFromString("https-so://foobar.test.com");
  EXPECT_EQ("https", origin->Protocol());
  EXPECT_EQ("test.com", origin->Host());
  EXPECT_EQ("foobar", origin->GetSuborigin()->GetName());

  origin = SecurityOrigin::CreateFromString("https-so://foobar.test.com");
  EXPECT_TRUE(origin->HasSuborigin());
  EXPECT_EQ("foobar", origin->GetSuborigin()->GetName());

  origin = SecurityOrigin::CreateFromString("https://foobar+test.com");
  EXPECT_FALSE(origin->HasSuborigin());

  origin = SecurityOrigin::CreateFromString("https.so://foobar+test.com");
  EXPECT_FALSE(origin->HasSuborigin());

  origin = SecurityOrigin::CreateFromString("https://_test.com");
  EXPECT_FALSE(origin->HasSuborigin());

  origin = SecurityOrigin::CreateFromString("https-so://_test.com");
  EXPECT_TRUE(origin->HasSuborigin());
  EXPECT_EQ("_test", origin->GetSuborigin()->GetName());

  origin = SecurityOrigin::CreateFromString("https-so-so://foobar.test.com");
  EXPECT_FALSE(origin->HasSuborigin());

  origin = WTF::AdoptRef<SecurityOrigin>(new SecurityOrigin);
  EXPECT_FALSE(origin->HasSuborigin());

  origin = SecurityOrigin::CreateFromString("https-so://foobar.test.com");
  Suborigin empty_suborigin;
  EXPECT_DEATH(origin->AddSuborigin(empty_suborigin), "");
}

TEST_F(SecurityOriginTest, SuboriginsParsing) {
  ScopedSuboriginsForTest suborigins(true);
  String protocol, real_protocol, host, real_host, suborigin;
  protocol = "https";
  host = "test.com";
  EXPECT_FALSE(SecurityOrigin::DeserializeSuboriginAndProtocolAndHost(
      protocol, host, suborigin, real_protocol, real_host));

  protocol = "https-so";
  host = "foobar.test.com";
  EXPECT_TRUE(SecurityOrigin::DeserializeSuboriginAndProtocolAndHost(
      protocol, host, suborigin, real_protocol, real_host));
  EXPECT_EQ("https", real_protocol);
  EXPECT_EQ("test.com", real_host);
  EXPECT_EQ("foobar", suborigin);

  RefPtr<SecurityOrigin> origin;
  StringBuilder builder;

  origin = SecurityOrigin::CreateFromString("https-so://foobar.test.com");
  origin->BuildRawString(builder, true);
  EXPECT_EQ("https-so://foobar.test.com", builder.ToString());
  EXPECT_EQ("https-so://foobar.test.com", origin->ToString());
  builder.Clear();
  origin->BuildRawString(builder, false);
  EXPECT_EQ("https://test.com", builder.ToString());
  EXPECT_EQ("https://test.com", origin->ToPhysicalOriginString());

  Suborigin suborigin_obj;
  suborigin_obj.SetName("foobar");
  builder.Clear();
  origin = SecurityOrigin::CreateFromString("https://test.com");
  origin->AddSuborigin(suborigin_obj);
  origin->BuildRawString(builder, true);
  EXPECT_EQ("https-so://foobar.test.com", builder.ToString());
  EXPECT_EQ("https-so://foobar.test.com", origin->ToString());
  builder.Clear();
  origin->BuildRawString(builder, false);
  EXPECT_EQ("https://test.com", builder.ToString());
  EXPECT_EQ("https://test.com", origin->ToPhysicalOriginString());
}

TEST_F(SecurityOriginTest, SuboriginsIsSameSchemeHostPortAndSuborigin) {
  ScopedSuboriginsForTest suborigins(true);
  RefPtr<SecurityOrigin> origin =
      SecurityOrigin::CreateFromString("https-so://foobar.test.com");
  RefPtr<SecurityOrigin> other1 =
      SecurityOrigin::CreateFromString("https-so://bazbar.test.com");
  RefPtr<SecurityOrigin> other2 =
      SecurityOrigin::CreateFromString("http-so://foobar.test.com");
  RefPtr<SecurityOrigin> other3 =
      SecurityOrigin::CreateFromString("https-so://foobar.test.com:1234");
  RefPtr<SecurityOrigin> other4 =
      SecurityOrigin::CreateFromString("https://test.com");

  EXPECT_TRUE(origin->IsSameSchemeHostPortAndSuborigin(origin.get()));
  EXPECT_FALSE(origin->IsSameSchemeHostPortAndSuborigin(other1.get()));
  EXPECT_FALSE(origin->IsSameSchemeHostPortAndSuborigin(other2.get()));
  EXPECT_FALSE(origin->IsSameSchemeHostPortAndSuborigin(other3.get()));
  EXPECT_FALSE(origin->IsSameSchemeHostPortAndSuborigin(other4.get()));
}

TEST_F(SecurityOriginTest, CanAccess) {
  ScopedSuboriginsForTest suborigins(true);

  struct TestCase {
    bool can_access;
    const char* origin1;
    const char* origin2;
  };

  TestCase tests[] = {
      {true, "https://foobar.com", "https://foobar.com"},
      {false, "https://foobar.com", "https://bazbar.com"},
      {false, "https://foobar.com", "https-so://name.foobar.com"},
      {false, "https-so://name.foobar.com", "https://foobar.com"},
      {true, "https-so://name.foobar.com", "https-so://name.foobar.com"},
  };

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(tests); ++i) {
    RefPtr<SecurityOrigin> origin1 =
        SecurityOrigin::CreateFromString(tests[i].origin1);
    RefPtr<SecurityOrigin> origin2 =
        SecurityOrigin::CreateFromString(tests[i].origin2);
    EXPECT_EQ(tests[i].can_access, origin1->CanAccess(origin2.get()));
  }
}

TEST_F(SecurityOriginTest, CanRequest) {
  ScopedSuboriginsForTest suborigins(true);

  struct TestCase {
    bool can_request;
    bool can_request_no_suborigin;
    const char* origin;
    const char* url;
  };

  TestCase tests[] = {
      {true, true, "https://foobar.com", "https://foobar.com"},
      {false, false, "https://foobar.com", "https://bazbar.com"},
      {true, false, "https-so://name.foobar.com", "https://foobar.com"},
      {false, false, "https-so://name.foobar.com", "https://bazbar.com"},
  };

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(tests); ++i) {
    RefPtr<SecurityOrigin> origin =
        SecurityOrigin::CreateFromString(tests[i].origin);
    blink::KURL url(blink::kParsedURLString, tests[i].url);
    EXPECT_EQ(tests[i].can_request, origin->CanRequest(url));
    EXPECT_EQ(tests[i].can_request_no_suborigin,
              origin->CanRequestNoSuborigin(url));
  }
}

TEST_F(SecurityOriginTest, EffectivePort) {
  struct TestCase {
    unsigned short port;
    unsigned short effective_port;
    const char* origin;
  } cases[] = {
      {0, 80, "http://example.com"},
      {0, 80, "http://example.com:80"},
      {81, 81, "http://example.com:81"},
      {0, 443, "https://example.com"},
      {0, 443, "https://example.com:443"},
      {444, 444, "https://example.com:444"},
  };

  for (const auto& test : cases) {
    RefPtr<SecurityOrigin> origin =
        SecurityOrigin::CreateFromString(test.origin);
    EXPECT_EQ(test.port, origin->Port());
    EXPECT_EQ(test.effective_port, origin->EffectivePort());
  }
}

TEST_F(SecurityOriginTest, CreateFromTuple) {
  struct TestCase {
    const char* scheme;
    const char* host;
    unsigned short port;
    const char* origin;
  } cases[] = {
      {"http", "example.com", 80, "http://example.com"},
      {"http", "example.com", 81, "http://example.com:81"},
      {"https", "example.com", 443, "https://example.com"},
      {"https", "example.com", 444, "https://example.com:444"},
      {"file", "", 0, "file://"},
      {"file", "example.com", 0, "file://"},
  };

  for (const auto& test : cases) {
    RefPtr<SecurityOrigin> origin =
        SecurityOrigin::Create(test.scheme, test.host, test.port);
    EXPECT_EQ(test.origin, origin->ToString()) << test.origin;
  }
}

TEST_F(SecurityOriginTest, CreateFromTupleWithSuborigin) {
  struct TestCase {
    const char* scheme;
    const char* host;
    unsigned short port;
    const char* suborigin;
    const char* origin;
  } cases[] = {
      {"http", "example.com", 80, "", "http://example.com"},
      {"http", "example.com", 81, "", "http://example.com:81"},
      {"https", "example.com", 443, "", "https://example.com"},
      {"https", "example.com", 444, "", "https://example.com:444"},
      {"file", "", 0, "", "file://"},
      {"file", "example.com", 0, "", "file://"},
      {"http", "example.com", 80, "foobar", "http-so://foobar.example.com"},
      {"http", "example.com", 81, "foobar", "http-so://foobar.example.com:81"},
      {"https", "example.com", 443, "foobar", "https-so://foobar.example.com"},
      {"https", "example.com", 444, "foobar",
       "https-so://foobar.example.com:444"},
      {"file", "", 0, "foobar", "file://"},
      {"file", "example.com", 0, "foobar", "file://"},
  };

  for (const auto& test : cases) {
    RefPtr<SecurityOrigin> origin = SecurityOrigin::Create(
        test.scheme, test.host, test.port, test.suborigin);
    EXPECT_EQ(test.origin, origin->ToString()) << test.origin;
  }
}

TEST_F(SecurityOriginTest, UniquenessPropagatesToBlobUrls) {
  struct TestCase {
    const char* url;
    bool expected_uniqueness;
    const char* expected_origin_string;
  } cases[]{
      {"", true, "null"},
      {"null", true, "null"},
      {"data:text/plain,hello_world", true, "null"},
      {"file:///path", false, "file://"},
      {"filesystem:http://host/filesystem-path", false, "http://host"},
      {"filesystem:file:///filesystem-path", false, "file://"},
      {"filesystem:null/filesystem-path", true, "null"},
      {"blob:http://host/blob-id", false, "http://host"},
      {"blob:file:///blob-id", false, "file://"},
      {"blob:null/blob-id", true, "null"},
  };

  for (const TestCase& test : cases) {
    RefPtr<SecurityOrigin> origin = SecurityOrigin::CreateFromString(test.url);
    EXPECT_EQ(test.expected_uniqueness, origin->IsUnique());
    EXPECT_EQ(test.expected_origin_string, origin->ToString());

    KURL blob_url = BlobURL::CreatePublicURL(origin.get());
    RefPtr<SecurityOrigin> blob_url_origin = SecurityOrigin::Create(blob_url);
    EXPECT_EQ(blob_url_origin->IsUnique(), origin->IsUnique());
    EXPECT_EQ(blob_url_origin->ToString(), origin->ToString());
    EXPECT_EQ(blob_url_origin->ToRawString(), origin->ToRawString());
  }
}

TEST_F(SecurityOriginTest, UniqueOriginIsSameSchemeHostPort) {
  RefPtr<SecurityOrigin> unique_origin = SecurityOrigin::CreateUnique();
  RefPtr<SecurityOrigin> tuple_origin =
      SecurityOrigin::CreateFromString("http://example.com");

  EXPECT_TRUE(unique_origin->IsSameSchemeHostPort(unique_origin.get()));
  EXPECT_FALSE(SecurityOrigin::CreateUnique()->IsSameSchemeHostPort(
      unique_origin.get()));
  EXPECT_FALSE(tuple_origin->IsSameSchemeHostPort(unique_origin.get()));
  EXPECT_FALSE(unique_origin->IsSameSchemeHostPort(tuple_origin.get()));
}

TEST_F(SecurityOriginTest, CanonicalizeHost) {
  struct TestCase {
    const char* host;
    const char* canonical_output;
    bool expected_success;
  } cases[] = {
      {"", "", true},
      {"example.test", "example.test", true},
      {"EXAMPLE.TEST", "example.test", true},
      {"eXaMpLe.TeSt/path", "example.test%2Fpath", false},
      {",", "%2C", true},
      {"💩", "xn--ls8h", true},
      {"[]", "[]", false},
      {"%yo", "%25yo", false},
  };

  for (const TestCase& test : cases) {
    SCOPED_TRACE(::testing::Message() << "raw host: '" << test.host << "'");
    String host = String::FromUTF8(test.host);
    bool success = false;
    String canonical_host = SecurityOrigin::CanonicalizeHost(host, &success);
    EXPECT_EQ(test.canonical_output, canonical_host);
    EXPECT_EQ(test.expected_success, success);
  }
}

}  // namespace blink
