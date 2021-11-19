// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fpdfapi/parser/cpdf_parser.h"

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_linearized_header.h"
#include "core/fpdfapi/parser/cpdf_object.h"
#include "core/fpdfapi/parser/cpdf_syntax_parser.h"
#include "core/fxcrt/cfx_readonlymemorystream.h"
#include "core/fxcrt/fx_extension.h"
#include "core/fxcrt/fx_stream.h"
#include "core/fxcrt/retain_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/utils/path_service.h"
#include "third_party/base/cxx17_backports.h"

using testing::Return;

namespace {

CPDF_CrossRefTable::ObjectInfo GetObjInfo(const CPDF_Parser& parser,
                                          uint32_t obj_num) {
  const auto* info = parser.GetCrossRefTable()->GetObjectInfo(obj_num);
  return info ? *info : CPDF_CrossRefTable::ObjectInfo();
}

class TestObjectsHolder final : public CPDF_Parser::ParsedObjectsHolder {
 public:
  TestObjectsHolder() = default;
  ~TestObjectsHolder() override = default;

  // CPDF_Parser::ParsedObjectsHolder:
  bool TryInit() override { return true; }
  MOCK_METHOD1(GetOrParseIndirectObject, CPDF_Object*(uint32_t objnum));
};

}  // namespace

// A wrapper class to help test member functions of CPDF_Parser.
class CPDF_TestParser final : public CPDF_Parser {
 public:
  CPDF_TestParser() : CPDF_Parser(&object_holder_) {}
  ~CPDF_TestParser() = default;

  // Setup reading from a file and initial states.
  bool InitTestFromFile(const char* path) {
    RetainPtr<IFX_SeekableReadStream> pFileAccess =
        IFX_SeekableReadStream::CreateFromFilename(path);
    if (!pFileAccess)
      return false;

    // For the test file, the header is set at the beginning.
    SetSyntaxParserForTesting(std::make_unique<CPDF_SyntaxParser>(pFileAccess));
    return true;
  }

  // Setup reading from a buffer and initial states.
  bool InitTestFromBufferWithOffset(pdfium::span<const uint8_t> buffer,
                                    FX_FILESIZE header_offset) {
    SetSyntaxParserForTesting(CPDF_SyntaxParser::CreateForTesting(
        pdfium::MakeRetain<CFX_ReadOnlyMemoryStream>(buffer), header_offset));
    return true;
  }

  bool InitTestFromBuffer(pdfium::span<const uint8_t> buffer) {
    return InitTestFromBufferWithOffset(buffer, 0 /*header_offset*/);
  }

  // Expose protected CPDF_Parser methods for testing.
  using CPDF_Parser::LoadCrossRefV4;
  using CPDF_Parser::ParseLinearizedHeader;
  using CPDF_Parser::ParseStartXRef;
  using CPDF_Parser::RebuildCrossRef;
  using CPDF_Parser::StartParseInternal;

  TestObjectsHolder& object_holder() { return object_holder_; }

 private:
  TestObjectsHolder object_holder_;
};

TEST(cpdf_parser, RebuildCrossRefCorrectly) {
  CPDF_TestParser parser;
  std::string test_file;
  ASSERT_TRUE(PathService::GetTestFilePath("parser_rebuildxref_correct.pdf",
                                           &test_file));
  ASSERT_TRUE(parser.InitTestFromFile(test_file.c_str())) << test_file;

  ASSERT_TRUE(parser.RebuildCrossRef());
  const FX_FILESIZE offsets[] = {0, 15, 61, 154, 296, 374, 450};
  const uint16_t versions[] = {0, 0, 2, 4, 6, 8, 0};
  for (size_t i = 0; i < pdfium::size(offsets); ++i)
    EXPECT_EQ(offsets[i], GetObjInfo(parser, i).pos);
  for (size_t i = 0; i < pdfium::size(versions); ++i)
    EXPECT_EQ(versions[i], GetObjInfo(parser, i).gennum);
}

TEST(cpdf_parser, RebuildCrossRefFailed) {
  CPDF_TestParser parser;
  std::string test_file;
  ASSERT_TRUE(PathService::GetTestFilePath(
      "parser_rebuildxref_error_notrailer.pdf", &test_file));
  ASSERT_TRUE(parser.InitTestFromFile(test_file.c_str())) << test_file;

  ASSERT_FALSE(parser.RebuildCrossRef());
}

TEST(cpdf_parser, LoadCrossRefV4) {
  {
    static const unsigned char kXrefTable[] =
        "xref \n"
        "0 6 \n"
        "0000000003 65535 f \n"
        "0000000017 00000 n \n"
        "0000000081 00000 n \n"
        "0000000000 00007 f \n"
        "0000000331 00000 n \n"
        "0000000409 00000 n \n"
        "trail";  // Needed to end cross ref table reading.
    CPDF_TestParser parser;
    ASSERT_TRUE(parser.InitTestFromBuffer(kXrefTable));

    ASSERT_TRUE(parser.LoadCrossRefV4(0, false));
    static const FX_FILESIZE kOffsets[] = {0, 17, 81, 0, 331, 409};
    static const CPDF_TestParser::ObjectType kTypes[] = {
        CPDF_TestParser::ObjectType::kFree,
        CPDF_TestParser::ObjectType::kNotCompressed,
        CPDF_TestParser::ObjectType::kNotCompressed,
        CPDF_TestParser::ObjectType::kFree,
        CPDF_TestParser::ObjectType::kNotCompressed,
        CPDF_TestParser::ObjectType::kNotCompressed};
    static_assert(pdfium::size(kOffsets) == pdfium::size(kTypes),
                  "kOffsets / kTypes size mismatch");
    for (size_t i = 0; i < pdfium::size(kOffsets); ++i) {
      EXPECT_EQ(kOffsets[i], GetObjInfo(parser, i).pos);
      EXPECT_EQ(kTypes[i], GetObjInfo(parser, i).type);
    }
  }
  {
    static const unsigned char kXrefTable[] =
        "xref \n"
        "0 1 \n"
        "0000000000 65535 f \n"
        "3 1 \n"
        "0000025325 00000 n \n"
        "8 2 \n"
        "0000025518 00002 n \n"
        "0000025635 00000 n \n"
        "12 1 \n"
        "0000025777 00000 n \n"
        "trail";  // Needed to end cross ref table reading.
    CPDF_TestParser parser;
    ASSERT_TRUE(parser.InitTestFromBuffer(kXrefTable));

    ASSERT_TRUE(parser.LoadCrossRefV4(0, false));
    static const FX_FILESIZE kOffsets[] = {0, 0,     0,     25325, 0, 0,    0,
                                           0, 25518, 25635, 0,     0, 25777};
    static const CPDF_TestParser::ObjectType kTypes[] = {
        CPDF_TestParser::ObjectType::kFree,
        CPDF_TestParser::ObjectType::kFree,
        CPDF_TestParser::ObjectType::kFree,
        CPDF_TestParser::ObjectType::kNotCompressed,
        CPDF_TestParser::ObjectType::kFree,
        CPDF_TestParser::ObjectType::kFree,
        CPDF_TestParser::ObjectType::kFree,
        CPDF_TestParser::ObjectType::kFree,
        CPDF_TestParser::ObjectType::kNotCompressed,
        CPDF_TestParser::ObjectType::kNotCompressed,
        CPDF_TestParser::ObjectType::kFree,
        CPDF_TestParser::ObjectType::kFree,
        CPDF_TestParser::ObjectType::kNotCompressed};
    static_assert(pdfium::size(kOffsets) == pdfium::size(kTypes),
                  "kOffsets / kTypes size mismatch");
    for (size_t i = 0; i < pdfium::size(kOffsets); ++i) {
      EXPECT_EQ(kOffsets[i], GetObjInfo(parser, i).pos);
      EXPECT_EQ(kTypes[i], GetObjInfo(parser, i).type);
    }
  }
  {
    static const unsigned char kXrefTable[] =
        "xref \n"
        "0 1 \n"
        "0000000000 65535 f \n"
        "3 1 \n"
        "0000025325 00000 n \n"
        "8 2 \n"
        "0000000000 65535 f \n"
        "0000025635 00000 n \n"
        "12 1 \n"
        "0000025777 00000 n \n"
        "trail";  // Needed to end cross ref table reading.
    CPDF_TestParser parser;
    ASSERT_TRUE(parser.InitTestFromBuffer(kXrefTable));

    ASSERT_TRUE(parser.LoadCrossRefV4(0, false));
    static const FX_FILESIZE kOffsets[] = {0, 0, 0,     25325, 0, 0,    0,
                                           0, 0, 25635, 0,     0, 25777};
    static const CPDF_TestParser::ObjectType kTypes[] = {
        CPDF_TestParser::ObjectType::kFree,
        CPDF_TestParser::ObjectType::kFree,
        CPDF_TestParser::ObjectType::kFree,
        CPDF_TestParser::ObjectType::kNotCompressed,
        CPDF_TestParser::ObjectType::kFree,
        CPDF_TestParser::ObjectType::kFree,
        CPDF_TestParser::ObjectType::kFree,
        CPDF_TestParser::ObjectType::kFree,
        CPDF_TestParser::ObjectType::kFree,
        CPDF_TestParser::ObjectType::kNotCompressed,
        CPDF_TestParser::ObjectType::kFree,
        CPDF_TestParser::ObjectType::kFree,
        CPDF_TestParser::ObjectType::kNotCompressed};
    static_assert(pdfium::size(kOffsets) == pdfium::size(kTypes),
                  "kOffsets / kTypes size mismatch");
    for (size_t i = 0; i < pdfium::size(kOffsets); ++i) {
      EXPECT_EQ(kOffsets[i], GetObjInfo(parser, i).pos);
      EXPECT_EQ(kTypes[i], GetObjInfo(parser, i).type);
    }
  }
  {
    static const unsigned char kXrefTable[] =
        "xref \n"
        "0 7 \n"
        "0000000002 65535 f \n"
        "0000000023 00000 n \n"
        "0000000003 65535 f \n"
        "0000000004 65535 f \n"
        "0000000000 65535 f \n"
        "0000000045 00000 n \n"
        "0000000179 00000 n \n"
        "trail";  // Needed to end cross ref table reading.
    CPDF_TestParser parser;
    ASSERT_TRUE(parser.InitTestFromBuffer(kXrefTable));

    ASSERT_TRUE(parser.LoadCrossRefV4(0, false));
    static const FX_FILESIZE kOffsets[] = {0, 23, 0, 0, 0, 45, 179};
    static const CPDF_TestParser::ObjectType kTypes[] = {
        CPDF_TestParser::ObjectType::kFree,
        CPDF_TestParser::ObjectType::kNotCompressed,
        CPDF_TestParser::ObjectType::kFree,
        CPDF_TestParser::ObjectType::kFree,
        CPDF_TestParser::ObjectType::kFree,
        CPDF_TestParser::ObjectType::kNotCompressed,
        CPDF_TestParser::ObjectType::kNotCompressed};
    static_assert(pdfium::size(kOffsets) == pdfium::size(kTypes),
                  "kOffsets / kTypes size mismatch");
    for (size_t i = 0; i < pdfium::size(kOffsets); ++i) {
      EXPECT_EQ(kOffsets[i], GetObjInfo(parser, i).pos);
      EXPECT_EQ(kTypes[i], GetObjInfo(parser, i).type);
    }
  }
  {
    // Regression test for https://crbug.com/945624 - Make sure the parser
    // can correctly handle table sizes that are multiples of the read size,
    // which is 1024.
    std::string xref_table = "xref \n 0 2048 \n";
    xref_table.reserve(41000);
    for (int i = 0; i < 2048; ++i) {
      char buffer[21];
      snprintf(buffer, sizeof(buffer), "%010d 00000 n \n", i + 1);
      xref_table += buffer;
    }
    xref_table += "trail";  // Needed to end cross ref table reading.
    CPDF_TestParser parser;
    ASSERT_TRUE(parser.InitTestFromBuffer(
        pdfium::make_span(reinterpret_cast<const uint8_t*>(xref_table.c_str()),
                          xref_table.size())));

    ASSERT_TRUE(parser.LoadCrossRefV4(0, false));
    for (size_t i = 0; i < 2048; ++i) {
      EXPECT_EQ(static_cast<int>(i) + 1, GetObjInfo(parser, i).pos);
      EXPECT_EQ(CPDF_TestParser::ObjectType::kNotCompressed,
                GetObjInfo(parser, i).type);
    }
  }
}

TEST(cpdf_parser, ParseStartXRef) {
  CPDF_TestParser parser;
  std::string test_file;
  ASSERT_TRUE(
      PathService::GetTestFilePath("annotation_stamp_with_ap.pdf", &test_file));
  ASSERT_TRUE(parser.InitTestFromFile(test_file.c_str())) << test_file;

  EXPECT_EQ(100940, parser.ParseStartXRef());
  RetainPtr<CPDF_Object> cross_ref_v5_obj =
      parser.ParseIndirectObjectAt(100940, 0);
  ASSERT_TRUE(cross_ref_v5_obj);
  EXPECT_EQ(75u, cross_ref_v5_obj->GetObjNum());
}

TEST(cpdf_parser, ParseStartXRefWithHeaderOffset) {
  static constexpr FX_FILESIZE kTestHeaderOffset = 765;
  std::string test_file;
  ASSERT_TRUE(
      PathService::GetTestFilePath("annotation_stamp_with_ap.pdf", &test_file));
  RetainPtr<IFX_SeekableReadStream> pFileAccess =
      IFX_SeekableReadStream::CreateFromFilename(test_file.c_str());
  ASSERT_TRUE(pFileAccess);

  std::vector<unsigned char> data(pFileAccess->GetSize() + kTestHeaderOffset);
  ASSERT_TRUE(pFileAccess->ReadBlockAtOffset(&data.front() + kTestHeaderOffset,
                                             0, pFileAccess->GetSize()));
  CPDF_TestParser parser;
  parser.InitTestFromBufferWithOffset(data, kTestHeaderOffset);

  EXPECT_EQ(100940, parser.ParseStartXRef());
  RetainPtr<CPDF_Object> cross_ref_v5_obj =
      parser.ParseIndirectObjectAt(100940, 0);
  ASSERT_TRUE(cross_ref_v5_obj);
  EXPECT_EQ(75u, cross_ref_v5_obj->GetObjNum());
}

TEST(cpdf_parser, ParseLinearizedWithHeaderOffset) {
  static constexpr FX_FILESIZE kTestHeaderOffset = 765;
  std::string test_file;
  ASSERT_TRUE(PathService::GetTestFilePath("linearized.pdf", &test_file));
  RetainPtr<IFX_SeekableReadStream> pFileAccess =
      IFX_SeekableReadStream::CreateFromFilename(test_file.c_str());
  ASSERT_TRUE(pFileAccess);

  std::vector<unsigned char> data(pFileAccess->GetSize() + kTestHeaderOffset);
  ASSERT_TRUE(pFileAccess->ReadBlockAtOffset(&data.front() + kTestHeaderOffset,
                                             0, pFileAccess->GetSize()));
  CPDF_TestParser parser;
  parser.InitTestFromBufferWithOffset(data, kTestHeaderOffset);

  EXPECT_TRUE(parser.ParseLinearizedHeader());
}

TEST(cpdf_parser, BadStartXrefShouldNotBuildCrossRefTable) {
  const unsigned char kData[] =
      "%PDF1-7 0 obj <</Size 2 /W [0 0 0]\n>>\n"
      "stream\n"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
      "endstream\n"
      "endobj\n"
      "startxref\n"
      "6\n"
      "%%EOF\n";
  CPDF_TestParser parser;
  ASSERT_TRUE(parser.InitTestFromBuffer(kData));
  EXPECT_EQ(CPDF_Parser::FORMAT_ERROR, parser.StartParseInternal());
  ASSERT_TRUE(parser.GetCrossRefTable());
  EXPECT_EQ(0u, parser.GetCrossRefTable()->objects_info().size());
}

TEST(cpdf_parser, XrefObjectIndicesTooBig) {
  CPDF_TestParser parser;

  // Satisfy CPDF_Parser's checks, so the test data below can concentrate on the
  // /XRef stream and avoid also providing other valid dictionaries.
  auto dummy_root = pdfium::MakeRetain<CPDF_Dictionary>();
  EXPECT_CALL(parser.object_holder(), GetOrParseIndirectObject)
      .WillRepeatedly(Return(dummy_root.Get()));

  // Since /Index starts at 4194303, the object number will go past
  // `kMaxObjectNumber`.
  static_assert(CPDF_Parser::kMaxObjectNumber == 4194304,
                "Unexpected kMaxObjectNumber");
  const unsigned char kData[] =
      "%PDF1-7\n%\xa0\xf2\xa4\xf4\n"
      "7 0 obj <<\n"
      "  /Filter /ASCIIHexDecode\n"
      "  /Index [4194303 3]\n"
      "  /Root 1 0 R\n"
      "  /Size 4194306\n"
      "  /W [1 1 1]\n"
      ">>\n"
      "stream\n"
      "01 00 00\n"
      "01 0F 00\n"
      "01 12 00\n"
      "endstream\n"
      "endobj\n"
      "startxref\n"
      "14\n"
      "%%EOF\n";
  ASSERT_TRUE(parser.InitTestFromBuffer(kData));
  EXPECT_EQ(CPDF_Parser::SUCCESS, parser.StartParseInternal());
  ASSERT_TRUE(parser.GetCrossRefTable());
  const auto& objects_info = parser.GetCrossRefTable()->objects_info();
  EXPECT_EQ(2u, objects_info.size());

  // This should be the only object from table. Subsequent objects have object
  // numbers that are too big.
  auto first_object_it = objects_info.find(4194303);
  ASSERT_NE(first_object_it, objects_info.end());
  EXPECT_EQ(CPDF_Parser::ObjectType::kNormal, first_object_it->second.type);
  EXPECT_EQ(0, first_object_it->second.pos);

  // TODO(thestig): Should the xref table contain object 4194305?
  // Consider reworking CPDF_Parser's object representation to avoid having to
  // store this placeholder object.
  auto placeholder_object_it = objects_info.find(4194305);
  ASSERT_NE(placeholder_object_it, objects_info.end());
  EXPECT_EQ(CPDF_Parser::ObjectType::kFree, placeholder_object_it->second.type);
}

TEST(cpdf_parser, XrefHasInvalidArchiveObjectNumber) {
  CPDF_TestParser parser;

  // Satisfy CPDF_Parser's checks, so the test data below can concentrate on the
  // /XRef stream and avoid also providing other valid dictionaries.
  auto dummy_root = pdfium::MakeRetain<CPDF_Dictionary>();
  EXPECT_CALL(parser.object_holder(), GetOrParseIndirectObject)
      .WillRepeatedly(Return(dummy_root.Get()));

  // 0xFF in the first object in the xref object stream is invalid.
  const unsigned char kData[] =
      "%PDF1-7\n%\xa0\xf2\xa4\xf4\n"
      "7 0 obj <<\n"
      "  /Filter /ASCIIHexDecode\n"
      "  /Root 1 0 R\n"
      "  /Size 3\n"
      "  /W [1 1 1]\n"
      ">>\n"
      "stream\n"
      "02 FF 00\n"
      "01 0F 00\n"
      "01 12 00\n"
      "endstream\n"
      "endobj\n"
      "startxref\n"
      "14\n"
      "%%EOF\n";
  ASSERT_TRUE(parser.InitTestFromBuffer(kData));
  EXPECT_EQ(CPDF_Parser::SUCCESS, parser.StartParseInternal());
  ASSERT_TRUE(parser.GetCrossRefTable());
  const auto& objects_info = parser.GetCrossRefTable()->objects_info();
  EXPECT_EQ(2u, objects_info.size());

  // Skip over the first object, and continue parsing the remaining objects.
  auto second_object_it = objects_info.find(1);
  ASSERT_NE(second_object_it, objects_info.end());
  EXPECT_EQ(CPDF_Parser::ObjectType::kNormal, second_object_it->second.type);
  EXPECT_EQ(15, second_object_it->second.pos);

  auto third_object_it = objects_info.find(2);
  ASSERT_NE(third_object_it, objects_info.end());
  EXPECT_EQ(CPDF_Parser::ObjectType::kNormal, third_object_it->second.type);
  EXPECT_EQ(18, third_object_it->second.pos);
}
