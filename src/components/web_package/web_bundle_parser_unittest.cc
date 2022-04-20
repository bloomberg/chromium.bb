// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/web_bundle_parser.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/cbor/writer.h"
#include "components/web_package/web_bundle_builder.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_package {

namespace {

constexpr char kPrimaryUrl[] = "https://test.example.com/";
constexpr char kValidityUrl[] =
    "https://test.example.org/resource.validity.msg";
const uint64_t kSignatureDate = 1564272000;  // 2019-07-28T00:00:00Z
const uint64_t kSignatureDuration = 7 * 24 * 60 * 60;

std::string GetTestFileContents(const base::FilePath& path) {
  base::FilePath test_data_dir;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir);
  test_data_dir = test_data_dir.Append(
      base::FilePath(FILE_PATH_LITERAL("components/test/data/web_package")));

  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(test_data_dir.Append(path), &contents));
  return contents;
}

class TestDataSource : public mojom::BundleDataSource {
 public:
  explicit TestDataSource(const base::FilePath& path,
                          const bool is_random_access_context = false)
      : data_(GetTestFileContents(path)),
        is_random_access_context_(is_random_access_context) {}
  explicit TestDataSource(const std::vector<uint8_t>& data,
                          const bool is_random_access_context = false)
      : data_(reinterpret_cast<const char*>(data.data()), data.size()),
        is_random_access_context_(is_random_access_context) {}

  void Read(uint64_t offset, uint64_t length, ReadCallback callback) override {
    if (offset >= data_.size()) {
      std::move(callback).Run(absl::nullopt);
      return;
    }
    const uint8_t* start =
        reinterpret_cast<const uint8_t*>(data_.data()) + offset;
    uint64_t available_length = std::min(length, data_.size() - offset);
    std::move(callback).Run(
        std::vector<uint8_t>(start, start + available_length));
  }

  void Length(LengthCallback callback) override {
    EXPECT_TRUE(is_random_access_context_);
    std::move(callback).Run(data_.size());
  }

  void IsRandomAccessContext(IsRandomAccessContextCallback callback) override {
    std::move(callback).Run(is_random_access_context_);
  }

  base::StringPiece GetPayload(const mojom::BundleResponsePtr& response) {
    return base::StringPiece(data_).substr(response->payload_offset,
                                           response->payload_length);
  }

  void AddReceiver(mojo::PendingReceiver<mojom::BundleDataSource> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

 private:
  std::string data_;
  bool is_random_access_context_;
  mojo::ReceiverSet<mojom::BundleDataSource> receivers_;
};

using ParseBundleResult =
    std::pair<mojom::BundleMetadataPtr, mojom::BundleMetadataParseErrorPtr>;

ParseBundleResult ParseBundle(TestDataSource* data_source,
                              const GURL& base_url = GURL()) {
  mojo::PendingRemote<mojom::BundleDataSource> source_remote;
  data_source->AddReceiver(source_remote.InitWithNewPipeAndPassReceiver());

  mojo::PendingRemote<mojom::WebBundleParser> parser_remote;
  WebBundleParser parser_impl(parser_remote.InitWithNewPipeAndPassReceiver(),
                              std::move(source_remote), base_url);
  mojom::WebBundleParser& parser = parser_impl;

  base::test::TestFuture<mojom::BundleMetadataPtr,
                         mojom::BundleMetadataParseErrorPtr>
      future;
  parser.ParseMetadata(future.GetCallback());
  ParseBundleResult result = future.Take();
  EXPECT_TRUE((result.first && !result.second) ||
              (!result.first && result.second));
  return result;
}

void ExpectFormatError(ParseBundleResult result) {
  ASSERT_TRUE(result.second);
  EXPECT_EQ(result.second->type, mojom::BundleParseErrorType::kFormatError);
}

// Finds the response for |url|.
mojom::BundleResponseLocationPtr FindResponse(
    const mojom::BundleMetadataPtr& metadata,
    const GURL& url) {
  const auto item = metadata->requests.find(url);
  if (item == metadata->requests.end())
    return nullptr;

  return item->second.Clone();
}

mojom::BundleResponsePtr ParseResponse(
    TestDataSource* data_source,
    const mojom::BundleResponseLocationPtr& location,
    const GURL& base_url = GURL()) {
  mojo::PendingRemote<mojom::BundleDataSource> source_remote;
  data_source->AddReceiver(source_remote.InitWithNewPipeAndPassReceiver());

  mojo::PendingRemote<mojom::WebBundleParser> parser_remote;
  WebBundleParser parser_impl(parser_remote.InitWithNewPipeAndPassReceiver(),
                              std::move(source_remote), base_url);
  mojom::WebBundleParser& parser = parser_impl;

  base::test::TestFuture<mojom::BundleResponsePtr,
                         mojom::BundleResponseParseErrorPtr>
      future;
  parser.ParseResponse(location->offset, location->length,
                       future.GetCallback());
  return std::get<0>(future.Take());
}

cbor::Value CreateByteString(base::StringPiece s) {
  return cbor::Value(base::as_bytes(base::make_span(s)));
}

std::string AsString(const std::vector<uint8_t>& data) {
  return std::string(reinterpret_cast<const char*>(data.data()), data.size());
}

std::vector<uint8_t> CreateSmallBundle() {
  web_package::WebBundleBuilder builder;
  builder.AddExchange(kPrimaryUrl,
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  return builder.CreateBundle();
}

}  // namespace

class WebBundleParserTest : public testing::Test {
 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(WebBundleParserTest, WrongMagic) {
  WebBundleBuilder builder;
  std::vector<uint8_t> bundle = builder.CreateBundle();
  bundle[3] ^= 1;
  TestDataSource data_source(bundle);

  mojom::BundleMetadataParseErrorPtr error = ParseBundle(&data_source).second;
  ASSERT_TRUE(error);
  EXPECT_EQ(error->type, mojom::BundleParseErrorType::kFormatError);
}

TEST_F(WebBundleParserTest, UnknownVersion) {
  WebBundleBuilder builder;
  std::vector<uint8_t> bundle = builder.CreateBundle();
  // Modify the version string from "b2\0\0" to "q2\0\0".
  ASSERT_EQ(bundle[11], 'b');
  bundle[11] = 'q';
  TestDataSource data_source(bundle);

  mojom::BundleMetadataParseErrorPtr error = ParseBundle(&data_source).second;
  ASSERT_TRUE(error);
  EXPECT_EQ(error->type, mojom::BundleParseErrorType::kVersionError);
}

TEST_F(WebBundleParserTest, SectionLengthsTooLarge) {
  WebBundleBuilder builder;
  std::string too_long_section_name(8192, 'x');
  builder.AddSection(too_long_section_name, cbor::Value(0));
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatError(ParseBundle(&data_source));
}

TEST_F(WebBundleParserTest, DuplicateSectionName) {
  WebBundleBuilder builder;
  builder.AddSection("foo", cbor::Value(0));
  builder.AddSection("foo", cbor::Value(0));
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatError(ParseBundle(&data_source));
}

TEST_F(WebBundleParserTest, InvalidRequestURL) {
  WebBundleBuilder builder;
  builder.AddExchange("", {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatError(ParseBundle(&data_source));
}

TEST_F(WebBundleParserTest, RequestURLIsNotUTF8) {
  WebBundleBuilder builder(BundleVersion::kB2,
                           /*allow_invalid_utf8_strings_for_testing*/ true);
  builder.AddExchange("https://test.example.com/\xcc",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatError(ParseBundle(&data_source));
}

TEST_F(WebBundleParserTest, RequestURLHasBadScheme) {
  WebBundleBuilder builder;
  builder.AddExchange("file:///tmp/foo",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatError(ParseBundle(&data_source));
}

TEST_F(WebBundleParserTest, RequestURLHasCredentials) {
  WebBundleBuilder builder;
  builder.AddExchange("https://user:passwd@test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatError(ParseBundle(&data_source));
}

TEST_F(WebBundleParserTest, RequestURLHasFragment) {
  WebBundleBuilder builder;
  builder.AddExchange("https://test.example.com/#fragment",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatError(ParseBundle(&data_source));
}

TEST_F(WebBundleParserTest, RequestURLIsValidUuidInPackage) {
  const char uuid_in_package[] =
      "uuid-in-package:f81d4fae-7dec-11d0-a765-00a0c91e6bf6";
  WebBundleBuilder builder;
  builder.AddExchange(uuid_in_package,
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  ASSERT_EQ(metadata->requests.size(), 1u);
  auto location = FindResponse(metadata, GURL(uuid_in_package));
  ASSERT_TRUE(location);
}

TEST_F(WebBundleParserTest, RequestURLIsInvalidUuidInPackage) {
  const char uuid_in_package[] = "uuid-in-package:invalid";
  WebBundleBuilder builder;
  builder.AddExchange(uuid_in_package,
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatError(ParseBundle(&data_source));
}

TEST_F(WebBundleParserTest, NoStatusInResponseHeaders) {
  WebBundleBuilder builder;
  builder.AddExchange("https://test.example.com/",
                      {{"content-type", "text/plain"}},
                      "payload");  // ":status" is missing.
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_FALSE(ParseResponse(&data_source, location));
}

TEST_F(WebBundleParserTest, InvalidResponseStatus) {
  WebBundleBuilder builder;
  builder.AddExchange("https://test.example.com/",
                      {{":status", "0200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_FALSE(ParseResponse(&data_source, location));
}

TEST_F(WebBundleParserTest, ExtraPseudoInResponseHeaders) {
  WebBundleBuilder builder;
  builder.AddExchange(
      "https://test.example.com/",
      {{":status", "200"}, {":foo", ""}, {"content-type", "text/plain"}},
      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_FALSE(ParseResponse(&data_source, location));
}

TEST_F(WebBundleParserTest, UpperCaseCharacterInHeaderName) {
  WebBundleBuilder builder;
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"Content-Type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_FALSE(ParseResponse(&data_source, location));
}

TEST_F(WebBundleParserTest, InvalidHeaderValue) {
  WebBundleBuilder builder;
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "\n"}}, "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_FALSE(ParseResponse(&data_source, location));
}

TEST_F(WebBundleParserTest, NoContentTypeWithNonEmptyContent) {
  WebBundleBuilder builder;
  builder.AddExchange("https://test.example.com/", {{":status", "200"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_FALSE(ParseResponse(&data_source, location));
}

TEST_F(WebBundleParserTest, NoContentTypeWithEmptyContent) {
  WebBundleBuilder builder;
  builder.AddExchange("https://test.example.com/", {{":status", "301"}}, "");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  ASSERT_TRUE(ParseResponse(&data_source, location));
}

TEST_F(WebBundleParserTest, AllKnownSectionInCritical) {
  WebBundleBuilder builder;
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  cbor::Value::ArrayValue critical_section;
  critical_section.emplace_back("index");
  critical_section.emplace_back("critical");
  critical_section.emplace_back("responses");
  builder.AddSection("critical", cbor::Value(critical_section));
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
}

TEST_F(WebBundleParserTest, UnknownSectionInCritical) {
  WebBundleBuilder builder;
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  cbor::Value::ArrayValue critical_section;
  critical_section.emplace_back("unknown_section_name");
  builder.AddSection("critical", cbor::Value(critical_section));
  TestDataSource data_source(builder.CreateBundle());

  ExpectFormatError(ParseBundle(&data_source));
}

TEST_F(WebBundleParserTest, EmptySignaturesSection) {
  WebBundleBuilder builder;
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  // WebBundleBuilder omits signatures section if empty, so create it
  // ourselves.
  cbor::Value::ArrayValue signatures_section;
  signatures_section.emplace_back(cbor::Value::ArrayValue());  // authorities
  signatures_section.emplace_back(
      cbor::Value::ArrayValue());  // vouched-subsets
  builder.AddSection("signatures", cbor::Value(signatures_section));
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  EXPECT_TRUE(metadata->authorities.empty());
  EXPECT_TRUE(metadata->vouched_subsets.empty());
}

TEST_F(WebBundleParserTest, SignaturesSection) {
  WebBundleBuilder builder;
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");

  // Create a signatures section with some dummy data.
  cbor::Value::MapValue authority;
  authority.emplace("cert", CreateByteString("[cert]"));
  authority.emplace("ocsp", CreateByteString("[ocsp]"));
  authority.emplace("sct", CreateByteString("[sct]"));
  builder.AddAuthority(std::move(authority));

  cbor::Value signed_bytes = builder.CreateEncodedSigned(
      kValidityUrl, "[auth-sha256]", kSignatureDate,
      kSignatureDate + kSignatureDuration, "https://test.example.com/",
      "[header-sha256]", "[payload-integrity]");
  cbor::Value::MapValue vouched_subset;
  vouched_subset.emplace("authority", 0);
  vouched_subset.emplace("sig", CreateByteString("[sig]"));
  vouched_subset.emplace("signed", signed_bytes.Clone());
  builder.AddVouchedSubset(std::move(vouched_subset));

  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);

  ASSERT_EQ(metadata->authorities.size(), 1u);
  EXPECT_EQ(AsString(metadata->authorities[0]->cert), "[cert]");
  ASSERT_TRUE(metadata->authorities[0]->ocsp.has_value());
  EXPECT_EQ(AsString(*metadata->authorities[0]->ocsp), "[ocsp]");
  ASSERT_TRUE(metadata->authorities[0]->sct.has_value());
  EXPECT_EQ(AsString(*metadata->authorities[0]->sct), "[sct]");

  ASSERT_EQ(metadata->vouched_subsets.size(), 1u);
  EXPECT_EQ(metadata->vouched_subsets[0]->authority, 0u);
  EXPECT_EQ(AsString(metadata->vouched_subsets[0]->sig), "[sig]");
  EXPECT_EQ(AsString(metadata->vouched_subsets[0]->raw_signed),
            signed_bytes.GetBytestringAsString());

  const auto& parsed_signed = metadata->vouched_subsets[0]->parsed_signed;
  EXPECT_EQ(parsed_signed->validity_url, kValidityUrl);
  EXPECT_EQ(AsString(parsed_signed->auth_sha256), "[auth-sha256]");
  EXPECT_EQ(parsed_signed->date, kSignatureDate);
  EXPECT_EQ(parsed_signed->expires, kSignatureDate + kSignatureDuration);

  EXPECT_EQ(parsed_signed->subset_hashes.size(), 1u);
  const auto& hashes =
      parsed_signed->subset_hashes[GURL("https://test.example.com/")];
  ASSERT_TRUE(hashes);
  EXPECT_EQ(hashes->variants_value, "");
  ASSERT_EQ(hashes->resource_integrities.size(), 1u);
  EXPECT_EQ(AsString(hashes->resource_integrities[0]->header_sha256),
            "[header-sha256]");
  EXPECT_EQ(hashes->resource_integrities[0]->payload_integrity_header,
            "[payload-integrity]");
}

TEST_F(WebBundleParserTest, MultipleSignatures) {
  WebBundleBuilder builder;
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");

  // Create a signatures section with some dummy data.
  cbor::Value::MapValue authority1;
  authority1.emplace("cert", CreateByteString("[cert1]"));
  authority1.emplace("ocsp", CreateByteString("[ocsp]"));
  authority1.emplace("sct", CreateByteString("[sct]"));
  builder.AddAuthority(std::move(authority1));
  cbor::Value::MapValue authority2;
  authority2.emplace("cert", CreateByteString("[cert2]"));
  builder.AddAuthority(std::move(authority2));

  cbor::Value signed_bytes1 = builder.CreateEncodedSigned(
      kValidityUrl, "[auth-sha256]", kSignatureDate,
      kSignatureDate + kSignatureDuration, "https://test.example.com/",
      "[header-sha256]", "[payload-integrity]");
  cbor::Value::MapValue vouched_subset1;
  vouched_subset1.emplace("authority", 0);
  vouched_subset1.emplace("sig", CreateByteString("[sig1]"));
  vouched_subset1.emplace("signed", signed_bytes1.Clone());
  builder.AddVouchedSubset(std::move(vouched_subset1));

  cbor::Value signed_bytes2 = builder.CreateEncodedSigned(
      kValidityUrl, "[auth-sha256-2]", kSignatureDate,
      kSignatureDate + kSignatureDuration, "https://test.example.org/",
      "[header-sha256-2]", "[payload-integrity-2]");
  cbor::Value::MapValue vouched_subset2;
  vouched_subset2.emplace("authority", 1);
  vouched_subset2.emplace("sig", CreateByteString("[sig2]"));
  vouched_subset2.emplace("signed", signed_bytes2.Clone());
  builder.AddVouchedSubset(std::move(vouched_subset2));

  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);

  ASSERT_EQ(metadata->authorities.size(), 2u);
  EXPECT_EQ(AsString(metadata->authorities[0]->cert), "[cert1]");
  EXPECT_TRUE(metadata->authorities[0]->ocsp.has_value());
  EXPECT_TRUE(metadata->authorities[0]->sct.has_value());
  EXPECT_EQ(AsString(metadata->authorities[1]->cert), "[cert2]");
  EXPECT_FALSE(metadata->authorities[1]->ocsp.has_value());
  EXPECT_FALSE(metadata->authorities[1]->sct.has_value());

  ASSERT_EQ(metadata->vouched_subsets.size(), 2u);
  EXPECT_EQ(metadata->vouched_subsets[0]->authority, 0u);
  EXPECT_EQ(AsString(metadata->vouched_subsets[0]->sig), "[sig1]");
  EXPECT_EQ(AsString(metadata->vouched_subsets[0]->raw_signed),
            signed_bytes1.GetBytestringAsString());
  EXPECT_EQ(metadata->vouched_subsets[1]->authority, 1u);
  EXPECT_EQ(AsString(metadata->vouched_subsets[1]->sig), "[sig2]");
  EXPECT_EQ(AsString(metadata->vouched_subsets[1]->raw_signed),
            signed_bytes2.GetBytestringAsString());
}

TEST_F(WebBundleParserTest, ParseGoldenFile) {
  TestDataSource data_source(base::FilePath(FILE_PATH_LITERAL("hello_b2.wbn")));

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  ASSERT_EQ(metadata->requests.size(), 4u);
  EXPECT_EQ(metadata->primary_url, "https://test.example.org/");

  std::map<std::string, mojom::BundleResponsePtr> responses;
  for (const auto& item : metadata->requests) {
    auto location = FindResponse(metadata, item.first);
    ASSERT_TRUE(location);
    auto resp = ParseResponse(&data_source, location);
    ASSERT_TRUE(resp);
    responses[item.first.spec()] = std::move(resp);
  }

  ASSERT_TRUE(responses["https://test.example.org/"]);
  EXPECT_EQ(responses["https://test.example.org/"]->response_code, 200);
  EXPECT_EQ(
      responses["https://test.example.org/"]->response_headers["content-type"],
      "text/html; charset=utf-8");
  EXPECT_EQ(data_source.GetPayload(responses["https://test.example.org/"]),
            GetTestFileContents(
                base::FilePath(FILE_PATH_LITERAL("hello/index.html"))));

  EXPECT_TRUE(responses["https://test.example.org/index.html"]);
  EXPECT_TRUE(responses["https://test.example.org/manifest.webmanifest"]);
  EXPECT_TRUE(responses["https://test.example.org/script.js"]);
}

TEST_F(WebBundleParserTest, ParseSignedFile) {
  TestDataSource data_source(
      base::FilePath(FILE_PATH_LITERAL("hello_signed.wbn")));

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  EXPECT_EQ(metadata->authorities.size(), 1u);
  ASSERT_EQ(metadata->vouched_subsets.size(), 1u);
  EXPECT_EQ(metadata->vouched_subsets[0]->authority, 0u);

  const auto& parsed_signed = metadata->vouched_subsets[0]->parsed_signed;
  EXPECT_EQ(parsed_signed->validity_url, kValidityUrl);
  EXPECT_EQ(parsed_signed->date, kSignatureDate);
  EXPECT_EQ(parsed_signed->expires, kSignatureDate + kSignatureDuration);

  EXPECT_EQ(parsed_signed->subset_hashes.size(), metadata->requests.size());
  const auto& hashes =
      parsed_signed->subset_hashes[GURL("https://test.example.org/")];
  ASSERT_TRUE(hashes);
  EXPECT_EQ(hashes->variants_value, "");
  ASSERT_EQ(hashes->resource_integrities.size(), 1u);
  EXPECT_EQ(hashes->resource_integrities[0]->payload_integrity_header,
            "digest/mi-sha256-03");
}

TEST_F(WebBundleParserTest, SingleEntry) {
  WebBundleBuilder builder;
  builder.AddPrimaryURL(kPrimaryUrl);
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  ASSERT_EQ(metadata->version, mojom::BundleFormatVersion::kB2);
  ASSERT_EQ(metadata->requests.size(), 1u);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  auto response = ParseResponse(&data_source, location);
  ASSERT_TRUE(response);
  EXPECT_EQ(response->response_code, 200);
  EXPECT_EQ(response->response_headers.size(), 1u);
  EXPECT_EQ(response->response_headers["content-type"], "text/plain");
  EXPECT_EQ(data_source.GetPayload(response), "payload");
  EXPECT_EQ(metadata->primary_url, kPrimaryUrl);
}

TEST_F(WebBundleParserTest, NoPrimaryUrlSingleEntry) {
  WebBundleBuilder builder;
  builder.AddExchange("https://test.example.com/",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);
  ASSERT_EQ(metadata->requests.size(), 1u);
  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  auto response = ParseResponse(&data_source, location);
  ASSERT_TRUE(response);
  EXPECT_EQ(response->response_code, 200);
  EXPECT_EQ(response->response_headers.size(), 1u);
  EXPECT_EQ(response->response_headers["content-type"], "text/plain");
  EXPECT_EQ(data_source.GetPayload(response), "payload");
  EXPECT_TRUE(metadata->primary_url.is_empty());
}

TEST_F(WebBundleParserTest, RelativeURL) {
  WebBundleBuilder builder;
  builder.AddPrimaryURL("path/to/primary_url");
  builder.AddExchange("path/to/file.txt",
                      {{":status", "200"}, {"content-type", "text/plain"}},
                      "payload");
  TestDataSource data_source(builder.CreateBundle());

  const GURL base_url("https://test.example.com/dir/test.wbn");
  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source, base_url).first;
  EXPECT_EQ(metadata->primary_url,
            "https://test.example.com/dir/path/to/primary_url");
  ASSERT_TRUE(metadata);
  ASSERT_EQ(metadata->requests.size(), 1u);
  auto location = FindResponse(
      metadata, GURL("https://test.example.com/dir/path/to/file.txt"));
  ASSERT_TRUE(location);
  auto response = ParseResponse(&data_source, location, base_url);
  ASSERT_TRUE(response);
  EXPECT_EQ(response->response_code, 200);
  EXPECT_EQ(response->response_headers.size(), 1u);
  EXPECT_EQ(response->response_headers["content-type"], "text/plain");
  EXPECT_EQ(data_source.GetPayload(response), "payload");
}

TEST_F(WebBundleParserTest, RandomAccessContext) {
  std::vector<uint8_t> bundle = CreateSmallBundle();
  TestDataSource data_source(bundle, /*is_random_access_context=*/true);

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);

  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  auto response = ParseResponse(&data_source, location);
  ASSERT_TRUE(response);
  EXPECT_EQ(response->response_code, 200);
  EXPECT_EQ(response->response_headers.size(), 1u);
  EXPECT_EQ(response->response_headers["content-type"], "text/plain");
  EXPECT_EQ(data_source.GetPayload(response), "payload");
}

TEST_F(WebBundleParserTest, RandomAccessContextPrependedData) {
  std::vector<uint8_t> bundle = CreateSmallBundle();
  bundle.insert(bundle.begin(),
                {'o', 't', 'h', 'e', 'r', ' ', 'd', 'a', 't', 'a'});
  TestDataSource data_source(bundle, /*is_random_access_context=*/true);

  mojom::BundleMetadataPtr metadata = ParseBundle(&data_source).first;
  ASSERT_TRUE(metadata);

  auto location = FindResponse(metadata, GURL("https://test.example.com/"));
  ASSERT_TRUE(location);
  auto response = ParseResponse(&data_source, location);
  ASSERT_TRUE(response);
  EXPECT_EQ(response->response_code, 200);
  EXPECT_EQ(response->response_headers.size(), 1u);
  EXPECT_EQ(response->response_headers["content-type"], "text/plain");
  EXPECT_EQ(data_source.GetPayload(response), "payload");
}

TEST_F(WebBundleParserTest, RandomAccessContextLengthSmallerThanWebBundle) {
  std::vector<uint8_t> bundle = CreateSmallBundle();
  std::vector<uint8_t> invalid_length = {0, 0, 0, 0, 0, 0, 0, 10};
  std::copy(invalid_length.begin(), invalid_length.end(), bundle.end() - 8);
  TestDataSource data_source(bundle, /*is_random_access_context=*/true);

  ExpectFormatError(ParseBundle(&data_source));
}

TEST_F(WebBundleParserTest, RandomAccessContextFileSmallerThanLengthField) {
  std::vector<uint8_t> bundle = {1, 2, 3, 4};
  TestDataSource data_source(bundle, /*is_random_access_context=*/true);

  ExpectFormatError(ParseBundle(&data_source));
}

TEST_F(WebBundleParserTest, RandomAccessContextLengthBiggerThanFile) {
  std::vector<uint8_t> bundle = CreateSmallBundle();
  std::vector<uint8_t> invalid_length = {0xff, 0, 0, 0, 0, 0, 0, 0};
  std::copy(invalid_length.begin(), invalid_length.end(), bundle.end() - 8);
  TestDataSource data_source(bundle, /*is_random_access_context=*/true);

  ExpectFormatError(ParseBundle(&data_source));
}

// TODO(crbug.com/969596): Add a test case that loads a wbn file with variants,
// once gen-bundle supports variants.

}  // namespace web_package
