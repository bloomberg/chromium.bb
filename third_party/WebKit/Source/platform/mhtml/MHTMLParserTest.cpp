// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/mhtml/MHTMLParser.h"

#include "platform/SharedBuffer.h"
#include "platform/mhtml/ArchiveResource.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

std::string GetResourceData(
    const HeapVector<Member<ArchiveResource>>& resources,
    size_t index) {
  return std::string(resources[index]->Data()->Data(),
                     resources[index]->Data()->size());
}

}  // namespace

class MHTMLParserTest : public ::testing::Test {
 public:
  MHTMLParserTest() = default;

  HeapVector<Member<ArchiveResource>> ParseArchive(const char* mhtml_data,
                                                   size_t size) {
    scoped_refptr<SharedBuffer> buf = SharedBuffer::Create(mhtml_data, size);
    MHTMLParser parser(buf);
    return parser.ParseArchive();
  }
};

TEST_F(MHTMLParserTest, MHTMLPartHeaders) {
  const char mhtml_data[] =
      "From: <Saved by Blink>\r\n"
      "Subject: Test Subject\r\n"
      "MIME-Version: 1.0\r\n"
      "Content-Type: multipart/related;\r\n"
      "\ttype=\"text/html\";\r\n"
      "\tboundary=\"BoUnDaRy\"\r\n"
      "\r\n"
      "\r\n"
      "--BoUnDaRy\r\n"
      "Content-Location: http://www.example.com/page1\r\n"
      "Content-Transfer-Encoding: quoted-printable\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "single line\r\n"
      "--BoUnDaRy\r\n"
      "Content-Location: http://www.example.com/page2\r\n"
      "Content-ID: <foo-123@mhtml.blink>\r\n"
      "Content-Transfer-Encoding: binary\r\n"
      "Content-Type: text/plain\r\n"
      "\r\n"
      "bin\0ary\r\n"
      "--BoUnDaRy\r\n"
      "Content-Location: http://www.example.com/page3\r\n"
      "Content-Transfer-Encoding: base64\r\n"
      "Content-Type: text/css; charset=ascii\r\n"
      "\r\n"
      "MTIzYWJj\r\n"
      "\r\n"
      "--BoUnDaRy--\r\n";

  HeapVector<Member<ArchiveResource>> resources =
      ParseArchive(mhtml_data, sizeof(mhtml_data));
  EXPECT_EQ(3ul, resources.size());

  EXPECT_EQ("http://www.example.com/page1", resources[0]->Url());
  EXPECT_TRUE(resources[0]->ContentID().IsNull());
  EXPECT_EQ("text/html", resources[0]->MimeType());
  EXPECT_EQ("utf-8", resources[0]->TextEncoding());

  EXPECT_EQ("http://www.example.com/page2", resources[1]->Url());
  EXPECT_EQ("<foo-123@mhtml.blink>", resources[1]->ContentID());
  EXPECT_EQ("text/plain", resources[1]->MimeType());
  EXPECT_TRUE(resources[1]->TextEncoding().IsNull());

  EXPECT_EQ("http://www.example.com/page3", resources[2]->Url());
  EXPECT_TRUE(resources[2]->ContentID().IsNull());
  EXPECT_EQ("text/css", resources[2]->MimeType());
  EXPECT_EQ("ascii", resources[2]->TextEncoding());
}

TEST_F(MHTMLParserTest, QuotedPrintableContentTransferEncoding) {
  const char mhtml_data[] =
      "From: <Saved by Blink>\r\n"
      "Subject: Test Subject\r\n"
      "MIME-Version: 1.0\r\n"
      "Content-Type: multipart/related;\r\n"
      "\ttype=\"text/html\";\r\n"
      "\tboundary=\"BoUnDaRy\"\r\n"
      "\r\n"
      "\r\n"
      "--BoUnDaRy\r\n"
      "Content-Location: http://www.example.com/page1\r\n"
      "Content-Transfer-Encoding: quoted-printable\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "single line\r\n"
      "--BoUnDaRy\r\n"
      "Content-Location: http://www.example.com/page2\r\n"
      "Content-Transfer-Encoding: quoted-printable\r\n"
      "Content-Type: text/plain\r\n"
      "\r\n"
      "long line=3Dbar=3D=\r\n"
      "more\r\n"
      "--BoUnDaRy\r\n"
      "Content-Location: http://www.example.com/page3\r\n"
      "Content-Transfer-Encoding: quoted-printable\r\n"
      "Content-Type: text/css; charset=ascii\r\n"
      "\r\n"
      "first line\r\n"
      "second line\r\n"
      "\r\n"
      "--BoUnDaRy--\r\n";

  HeapVector<Member<ArchiveResource>> resources =
      ParseArchive(mhtml_data, sizeof(mhtml_data));
  EXPECT_EQ(3ul, resources.size());

  EXPECT_EQ("single line\r\n", GetResourceData(resources, 0));
  EXPECT_EQ("long line=bar=more\r\n", GetResourceData(resources, 1));
  EXPECT_EQ("first line\r\nsecond line\r\n\r\n", GetResourceData(resources, 2));
}

TEST_F(MHTMLParserTest, Base64ContentTransferEncoding) {
  const char mhtml_data[] =
      "From: <Saved by Blink>\r\n"
      "Subject: Test Subject\r\n"
      "MIME-Version: 1.0\r\n"
      "Content-Type: multipart/related;\r\n"
      "\ttype=\"text/html\";\r\n"
      "\tboundary=\"BoUnDaRy\"\r\n"
      "\r\n"
      "\r\n"
      "--BoUnDaRy\r\n"
      "Content-Location: http://www.example.com/page1\r\n"
      "Content-Transfer-Encoding: base64\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "MTIzYWJj\r\n"
      "\r\n"
      "--BoUnDaRy\r\n"
      "Content-Location: http://www.example.com/page2\r\n"
      "Content-Transfer-Encoding: base64\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "MTIzYWJj\r\n"
      "AQIDDQ4P\r\n"
      "\r\n"
      "--BoUnDaRy--\r\n";

  HeapVector<Member<ArchiveResource>> resources =
      ParseArchive(mhtml_data, sizeof(mhtml_data));
  EXPECT_EQ(2ul, resources.size());

  EXPECT_EQ("123abc", GetResourceData(resources, 0));
  EXPECT_EQ(std::string("123abc\x01\x02\x03\x0D\x0E\x0F", 12),
            GetResourceData(resources, 1));
}

TEST_F(MHTMLParserTest, EightBitContentTransferEncoding) {
  const char mhtml_data[] =
      "From: <Saved by Blink>\r\n"
      "Subject: Test Subject\r\n"
      "MIME-Version: 1.0\r\n"
      "Content-Type: multipart/related;\r\n"
      "\ttype=\"text/html\";\r\n"
      "\tboundary=\"BoUnDaRy\"\r\n"
      "\r\n"
      "\r\n"
      "--BoUnDaRy\r\n"
      "Content-Location: http://www.example.com/page2\r\n"
      "Content-Transfer-Encoding: 8bit\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "123\r\n"
      "bin\0ary\r\n"
      "\r\n"
      "--BoUnDaRy--\r\n";

  HeapVector<Member<ArchiveResource>> resources =
      ParseArchive(mhtml_data, sizeof(mhtml_data));
  EXPECT_EQ(1ul, resources.size());

  EXPECT_EQ(std::string("123bin\0ary", 10), GetResourceData(resources, 0));
}

TEST_F(MHTMLParserTest, SevenBitContentTransferEncoding) {
  const char mhtml_data[] =
      "From: <Saved by Blink>\r\n"
      "Subject: Test Subject\r\n"
      "MIME-Version: 1.0\r\n"
      "Content-Type: multipart/related;\r\n"
      "\ttype=\"text/html\";\r\n"
      "\tboundary=\"BoUnDaRy\"\r\n"
      "\r\n"
      "\r\n"
      "--BoUnDaRy\r\n"
      "Content-Location: http://www.example.com/page2\r\n"
      "Content-Transfer-Encoding: 7bit\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "\r\n"
      "123\r\n"
      "abcdefg\r\n"
      "\r\n"
      "--BoUnDaRy--\r\n";

  HeapVector<Member<ArchiveResource>> resources =
      ParseArchive(mhtml_data, sizeof(mhtml_data));
  EXPECT_EQ(1ul, resources.size());

  EXPECT_EQ(std::string("123abcdefg", 10), GetResourceData(resources, 0));
}

TEST_F(MHTMLParserTest, BinaryContentTransferEncoding) {
  const char mhtml_data[] =
      "From: <Saved by Blink>\r\n"
      "Subject: Test Subject\r\n"
      "MIME-Version: 1.0\r\n"
      "Content-Type: multipart/related;\r\n"
      "\ttype=\"text/html\";\r\n"
      "\tboundary=\"BoUnDaRy\"\r\n"
      "\r\n"
      "\r\n"
      "--BoUnDaRy\r\n"
      "Content-Location: http://www.example.com/page1\r\n"
      "Content-Transfer-Encoding: binary\r\n"
      "Content-Type: binary/octet-stream\r\n"
      "\r\n"
      "bin\0ary\r\n"
      "--BoUnDaRy\r\n"
      "Content-Location: http://www.example.com/page2\r\n"
      "Content-Transfer-Encoding: binary\r\n"
      "Content-Type: binary/octet-stream\r\n"
      "\r\n"
      "bin\0ary"
      "--BoUnDaRy\r\n"
      "Content-Location: http://www.example.com/page3\r\n"
      "Content-Transfer-Encoding: binary\r\n"
      "Content-Type: binary/octet-stream\r\n"
      "\r\n"
      "--BoUnDaRy--\r\n";

  HeapVector<Member<ArchiveResource>> resources =
      ParseArchive(mhtml_data, sizeof(mhtml_data));
  EXPECT_EQ(3ul, resources.size());

  EXPECT_EQ(std::string("bin\0ary", 7), GetResourceData(resources, 0));
  EXPECT_EQ(std::string("bin\0ary", 7), GetResourceData(resources, 1));
  EXPECT_EQ("", GetResourceData(resources, 2));
}

TEST_F(MHTMLParserTest, UnknownContentTransferEncoding) {
  // Unknown encoding is treated as binary.
  const char mhtml_data[] =
      "From: <Saved by Blink>\r\n"
      "Subject: Test Subject\r\n"
      "MIME-Version: 1.0\r\n"
      "Content-Type: multipart/related;\r\n"
      "\ttype=\"text/html\";\r\n"
      "\tboundary=\"BoUnDaRy\"\r\n"
      "\r\n"
      "\r\n"
      "--BoUnDaRy\r\n"
      "Content-Location: http://www.example.com/page1\r\n"
      "Content-Transfer-Encoding: foo\r\n"
      "Content-Type: binary/octet-stream\r\n"
      "\r\n"
      "bin\0ary\r\n"
      "--BoUnDaRy\r\n"
      "Content-Location: http://www.example.com/page2\r\n"
      "Content-Transfer-Encoding: unknown\r\n"
      "Content-Type: binary/octet-stream\r\n"
      "\r\n"
      "bin\0ary"
      "--BoUnDaRy\r\n"
      "Content-Location: http://www.example.com/page3\r\n"
      "Content-Transfer-Encoding: \r\n"
      "Content-Type: binary/octet-stream\r\n"
      "\r\n"
      "--BoUnDaRy--\r\n";

  HeapVector<Member<ArchiveResource>> resources =
      ParseArchive(mhtml_data, sizeof(mhtml_data));
  EXPECT_EQ(3ul, resources.size());

  EXPECT_EQ(std::string("bin\0ary", 7), GetResourceData(resources, 0));
  EXPECT_EQ(std::string("bin\0ary", 7), GetResourceData(resources, 1));
  EXPECT_EQ("", GetResourceData(resources, 2));
}

TEST_F(MHTMLParserTest, NoContentTransferEncoding) {
  // Missing encoding is treated as binary.
  const char mhtml_data[] =
      "From: <Saved by Blink>\r\n"
      "Subject: Test Subject\r\n"
      "MIME-Version: 1.0\r\n"
      "Content-Type: multipart/related;\r\n"
      "\ttype=\"text/html\";\r\n"
      "\tboundary=\"BoUnDaRy\"\r\n"
      "\r\n"
      "\r\n"
      "--BoUnDaRy\r\n"
      "Content-Location: http://www.example.com/page1\r\n"
      "Content-Type: binary/octet-stream\r\n"
      "\r\n"
      "bin\0ary\r\n"
      "--BoUnDaRy\r\n"
      "Content-Location: http://www.example.com/page2\r\n"
      "Content-Type: binary/octet-stream\r\n"
      "\r\n"
      "bin\0ary"
      "--BoUnDaRy\r\n"
      "Content-Location: http://www.example.com/page3\r\n"
      "Content-Type: binary/octet-stream\r\n"
      "\r\n"
      "--BoUnDaRy--\r\n";

  HeapVector<Member<ArchiveResource>> resources =
      ParseArchive(mhtml_data, sizeof(mhtml_data));
  EXPECT_EQ(3ul, resources.size());

  EXPECT_EQ(std::string("bin\0ary", 7), GetResourceData(resources, 0));
  EXPECT_EQ(std::string("bin\0ary", 7), GetResourceData(resources, 1));
  EXPECT_EQ("", GetResourceData(resources, 2));
}

}  // namespace blink
