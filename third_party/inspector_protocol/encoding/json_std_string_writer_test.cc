// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "json_std_string_writer.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "gtest/gtest.h"
#include "linux_dev_platform.h"

namespace inspector_protocol {
std::vector<uint16_t> UTF16String(const std::string& utf8) {
  base::string16 string16 = base::UTF8ToUTF16(utf8);
  return std::vector<uint16_t>(string16.data(),
                               string16.data() + string16.size());
}

void WriteUTF8AsUTF16(JSONParserHandler* writer, const std::string& utf8) {
  std::vector<uint16_t> utf16 = UTF16String(utf8);
  writer->HandleString16(span<uint16_t>(utf16.data(), utf16.size()));
}

TEST(JsonStdStringWriterTest, HelloWorld) {
  std::string out;
  Status status;
  std::unique_ptr<JSONParserHandler> writer =
      NewJSONWriter(GetLinuxDevPlatform(), &out, &status);
  writer->HandleObjectBegin();
  WriteUTF8AsUTF16(writer.get(), "msg1");
  WriteUTF8AsUTF16(writer.get(), "Hello, 🌎.");
  std::string key = "msg1-as-utf8";
  std::string value = "Hello, 🌎.";
  writer->HandleString8(
      span<uint8_t>(reinterpret_cast<const uint8_t*>(key.data()), key.size()));
  writer->HandleString8(span<uint8_t>(
      reinterpret_cast<const uint8_t*>(value.data()), value.size()));
  WriteUTF8AsUTF16(writer.get(), "msg2");
  WriteUTF8AsUTF16(writer.get(), "\\\b\r\n\t\f\"");
  WriteUTF8AsUTF16(writer.get(), "nested");
  writer->HandleObjectBegin();
  WriteUTF8AsUTF16(writer.get(), "double");
  writer->HandleDouble(3.1415);
  WriteUTF8AsUTF16(writer.get(), "int");
  writer->HandleInt32(-42);
  WriteUTF8AsUTF16(writer.get(), "bool");
  writer->HandleBool(false);
  WriteUTF8AsUTF16(writer.get(), "null");
  writer->HandleNull();
  writer->HandleObjectEnd();
  WriteUTF8AsUTF16(writer.get(), "array");
  writer->HandleArrayBegin();
  writer->HandleInt32(1);
  writer->HandleInt32(2);
  writer->HandleInt32(3);
  writer->HandleArrayEnd();
  writer->HandleObjectEnd();
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(
      "{\"msg1\":\"Hello, \\ud83c\\udf0e.\","
      "\"msg1-as-utf8\":\"Hello, \\ud83c\\udf0e.\","
      "\"msg2\":\"\\\\\\b\\r\\n\\t\\f\\\"\","
      "\"nested\":{\"double\":3.1415,\"int\":-42,"
      "\"bool\":false,\"null\":null},\"array\":[1,2,3]}",
      out);
}

TEST(JsonStdStringWriterTest, BinaryEncodedAsJsonString) {
  // The encoder emits binary submitted to JSONParserHandler::HandleBinary
  // as base64. The following three examples are taken from
  // https://en.wikipedia.org/wiki/Base64.
  {
    std::string out;
    Status status;
    std::unique_ptr<JSONParserHandler> writer =
        NewJSONWriter(GetLinuxDevPlatform(), &out, &status);
    writer->HandleBinary({'M', 'a', 'n'});
    EXPECT_TRUE(status.ok());
    EXPECT_EQ("\"TWFu\"", out);
  }
  {
    std::string out;
    Status status;
    std::unique_ptr<JSONParserHandler> writer =
        NewJSONWriter(GetLinuxDevPlatform(), &out, &status);
    writer->HandleBinary({'M', 'a'});
    EXPECT_TRUE(status.ok());
    EXPECT_EQ("\"TWE=\"", out);
  }
  {
    std::string out;
    Status status;
    std::unique_ptr<JSONParserHandler> writer =
        NewJSONWriter(GetLinuxDevPlatform(), &out, &status);
    writer->HandleBinary({'M'});
    EXPECT_TRUE(status.ok());
    EXPECT_EQ("\"TQ==\"", out);
  }
  {  // "Hello, world.", verified with base64decode.org.
    std::string out;
    Status status;
    std::unique_ptr<JSONParserHandler> writer =
        NewJSONWriter(GetLinuxDevPlatform(), &out, &status);
    writer->HandleBinary(
        {'H', 'e', 'l', 'l', 'o', ',', ' ', 'w', 'o', 'r', 'l', 'd', '.'});
    EXPECT_TRUE(status.ok());
    EXPECT_EQ("\"SGVsbG8sIHdvcmxkLg==\"", out);
  }
}

TEST(JsonStdStringWriterTest, HandlesErrors) {
  // When an error is sent via HandleError, it saves it in the provided
  // status and clears the output.
  std::string out;
  Status status;
  std::unique_ptr<JSONParserHandler> writer =
      NewJSONWriter(GetLinuxDevPlatform(), &out, &status);
  writer->HandleObjectBegin();
  WriteUTF8AsUTF16(writer.get(), "msg1");
  writer->HandleError(Status{Error::JSON_PARSER_VALUE_EXPECTED, 42});
  EXPECT_EQ(Error::JSON_PARSER_VALUE_EXPECTED, status.error);
  EXPECT_EQ(42, status.pos);
  EXPECT_EQ("", out);
}

// We'd use Gmock but unfortunately it only handles copyable return types.
class MockPlatform : public Platform {
 public:
  // Not implemented.
  bool StrToD(const char* str, double* result) const override { return false; }

  // A map with pre-registered responses for DToSTr.
  std::map<double, std::string> dtostr_responses;

  std::unique_ptr<char[]> DToStr(double value) const override {
    auto it = dtostr_responses.find(value);
    assert(it != dtostr_responses.end());
    const std::string& str = it->second;
    std::unique_ptr<char[]> response(new char[str.size() + 1]);
    memcpy(response.get(), str.c_str(), str.size() + 1);
    return response;
  }
};

TEST(JsonStdStringWriterTest, DoubleToString) {
  // This "broken" platform responds without the leading 0 before the
  // decimal dot, so it'd be invalid JSON.
  MockPlatform platform;
  platform.dtostr_responses[.1] = ".1";
  platform.dtostr_responses[-.7] = "-.7";

  std::string out;
  Status status;
  std::unique_ptr<JSONParserHandler> writer =
      NewJSONWriter(&platform, &out, &status);
  writer->HandleArrayBegin();
  writer->HandleDouble(.1);
  writer->HandleDouble(-.7);
  writer->HandleArrayEnd();
  EXPECT_EQ("[0.1,-0.7]", out);
}
}  // namespace inspector_protocol
