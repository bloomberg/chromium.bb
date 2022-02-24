#include "http2/adapter/header_validator.h"

#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "common/platform/api/quiche_test.h"

namespace http2 {
namespace adapter {
namespace test {

using ::testing::Optional;

using Header = std::pair<absl::string_view, absl::string_view>;
constexpr Header kSampleRequestPseudoheaders[] = {{":authority", "www.foo.com"},
                                                  {":method", "GET"},
                                                  {":path", "/foo"},
                                                  {":scheme", "https"}};

TEST(HeaderValidatorTest, HeaderNameEmpty) {
  HeaderValidator v;
  HeaderValidator::HeaderStatus status = v.ValidateSingleHeader("", "value");
  EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID, status);
}

TEST(HeaderValidatorTest, HeaderValueEmpty) {
  HeaderValidator v;
  HeaderValidator::HeaderStatus status = v.ValidateSingleHeader("name", "");
  EXPECT_EQ(HeaderValidator::HEADER_OK, status);
}

TEST(HeaderValidatorTest, ExceedsMaxSize) {
  HeaderValidator v;
  v.SetMaxFieldSize(64u);
  HeaderValidator::HeaderStatus status =
      v.ValidateSingleHeader("name", "value");
  EXPECT_EQ(HeaderValidator::HEADER_OK, status);
  status = v.ValidateSingleHeader(
      "name2",
      "Antidisestablishmentariansism is supercalifragilisticexpialodocious.");
  EXPECT_EQ(HeaderValidator::HEADER_FIELD_TOO_LONG, status);
}

TEST(HeaderValidatorTest, NameHasInvalidChar) {
  HeaderValidator v;
  for (const bool is_pseudo_header : {true, false}) {
    // These characters should be allowed. (Not exhaustive.)
    for (const char* c : {"!", "3", "a", "_", "|", "~"}) {
      const std::string name = is_pseudo_header ? absl::StrCat(":met", c, "hod")
                                                : absl::StrCat("na", c, "me");
      HeaderValidator::HeaderStatus status =
          v.ValidateSingleHeader(name, "value");
      EXPECT_EQ(HeaderValidator::HEADER_OK, status);
    }
    // These should not. (Not exhaustive.)
    for (const char* c : {"\\", "<", ";", "[", "=", " ", "\r", "\n", ",", "\"",
                          "\x1F", "\x91"}) {
      const std::string name = is_pseudo_header ? absl::StrCat(":met", c, "hod")
                                                : absl::StrCat("na", c, "me");
      HeaderValidator::HeaderStatus status =
          v.ValidateSingleHeader(name, "value");
      EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID, status);
    }
    // Uppercase characters in header names should not be allowed.
    const std::string uc_name = is_pseudo_header ? ":Method" : "Name";
    HeaderValidator::HeaderStatus status =
        v.ValidateSingleHeader(uc_name, "value");
    EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID, status);
  }
}

TEST(HeaderValidatorTest, ValueHasInvalidChar) {
  HeaderValidator v;
  // These characters should be allowed. (Not exhaustive.)
  for (const char* c :
       {"!", "3", "a", "_", "|", "~", "\\", "<", ";", "[", "=", "A", "\t"}) {
    HeaderValidator::HeaderStatus status =
        v.ValidateSingleHeader("name", absl::StrCat("val", c, "ue"));
    EXPECT_EQ(HeaderValidator::HEADER_OK, status);
  }
  // These should not.
  for (const char* c : {"\r", "\n"}) {
    HeaderValidator::HeaderStatus status =
        v.ValidateSingleHeader("name", absl::StrCat("val", c, "ue"));
    EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID, status);
  }
}

TEST(HeaderValidatorTest, StatusHasInvalidChar) {
  HeaderValidator v;

  for (HeaderType type : {HeaderType::RESPONSE, HeaderType::RESPONSE_100}) {
    // When `:status` has a non-digit value, validation will fail.
    v.StartHeaderBlock();
    EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
              v.ValidateSingleHeader(":status", "bar"));
    EXPECT_FALSE(v.FinishHeaderBlock(type));

    // When `:status` is too short, validation will fail.
    v.StartHeaderBlock();
    EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
              v.ValidateSingleHeader(":status", "10"));
    EXPECT_FALSE(v.FinishHeaderBlock(type));

    // When `:status` is too long, validation will fail.
    v.StartHeaderBlock();
    EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
              v.ValidateSingleHeader(":status", "9000"));
    EXPECT_FALSE(v.FinishHeaderBlock(type));

    // When `:status` is just right, validation will succeed.
    v.StartHeaderBlock();
    EXPECT_EQ(HeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(":status", "400"));
    EXPECT_TRUE(v.FinishHeaderBlock(type));
  }
}

TEST(HeaderValidatorTest, AuthorityHasInvalidChar) {
  HeaderValidator v;
  v.StartHeaderBlock();

  // These characters should be allowed. (Not exhaustive.)
  for (const absl::string_view c : {"1", "-", "!", ":", "+", "=", ","}) {
    HeaderValidator::HeaderStatus status = v.ValidateSingleHeader(
        ":authority", absl::StrCat("ho", c, "st.example.com"));
    EXPECT_EQ(HeaderValidator::HEADER_OK, status);
  }
  // These should not.
  for (const absl::string_view c : {"\r", "\n", "|", "\\", "`"}) {
    HeaderValidator::HeaderStatus status = v.ValidateSingleHeader(
        ":authority", absl::StrCat("ho", c, "st.example.com"));
    EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID, status);
  }

  // IPv4 example
  HeaderValidator::HeaderStatus status =
      v.ValidateSingleHeader(":authority", "123.45.67.89");
  EXPECT_EQ(HeaderValidator::HEADER_OK, status);

  // IPv6 examples
  status = v.ValidateSingleHeader(":authority",
                                  "2001:0db8:85a3:0000:0000:8a2e:0370:7334");
  EXPECT_EQ(HeaderValidator::HEADER_OK, status);
  status = v.ValidateSingleHeader(":authority", "[::1]:80");
  EXPECT_EQ(HeaderValidator::HEADER_OK, status);

  // Empty field
  status = v.ValidateSingleHeader(":authority", "");
  EXPECT_EQ(HeaderValidator::HEADER_OK, status);
}

TEST(HeaderValidatorTest, RequestPseudoHeaders) {
  HeaderValidator v;
  for (Header to_skip : kSampleRequestPseudoheaders) {
    v.StartHeaderBlock();
    for (Header to_add : kSampleRequestPseudoheaders) {
      if (to_add != to_skip) {
        EXPECT_EQ(HeaderValidator::HEADER_OK,
                  v.ValidateSingleHeader(to_add.first, to_add.second));
      }
    }
    // When any pseudo-header is missing, final validation will fail.
    EXPECT_FALSE(v.FinishHeaderBlock(HeaderType::REQUEST));
  }

  // When all pseudo-headers are present, final validation will succeed.
  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    EXPECT_EQ(HeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(to_add.first, to_add.second));
  }
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::REQUEST));

  // When an extra pseudo-header is present, final validation will fail.
  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    EXPECT_EQ(HeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(to_add.first, to_add.second));
  }
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":extra", "blah"));
  EXPECT_FALSE(v.FinishHeaderBlock(HeaderType::REQUEST));

  // When a required pseudo-header is repeated, final validation will fail.
  for (Header to_repeat : kSampleRequestPseudoheaders) {
    v.StartHeaderBlock();
    for (Header to_add : kSampleRequestPseudoheaders) {
      EXPECT_EQ(HeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, to_add.second));
      if (to_add == to_repeat) {
        EXPECT_EQ(HeaderValidator::HEADER_OK,
                  v.ValidateSingleHeader(to_add.first, to_add.second));
      }
    }
    EXPECT_FALSE(v.FinishHeaderBlock(HeaderType::REQUEST));
  }
}

TEST(HeaderValidatorTest, WebsocketPseudoHeaders) {
  HeaderValidator v;
  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    EXPECT_EQ(HeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(to_add.first, to_add.second));
  }
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":protocol", "websocket"));
  // At this point, `:protocol` is treated as an extra pseudo-header.
  EXPECT_FALSE(v.FinishHeaderBlock(HeaderType::REQUEST));

  // Future header blocks may send the `:protocol` pseudo-header for CONNECT
  // requests.
  v.AllowConnect();

  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    EXPECT_EQ(HeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(to_add.first, to_add.second));
  }
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":protocol", "websocket"));
  // The method is not "CONNECT", so `:protocol` is still treated as an extra
  // pseudo-header.
  EXPECT_FALSE(v.FinishHeaderBlock(HeaderType::REQUEST));

  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    if (to_add.first == ":method") {
      EXPECT_EQ(HeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, "CONNECT"));
    } else {
      EXPECT_EQ(HeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, to_add.second));
    }
  }
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":protocol", "websocket"));
  // After allowing the method, `:protocol` is acepted for CONNECT requests.
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::REQUEST));
}

TEST(HeaderValidatorTest, AsteriskPathPseudoHeader) {
  HeaderValidator v;

  // An asterisk :path should not be allowed for non-OPTIONS requests.
  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    if (to_add.first == ":path") {
      EXPECT_EQ(HeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, "*"));
    } else {
      EXPECT_EQ(HeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, to_add.second));
    }
  }
  EXPECT_FALSE(v.FinishHeaderBlock(HeaderType::REQUEST));

  // An asterisk :path should be allowed for OPTIONS requests.
  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    if (to_add.first == ":path") {
      EXPECT_EQ(HeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, "*"));
    } else if (to_add.first == ":method") {
      EXPECT_EQ(HeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, "OPTIONS"));
    } else {
      EXPECT_EQ(HeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, to_add.second));
    }
  }
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::REQUEST));
}

TEST(HeaderValidatorTest, InvalidPathPseudoHeader) {
  HeaderValidator v;

  // An empty path should fail on single header validation and finish.
  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    if (to_add.first == ":path") {
      EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
                v.ValidateSingleHeader(to_add.first, ""));
    } else {
      EXPECT_EQ(HeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, to_add.second));
    }
  }
  EXPECT_FALSE(v.FinishHeaderBlock(HeaderType::REQUEST));

  // A path that does not start with a slash should fail on finish.
  v.StartHeaderBlock();
  for (Header to_add : kSampleRequestPseudoheaders) {
    if (to_add.first == ":path") {
      EXPECT_EQ(HeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, "shawarma"));
    } else {
      EXPECT_EQ(HeaderValidator::HEADER_OK,
                v.ValidateSingleHeader(to_add.first, to_add.second));
    }
  }
  EXPECT_FALSE(v.FinishHeaderBlock(HeaderType::REQUEST));
}

TEST(HeaderValidatorTest, ResponsePseudoHeaders) {
  HeaderValidator v;

  for (HeaderType type : {HeaderType::RESPONSE, HeaderType::RESPONSE_100}) {
    // When `:status` is missing, validation will fail.
    v.StartHeaderBlock();
    EXPECT_EQ(HeaderValidator::HEADER_OK, v.ValidateSingleHeader("foo", "bar"));
    EXPECT_FALSE(v.FinishHeaderBlock(type));

    // When all pseudo-headers are present, final validation will succeed.
    v.StartHeaderBlock();
    EXPECT_EQ(HeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(":status", "199"));
    EXPECT_TRUE(v.FinishHeaderBlock(type));
    EXPECT_EQ("199", v.status_header());

    // When `:status` is repeated, validation will fail.
    v.StartHeaderBlock();
    EXPECT_EQ(HeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(":status", "199"));
    EXPECT_EQ(HeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(":status", "299"));
    EXPECT_FALSE(v.FinishHeaderBlock(type));

    // When an extra pseudo-header is present, final validation will fail.
    v.StartHeaderBlock();
    EXPECT_EQ(HeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(":status", "199"));
    EXPECT_EQ(HeaderValidator::HEADER_OK,
              v.ValidateSingleHeader(":extra", "blorp"));
    EXPECT_FALSE(v.FinishHeaderBlock(type));
  }
}

TEST(HeaderValidatorTest, Response204) {
  HeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":status", "204"));
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("x-content", "is not present"));
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::RESPONSE));
}

TEST(HeaderValidatorTest, Response204WithContentLengthZero) {
  HeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":status", "204"));
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("x-content", "is not present"));
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("content-length", "0"));
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::RESPONSE));
}

TEST(HeaderValidatorTest, Response204WithContentLength) {
  HeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":status", "204"));
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("x-content", "is not present"));
  EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
            v.ValidateSingleHeader("content-length", "1"));
}

TEST(HeaderValidatorTest, Response100) {
  HeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":status", "100"));
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("x-content", "is not present"));
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::RESPONSE));
}

TEST(HeaderValidatorTest, Response100WithContentLengthZero) {
  HeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":status", "100"));
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("x-content", "is not present"));
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("content-length", "0"));
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::RESPONSE));
}

TEST(HeaderValidatorTest, Response100WithContentLength) {
  HeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":status", "100"));
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("x-content", "is not present"));
  EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
            v.ValidateSingleHeader("content-length", "1"));
}

TEST(HeaderValidatorTest, ResponseTrailerPseudoHeaders) {
  HeaderValidator v;

  // When no pseudo-headers are present, validation will succeed.
  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_OK, v.ValidateSingleHeader("foo", "bar"));
  EXPECT_TRUE(v.FinishHeaderBlock(HeaderType::RESPONSE_TRAILER));

  // When any pseudo-header is present, final validation will fail.
  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader(":status", "200"));
  EXPECT_EQ(HeaderValidator::HEADER_OK, v.ValidateSingleHeader("foo", "bar"));
  EXPECT_FALSE(v.FinishHeaderBlock(HeaderType::RESPONSE_TRAILER));
}

TEST(HeaderValidatorTest, ValidContentLength) {
  HeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(v.content_length(), absl::nullopt);
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("content-length", "41"));
  EXPECT_THAT(v.content_length(), Optional(41));

  v.StartHeaderBlock();
  EXPECT_EQ(v.content_length(), absl::nullopt);
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("content-length", "42"));
  EXPECT_THAT(v.content_length(), Optional(42));
}

TEST(HeaderValidatorTest, InvalidContentLength) {
  HeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(v.content_length(), absl::nullopt);
  EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
            v.ValidateSingleHeader("content-length", ""));
  EXPECT_EQ(v.content_length(), absl::nullopt);
  EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
            v.ValidateSingleHeader("content-length", "nan"));
  EXPECT_EQ(v.content_length(), absl::nullopt);
  EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
            v.ValidateSingleHeader("content-length", "-42"));
  EXPECT_EQ(v.content_length(), absl::nullopt);
  // End on a positive note.
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("content-length", "42"));
  EXPECT_THAT(v.content_length(), Optional(42));
}

TEST(HeaderValidatorTest, TeHeader) {
  HeaderValidator v;

  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_OK,
            v.ValidateSingleHeader("te", "trailers"));

  v.StartHeaderBlock();
  EXPECT_EQ(HeaderValidator::HEADER_FIELD_INVALID,
            v.ValidateSingleHeader("te", "trailers, deflate"));
}

}  // namespace test
}  // namespace adapter
}  // namespace http2
