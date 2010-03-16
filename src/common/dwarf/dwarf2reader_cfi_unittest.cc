// Copyright (c) 2010, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Original author: Jim Blandy <jimb@mozilla.com> <jimb@red-bean.com>

// dwarf2reader_cfi_unittest.cc: Unit tests for dwarf2reader::CallFrameInfo

#include <vector>

#include "breakpad_googletest_includes.h"
#include "common/dwarf/bytereader.h"
#include "common/dwarf/cfi_assembler.h"
#include "common/dwarf/dwarf2reader.h"
#include "google_breakpad/common/breakpad_types.h"

using google_breakpad::CFISection;
using google_breakpad::TestAssembler::Label;
using google_breakpad::TestAssembler::kBigEndian;
using google_breakpad::TestAssembler::kLittleEndian;
using google_breakpad::TestAssembler::Section;

using dwarf2reader::ENDIANNESS_BIG;
using dwarf2reader::ENDIANNESS_LITTLE;
using dwarf2reader::ByteReader;
using dwarf2reader::CallFrameInfo;

using std::vector;
using testing::InSequence;
using testing::Return;
using testing::Sequence;
using testing::Test;
using testing::_;

class MockCallFrameInfoHandler: public CallFrameInfo::Handler {
 public:
  MOCK_METHOD6(Entry, bool(size_t offset, uint64 address, uint64 length,
                           uint8 version, const string &augmentation,
                           unsigned return_address));
  MOCK_METHOD2(UndefinedRule, bool(uint64 address, int reg));
  MOCK_METHOD2(SameValueRule, bool(uint64 address, int reg));
  MOCK_METHOD4(OffsetRule, bool(uint64 address, int reg, int base_register,
                                long offset));
  MOCK_METHOD4(ValOffsetRule, bool(uint64 address, int reg, int base_register,
                                   long offset));
  MOCK_METHOD3(RegisterRule, bool(uint64 address, int reg, int base_register));
  MOCK_METHOD3(ExpressionRule, bool(uint64 address, int reg,
                                    const string &expression));
  MOCK_METHOD3(ValExpressionRule, bool(uint64 address, int reg,
                                       const string &expression));
  MOCK_METHOD0(End, bool());
};

class MockCallFrameErrorReporter: public CallFrameInfo::Reporter {
 public:
  MockCallFrameErrorReporter() : Reporter("mock filename", "mock section") { }
  MOCK_METHOD2(Incomplete, void(uint64, CallFrameInfo::EntryKind));
  MOCK_METHOD2(CIEPointerOutOfRange, void(uint64, uint64));
  MOCK_METHOD2(BadCIEId, void(uint64, uint64));
  MOCK_METHOD2(UnrecognizedVersion, void(uint64, int version));
  MOCK_METHOD2(UnrecognizedAugmentation, void(uint64, const string &));
  MOCK_METHOD2(RestoreInCIE, void(uint64, uint64));
  MOCK_METHOD3(BadInstruction, void(uint64, CallFrameInfo::EntryKind, uint64));
  MOCK_METHOD3(NoCFARule, void(uint64, CallFrameInfo::EntryKind, uint64));
  MOCK_METHOD3(EmptyStateStack, void(uint64, CallFrameInfo::EntryKind, uint64));
};

struct CFIFixture {

  enum { kCFARegister = CallFrameInfo::Handler::kCFARegister };

  CFIFixture() {
    // Default expectations for the data handler.
    //
    // - Leave Entry and End without expectations, as it's probably a
    //   good idea to set those explicitly in each test.
    //
    // - Expect the *Rule functions to not be called, 
    //   so that each test can simply list the calls they expect.
    //
    // I gather I could use StrictMock for this, but the manual seems
    // to suggest using that only as a last resort, and this isn't so
    // bad.
    EXPECT_CALL(handler, UndefinedRule(_, _)).Times(0);
    EXPECT_CALL(handler, SameValueRule(_, _)).Times(0);
    EXPECT_CALL(handler, OffsetRule(_, _, _, _)).Times(0);
    EXPECT_CALL(handler, ValOffsetRule(_, _, _, _)).Times(0);
    EXPECT_CALL(handler, RegisterRule(_, _, _)).Times(0);
    EXPECT_CALL(handler, ExpressionRule(_, _, _)).Times(0);
    EXPECT_CALL(handler, ValExpressionRule(_, _, _)).Times(0);

    // Default expectations for the error/warning reporer.
    EXPECT_CALL(reporter, Incomplete(_, _)).Times(0);
    EXPECT_CALL(reporter, CIEPointerOutOfRange(_, _)).Times(0);
    EXPECT_CALL(reporter, BadCIEId(_, _)).Times(0);
    EXPECT_CALL(reporter, UnrecognizedVersion(_, _)).Times(0);
    EXPECT_CALL(reporter, UnrecognizedAugmentation(_, _)).Times(0);
    EXPECT_CALL(reporter, RestoreInCIE(_, _)).Times(0);
    EXPECT_CALL(reporter, BadInstruction(_, _, _)).Times(0);
    EXPECT_CALL(reporter, NoCFARule(_, _, _)).Times(0);
    EXPECT_CALL(reporter, EmptyStateStack(_, _, _)).Times(0);
  }

  MockCallFrameInfoHandler handler;
  MockCallFrameErrorReporter reporter;
};

class CFI: public CFIFixture, public Test { };

TEST_F(CFI, EmptyRegion) {
  EXPECT_CALL(handler, Entry(_, _, _, _, _, _)).Times(0);
  EXPECT_CALL(handler, End()).Times(0);
  static const char data[1] = { 42 };

  ByteReader byte_reader(ENDIANNESS_BIG);
  CallFrameInfo parser(data, 0, &byte_reader, &handler, &reporter);
  EXPECT_TRUE(parser.Start());
}

TEST_F(CFI, IncompleteLength32) {
  CFISection section(kBigEndian, 8);
  section
      // Not even long enough for an initial length.
      .D16(0xa0f)
      // Padding to keep valgrind happy. We subtract these off when we
      // construct the parser.
      .D16(0);

  EXPECT_CALL(handler, Entry(_, _, _, _, _, _)).Times(0);
  EXPECT_CALL(handler, End()).Times(0);

  EXPECT_CALL(reporter, Incomplete(_, CallFrameInfo::kUnknown))
      .WillOnce(Return());

  string contents;
  ASSERT_TRUE(section.GetContents(&contents));

  ByteReader byte_reader(ENDIANNESS_BIG);
  byte_reader.SetAddressSize(8);
  CallFrameInfo parser(contents.data(), contents.size() - 2,
                       &byte_reader, &handler, &reporter);
  EXPECT_FALSE(parser.Start());
}

TEST_F(CFI, IncompleteLength64) {
  CFISection section(kLittleEndian, 4);
  section
      // An incomplete 64-bit DWARF initial length.
      .D32(0xffffffff).D32(0x71fbaec2)
      // Padding to keep valgrind happy. We subtract these off when we
      // construct the parser.
      .D32(0);

  EXPECT_CALL(handler, Entry(_, _, _, _, _, _)).Times(0);
  EXPECT_CALL(handler, End()).Times(0);

  EXPECT_CALL(reporter, Incomplete(_, CallFrameInfo::kUnknown))
      .WillOnce(Return());

  string contents;
  ASSERT_TRUE(section.GetContents(&contents));

  ByteReader byte_reader(ENDIANNESS_LITTLE);
  byte_reader.SetAddressSize(4);
  CallFrameInfo parser(contents.data(), contents.size() - 4,
                       &byte_reader, &handler, &reporter);
  EXPECT_FALSE(parser.Start());
}

TEST_F(CFI, IncompleteId32) {
  CFISection section(kBigEndian, 8);
  section
      .D32(3)                      // Initial length, not long enough for id
      .D8(0xd7).D8(0xe5).D8(0xf1)  // incomplete id
      .CIEHeader(8727, 3983, 8889, 3, "")
      .FinishEntry();

  EXPECT_CALL(handler, Entry(_, _, _, _, _, _)).Times(0);
  EXPECT_CALL(handler, End()).Times(0);

  EXPECT_CALL(reporter, Incomplete(_, CallFrameInfo::kUnknown))
      .WillOnce(Return());

  string contents;
  ASSERT_TRUE(section.GetContents(&contents));

  ByteReader byte_reader(ENDIANNESS_BIG);
  byte_reader.SetAddressSize(8);
  CallFrameInfo parser(contents.data(), contents.size(),
                       &byte_reader, &handler, &reporter);
  EXPECT_FALSE(parser.Start());
}

TEST_F(CFI, BadId32) {
  CFISection section(kBigEndian, 8);
  section
      .D32(0x100)                       // Initial length
      .D32(0xe802fade)                  // bogus ID
      .Append(0x100 - 4, 0x42);         // make the length true
  section
      .CIEHeader(1672, 9872, 8529, 3, "")
      .FinishEntry();

  EXPECT_CALL(handler, Entry(_, _, _, _, _, _)).Times(0);
  EXPECT_CALL(handler, End()).Times(0);

  EXPECT_CALL(reporter, CIEPointerOutOfRange(_, 0xe802fade))
      .WillOnce(Return());

  string contents;
  ASSERT_TRUE(section.GetContents(&contents));

  ByteReader byte_reader(ENDIANNESS_BIG);
  byte_reader.SetAddressSize(8);
  CallFrameInfo parser(contents.data(), contents.size(),
                       &byte_reader, &handler, &reporter);
  EXPECT_FALSE(parser.Start());
}

// A lone CIE shouldn't cause any handler calls.
TEST_F(CFI, SingleCIE) {
  CFISection section(kLittleEndian, 4);
  section.CIEHeader(0xffe799a8, 0x3398dcdd, 0x6e9683de, 3, "");
  section.Append(10, dwarf2reader::DW_CFA_nop);
  section.FinishEntry();

  EXPECT_CALL(handler, Entry(_, _, _, _, _, _)).Times(0);
  EXPECT_CALL(handler, End()).Times(0);

  string contents;
  EXPECT_TRUE(section.GetContents(&contents));
  ByteReader byte_reader(ENDIANNESS_LITTLE);
  byte_reader.SetAddressSize(4);
  CallFrameInfo parser(contents.data(), contents.size(),
                       &byte_reader, &handler, &reporter);
  EXPECT_TRUE(parser.Start());
}

// One FDE, one CIE.
TEST_F(CFI, OneFDE) {
  CFISection section(kBigEndian, 4);
  Label cie;
  section
      .Mark(&cie)
      .CIEHeader(0x4be22f75, 0x2492236e, 0x6b6efb87, 3, "")
      .FinishEntry()
      .FDEHeader(cie, 0x7714740d, 0x3d5a10cd)
      .FinishEntry();

  {
    InSequence s;
    EXPECT_CALL(handler,
                Entry(_, 0x7714740d, 0x3d5a10cd, 3, "", 0x6b6efb87))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End()).WillOnce(Return(true));
  }

  string contents;
  EXPECT_TRUE(section.GetContents(&contents));
  ByteReader byte_reader(ENDIANNESS_BIG);
  byte_reader.SetAddressSize(4);
  CallFrameInfo parser(contents.data(), contents.size(),
                       &byte_reader, &handler, &reporter);
  EXPECT_TRUE(parser.Start());
}

// Two FDEs share a CIE.
TEST_F(CFI, TwoFDEsOneCIE) {
  CFISection section(kBigEndian, 4);
  Label cie;
  section
      // First FDE.
      .FDEHeader(cie, 0xa42744df, 0xa3b42121)
      .FinishEntry()
      // CIE.
      .Mark(&cie)
      .CIEHeader(0x04f7dc7b, 0x3d00c05f, 0xbd43cb59, 3, "")
      .FinishEntry()
      // Second FDE.
      .FDEHeader(cie, 0x6057d391, 0x700f608d)
      .FinishEntry();

  {
    InSequence s;
    EXPECT_CALL(handler,
                Entry(_, 0xa42744df, 0xa3b42121, 3, "", 0xbd43cb59))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End()).WillOnce(Return(true));
  }
  {
    InSequence s;
    EXPECT_CALL(handler,
                Entry(_, 0x6057d391, 0x700f608d, 3, "", 0xbd43cb59))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End()).WillOnce(Return(true));
  }

  string contents;
  EXPECT_TRUE(section.GetContents(&contents));
  ByteReader byte_reader(ENDIANNESS_BIG);
  byte_reader.SetAddressSize(4);
  CallFrameInfo parser(contents.data(), contents.size(),
                       &byte_reader, &handler, &reporter);
  EXPECT_TRUE(parser.Start());
}

// Two FDEs, two CIEs.
TEST_F(CFI, TwoFDEsTwoCIEs) {
  CFISection section(kLittleEndian, 8);
  Label cie1, cie2;
  section
      // First CIE.
      .Mark(&cie1)
      .CIEHeader(0x694d5d45, 0x4233221b, 0xbf45e65a, 3, "")
      .FinishEntry()
      // First FDE which cites second CIE.
      .FDEHeader(cie2, 0x778b27dfe5871f05ULL, 0x324ace3448070926ULL)
      .FinishEntry()
      // Second FDE, which cites first CIE.
      .FDEHeader(cie1, 0xf6054ca18b10bf5fULL, 0x45fdb970d8bca342ULL)
      .FinishEntry()
      // Second CIE.
      .Mark(&cie2)
      .CIEHeader(0xfba3fad7, 0x6287e1fd, 0x61d2c581, 2, "")
      .FinishEntry();

  {
    InSequence s;
    EXPECT_CALL(handler,
                Entry(_, 0x778b27dfe5871f05ULL, 0x324ace3448070926ULL, 2,
                      "", 0x61d2c581))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End()).WillOnce(Return(true));
  }
  {
    InSequence s;
    EXPECT_CALL(handler,
                Entry(_, 0xf6054ca18b10bf5fULL, 0x45fdb970d8bca342ULL, 3,
                      "", 0xbf45e65a))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End()).WillOnce(Return(true));
  }

  string contents;
  EXPECT_TRUE(section.GetContents(&contents));
  ByteReader byte_reader(ENDIANNESS_LITTLE);
  byte_reader.SetAddressSize(8);
  CallFrameInfo parser(contents.data(), contents.size(),
                       &byte_reader, &handler, &reporter);
  EXPECT_TRUE(parser.Start());
}

// An FDE whose CIE specifies a version we don't recognize.
TEST_F(CFI, BadVersion) {
  CFISection section(kBigEndian, 4);
  Label cie1, cie2;
  section
      .Mark(&cie1)
      .CIEHeader(0xca878cf0, 0x7698ec04, 0x7b616f54, 0x52, "")
      .FinishEntry()
      // We should skip this entry, as its CIE specifies a version we
      // don't recognize.
      .FDEHeader(cie1, 0x08852292, 0x2204004a)
      .FinishEntry()
      // Despite the above, we should visit this entry.
      .Mark(&cie2)
      .CIEHeader(0x7c3ae7c9, 0xb9b9a512, 0x96cb3264, 3, "")
      .FinishEntry()
      .FDEHeader(cie2, 0x2094735a, 0x6e875501)
      .FinishEntry();

  EXPECT_CALL(reporter, UnrecognizedVersion(_, 0x52))
    .WillOnce(Return());

  {
    InSequence s;
    // We should see no mention of the first FDE, but we should get
    // a call to Entry for the second.
    EXPECT_CALL(handler, Entry(_, 0x2094735a, 0x6e875501, 3, "",
                               0x96cb3264))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End())
        .WillOnce(Return(true));
  }

  string contents;
  EXPECT_TRUE(section.GetContents(&contents));
  ByteReader byte_reader(ENDIANNESS_BIG);
  byte_reader.SetAddressSize(4);
  CallFrameInfo parser(contents.data(), contents.size(),
                       &byte_reader, &handler, &reporter);
  EXPECT_FALSE(parser.Start());
}

// An FDE whose CIE specifies an augmentation we don't recognize.
TEST_F(CFI, BadAugmentation) {
  CFISection section(kBigEndian, 4);
  Label cie1, cie2;
  section
      .Mark(&cie1)
      .CIEHeader(0x4be22f75, 0x2492236e, 0x6b6efb87, 3, "spaniels!")
      .FinishEntry()
      // We should skip this entry, as its CIE specifies an
      // augmentation we don't recognize.
      .FDEHeader(cie1, 0x7714740d, 0x3d5a10cd)
      .FinishEntry()
      // Despite the above, we should visit this entry.
      .Mark(&cie2)
      .CIEHeader(0xf8bc4399, 0x8cf09931, 0xf2f519b2, 3, "")
      .FinishEntry()
      .FDEHeader(cie2, 0x7bf0fda0, 0xcbcd28d8)
      .FinishEntry();

  EXPECT_CALL(reporter, UnrecognizedAugmentation(_, "spaniels!"))
    .WillOnce(Return());

  {
    InSequence s;
    // We should see no mention of the first FDE, but we should get
    // a call to Entry for the second.
    EXPECT_CALL(handler, Entry(_, 0x7bf0fda0, 0xcbcd28d8, 3, "",
                               0xf2f519b2))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End())
        .WillOnce(Return(true));
  }

  string contents;
  EXPECT_TRUE(section.GetContents(&contents));
  ByteReader byte_reader(ENDIANNESS_BIG);
  byte_reader.SetAddressSize(4);
  CallFrameInfo parser(contents.data(), contents.size(),
                       &byte_reader, &handler, &reporter);
  EXPECT_FALSE(parser.Start());
}

// The return address column field is a byte in CFI version 1
// (DWARF2), but a ULEB128 value in version 3 (DWARF3).
TEST_F(CFI, CIEVersion1ReturnColumn) {
  CFISection section(kBigEndian, 4);
  Label cie;
  section
      // CIE, using the version 1 format: return column is a ubyte.
      .Mark(&cie)
      // Use a value for the return column that is parsed differently
      // as a ubyte and as a ULEB128.
      .CIEHeader(0xbcdea24f, 0x5be28286, 0x9f, 1, "")
      .FinishEntry()
      // FDE, citing that CIE.
      .FDEHeader(cie, 0xb8d347b5, 0x825e55dc)
      .FinishEntry();

  {
    InSequence s;
    EXPECT_CALL(handler, Entry(_, 0xb8d347b5, 0x825e55dc, 1, "", 0x9f))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End()).WillOnce(Return(true));
  }

  string contents;
  EXPECT_TRUE(section.GetContents(&contents));
  ByteReader byte_reader(ENDIANNESS_BIG);
  byte_reader.SetAddressSize(4);
  CallFrameInfo parser(contents.data(), contents.size(),
                       &byte_reader, &handler, &reporter);
  EXPECT_TRUE(parser.Start());
}

// The return address column field is a byte in CFI version 1
// (DWARF2), but a ULEB128 value in version 3 (DWARF3).
TEST_F(CFI, CIEVersion3ReturnColumn) {
  CFISection section(kBigEndian, 4);
  Label cie;
  section
      // CIE, using the version 3 format: return column is a ULEB128.
      .Mark(&cie)
      // Use a value for the return column that is parsed differently
      // as a ubyte and as a ULEB128.
      .CIEHeader(0x0ab4758d, 0xc010fdf7, 0x89, 3, "")
      .FinishEntry()
      // FDE, citing that CIE.
      .FDEHeader(cie, 0x86763f2b, 0x2a66dc23)
      .FinishEntry();

  {
    InSequence s;
    EXPECT_CALL(handler, Entry(_, 0x86763f2b, 0x2a66dc23, 3, "", 0x89))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End()).WillOnce(Return(true));
  }

  string contents;
  EXPECT_TRUE(section.GetContents(&contents));
  ByteReader byte_reader(ENDIANNESS_BIG);
  byte_reader.SetAddressSize(4);
  CallFrameInfo parser(contents.data(), contents.size(),
                       &byte_reader, &handler, &reporter);
  EXPECT_TRUE(parser.Start());
}

struct CFIInsnFixture: public CFIFixture {
  CFIInsnFixture() : CFIFixture() {
    data_factor = 0xb6f;
    return_register = 0x9be1ed9f;
    version = 3;
    cfa_base_register = 0x383a3aa;
    cfa_offset = 0xf748;
  }
  
  // Prepare SECTION to receive FDE instructions.
  //
  // - Append a stock CIE header that establishes the fixture's
  //   code_factor, data_factor, return_register, version, and
  //   augmentation values.
  // - Have the CIE set up a CFA rule using cfa_base_register and
  //   cfa_offset.
  // - Append a stock FDE header, referring to the above CIE, for the
  //   fde_size bytes at fde_start. Choose fde_start and fde_size
  //   appropriately for the section's address size.
  // - Set appropriate expectations on handler in sequence s for the
  //   frame description entry and the CIE's CFA rule.
  //
  // On return, SECTION is ready to have FDE instructions appended to
  // it, and its FinishEntry member called.
  void StockCIEAndFDE(CFISection *section) {
    // Choose appropriate constants for our address size.
    if (section->AddressSize() == 4) {
      fde_start = 0xc628ecfbU;
      fde_size = 0x5dee04a2;
      code_factor = 0x60b;
    } else {
      assert(section->AddressSize() == 8);
      fde_start = 0x0005c57ce7806bd3ULL;
      fde_size = 0x2699521b5e333100ULL;
      code_factor = 0x01008e32855274a8ULL;
    }

    // Create the CIE.
    (*section)
        .Mark(&cie_label)
        .CIEHeader(code_factor, data_factor, return_register, version,
                   "")
        .D8(dwarf2reader::DW_CFA_def_cfa)
        .ULEB128(cfa_base_register)
        .ULEB128(cfa_offset)
        .FinishEntry();

    // Create the FDE.
    section->FDEHeader(cie_label, fde_start, fde_size);

    // Expect an Entry call for the FDE and a ValOffsetRule call for the
    // CIE's CFA rule.
    EXPECT_CALL(handler, Entry(_, fde_start, fde_size, version, "",
                               return_register))
        .InSequence(s)
        .WillOnce(Return(true));
    EXPECT_CALL(handler, ValOffsetRule(fde_start, kCFARegister,
                                       cfa_base_register, cfa_offset))
      .InSequence(s)
      .WillOnce(Return(true));
  }

  // Run the contents of SECTION through a CallFrameInfo parser,
  // expecting parser.Start to return SUCCEEDS
  void ParseSection(CFISection *section, bool succeeds = true) {
    string contents;
    EXPECT_TRUE(section->GetContents(&contents));
    dwarf2reader::Endianness endianness;
    if (section->endianness() == kBigEndian)
      endianness = ENDIANNESS_BIG;
    else {
      assert(section->endianness() == kLittleEndian);
      endianness = ENDIANNESS_LITTLE;
    }
    ByteReader byte_reader(endianness);
    byte_reader.SetAddressSize(section->AddressSize());
    CallFrameInfo parser(contents.data(), contents.size(),
                         &byte_reader, &handler, &reporter);
    if (succeeds)
      EXPECT_TRUE(parser.Start());
    else
      EXPECT_FALSE(parser.Start());
  }

  Label cie_label;
  Sequence s;
  uint64 code_factor;
  int data_factor;
  unsigned return_register;
  unsigned version;
  unsigned cfa_base_register;
  int cfa_offset;
  uint64 fde_start, fde_size;
};

class CFIInsn: public CFIInsnFixture, public Test { };

TEST_F(CFIInsn, DW_CFA_set_loc) {
  CFISection section(kBigEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_set_loc).D32(0xb1ee3e7a)
      // Use DW_CFA_def_cfa to force a handler call that we can use to
      // check the effect of the DW_CFA_set_loc.
      .D8(dwarf2reader::DW_CFA_def_cfa).ULEB128(0x4defb431).ULEB128(0x6d17b0ee)
      .FinishEntry();

  EXPECT_CALL(handler,
              ValOffsetRule(0xb1ee3e7a, kCFARegister, 0x4defb431, 0x6d17b0ee))
      .InSequence(s)
      .WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_advance_loc) {
  CFISection section(kBigEndian, 8);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_advance_loc | 0x2a)
      // Use DW_CFA_def_cfa to force a handler call that we can use to
      // check the effect of the DW_CFA_advance_loc.
      .D8(dwarf2reader::DW_CFA_def_cfa).ULEB128(0x5bbb3715).ULEB128(0x0186c7bf)
      .FinishEntry();

  EXPECT_CALL(handler,
              ValOffsetRule(fde_start + 0x2a * code_factor,
                            kCFARegister, 0x5bbb3715, 0x0186c7bf))
        .InSequence(s)
        .WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_advance_loc1) {
  CFISection section(kLittleEndian, 8);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_advance_loc1).D8(0xd8)
      .D8(dwarf2reader::DW_CFA_def_cfa).ULEB128(0x69d5696a).ULEB128(0x1eb7fc93)
      .FinishEntry();

  EXPECT_CALL(handler,
              ValOffsetRule((fde_start + 0xd8 * code_factor),
                            kCFARegister, 0x69d5696a, 0x1eb7fc93))
      .InSequence(s)
      .WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_advance_loc2) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_advance_loc2).D16(0x3adb)
      .D8(dwarf2reader::DW_CFA_def_cfa).ULEB128(0x3a368bed).ULEB128(0x3194ee37)
      .FinishEntry();

  EXPECT_CALL(handler,
              ValOffsetRule((fde_start + 0x3adb * code_factor),
                            kCFARegister, 0x3a368bed, 0x3194ee37))
      .InSequence(s)
      .WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_advance_loc4) {
  CFISection section(kBigEndian, 8);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_advance_loc4).D32(0x15813c88)
      .D8(dwarf2reader::DW_CFA_def_cfa).ULEB128(0x135270c5).ULEB128(0x24bad7cb)
      .FinishEntry();

  EXPECT_CALL(handler,
              ValOffsetRule((fde_start + 0x15813c88ULL * code_factor),
                            kCFARegister, 0x135270c5, 0x24bad7cb))
      .InSequence(s)
      .WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_MIPS_advance_loc8) {
  code_factor = 0x2d;
  CFISection section(kBigEndian, 8);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_MIPS_advance_loc8).D64(0x3c4f3945b92c14ULL)
      .D8(dwarf2reader::DW_CFA_def_cfa).ULEB128(0xe17ed602).ULEB128(0x3d162e7f)
      .FinishEntry();

  EXPECT_CALL(handler,
              ValOffsetRule((fde_start + 0x3c4f3945b92c14ULL * code_factor),
                            kCFARegister, 0xe17ed602, 0x3d162e7f))
      .InSequence(s)
      .WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_def_cfa) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_def_cfa).ULEB128(0x4e363a85).ULEB128(0x815f9aa7)
      .FinishEntry();

  EXPECT_CALL(handler,
              ValOffsetRule(fde_start, kCFARegister, 0x4e363a85, 0x815f9aa7))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_def_cfa_sf) {
  CFISection section(kBigEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_def_cfa_sf).ULEB128(0x8ccb32b7).LEB128(0x9ea)
      .D8(dwarf2reader::DW_CFA_def_cfa_sf).ULEB128(0x9b40f5da).LEB128(-0x40a2)
      .FinishEntry();

  EXPECT_CALL(handler,
              ValOffsetRule(fde_start, kCFARegister, 0x8ccb32b7,
                            0x9ea * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler,
              ValOffsetRule(fde_start, kCFARegister, 0x9b40f5da,
                            -0x40a2 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_def_cfa_register) {
  CFISection section(kLittleEndian, 8);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_def_cfa_register).ULEB128(0x3e7e9363)
      .FinishEntry();

  EXPECT_CALL(handler,
              ValOffsetRule(fde_start, kCFARegister, 0x3e7e9363, cfa_offset))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

// DW_CFA_def_cfa_register should have no effect when applied to a
// non-base/offset rule.
TEST_F(CFIInsn, DW_CFA_def_cfa_registerBadRule) {
  CFISection section(kBigEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_def_cfa_expression).Block("needle in a haystack")
      .D8(dwarf2reader::DW_CFA_def_cfa_register).ULEB128(0xf1b49e49)
      .FinishEntry();

  EXPECT_CALL(handler,
              ValExpressionRule(fde_start, kCFARegister,
                                "needle in a haystack"))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_def_cfa_offset) {
  CFISection section(kBigEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_def_cfa_offset).ULEB128(0x1e8e3b9b)
      .FinishEntry();

  EXPECT_CALL(handler,
              ValOffsetRule(fde_start, kCFARegister, cfa_base_register,
                            0x1e8e3b9b))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_def_cfa_offset_sf) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_def_cfa_offset_sf).LEB128(0x970)
      .D8(dwarf2reader::DW_CFA_def_cfa_offset_sf).LEB128(-0x2cd)
      .FinishEntry();

  EXPECT_CALL(handler,
              ValOffsetRule(fde_start, kCFARegister, cfa_base_register,
                            0x970 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler,
              ValOffsetRule(fde_start, kCFARegister, cfa_base_register,
                            -0x2cd * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

// DW_CFA_def_cfa_offset should have no effect when applied to a
// non-base/offset rule.
TEST_F(CFIInsn, DW_CFA_def_cfa_offsetBadRule) {
  CFISection section(kBigEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_def_cfa_expression).Block("six ways to Sunday")
      .D8(dwarf2reader::DW_CFA_def_cfa_offset).ULEB128(0x1e8e3b9b)
      .FinishEntry();

  EXPECT_CALL(handler,
              ValExpressionRule(fde_start, kCFARegister, "six ways to Sunday"))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_def_cfa_expression) {
  CFISection section(kLittleEndian, 8);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_def_cfa_expression).Block("eating crow")
      .FinishEntry();

  EXPECT_CALL(handler, ValExpressionRule(fde_start, kCFARegister,
                                         "eating crow"))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_undefined) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_undefined).ULEB128(0x300ce45d)
      .FinishEntry();

  EXPECT_CALL(handler, UndefinedRule(fde_start, 0x300ce45d))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_same_value) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_same_value).ULEB128(0x3865a760)
      .FinishEntry();

  EXPECT_CALL(handler, SameValueRule(fde_start, 0x3865a760))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_offset) {
  CFISection section(kBigEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_offset | 0x2c).ULEB128(0x9f6)
      .FinishEntry();

  EXPECT_CALL(handler,
              OffsetRule(fde_start, 0x2c, kCFARegister, 0x9f6 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_offset_extended) {
  CFISection section(kBigEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_offset_extended).ULEB128(0x402b).ULEB128(0xb48)
      .FinishEntry();

  EXPECT_CALL(handler,
              OffsetRule(fde_start, 0x402b, kCFARegister, 0xb48 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_offset_extended_sf) {
  CFISection section(kBigEndian, 8);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_offset_extended_sf)
          .ULEB128(0x997c23ee).LEB128(0x2d00)
      .D8(dwarf2reader::DW_CFA_offset_extended_sf)
          .ULEB128(0x9519eb82).LEB128(-0xa77)
      .FinishEntry();

  EXPECT_CALL(handler,
              OffsetRule(fde_start, 0x997c23ee,
                         kCFARegister, 0x2d00 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler,
              OffsetRule(fde_start, 0x9519eb82,
                         kCFARegister, -0xa77 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_val_offset) {
  CFISection section(kBigEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_val_offset).ULEB128(0x623562fe).ULEB128(0x673)
      .FinishEntry();

  EXPECT_CALL(handler,
              ValOffsetRule(fde_start, 0x623562fe,
                            kCFARegister, 0x673 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_val_offset_sf) {
  CFISection section(kBigEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_val_offset_sf).ULEB128(0x6f4f).LEB128(0xaab)
      .D8(dwarf2reader::DW_CFA_val_offset_sf).ULEB128(0x2483).LEB128(-0x8a2)
      .FinishEntry();

  EXPECT_CALL(handler,
              ValOffsetRule(fde_start, 0x6f4f,
                            kCFARegister, 0xaab * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler,
              ValOffsetRule(fde_start, 0x2483,
                            kCFARegister, -0x8a2 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_register) {
  CFISection section(kLittleEndian, 8);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_register).ULEB128(0x278d18f9).ULEB128(0x1a684414)
      .FinishEntry();

  EXPECT_CALL(handler, RegisterRule(fde_start, 0x278d18f9, 0x1a684414))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_expression) {
  CFISection section(kBigEndian, 8);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_expression).ULEB128(0xa1619fb2)
      .Block("plus ça change, plus c'est la même chose")
      .FinishEntry();

  EXPECT_CALL(handler,
              ExpressionRule(fde_start, 0xa1619fb2,
                             "plus ça change, plus c'est la même chose"))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_val_expression) {
  CFISection section(kBigEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_val_expression).ULEB128(0xc5e4a9e3)
      .Block("he who has the gold makes the rules")
      .FinishEntry();

  EXPECT_CALL(handler,
              ValExpressionRule(fde_start, 0xc5e4a9e3,
                                "he who has the gold makes the rules"))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_restore) {
  CFISection section(kLittleEndian, 8);
  code_factor = 0x01bd188a9b1fa083ULL;
  data_factor = -0x1ac8;
  return_register = 0x8c35b049;
  version = 2;
  fde_start = 0x2d70fe998298bbb1ULL;
  fde_size = 0x46ccc2e63cf0b108ULL;
  Label cie;
  section
      .Mark(&cie)
      .CIEHeader(code_factor, data_factor, return_register, version,
                 "")
      // Provide a CFA rule, because register rules require them.
      .D8(dwarf2reader::DW_CFA_def_cfa).ULEB128(0x6ca1d50e).ULEB128(0x372e38e8)
      // Provide an offset(N) rule for register 0x3c.
      .D8(dwarf2reader::DW_CFA_offset | 0x3c).ULEB128(0xb348)
      .FinishEntry()
      // In the FDE...
      .FDEHeader(cie, fde_start, fde_size)
      // At a second address, provide a new offset(N) rule for register 0x3c.
      .D8(dwarf2reader::DW_CFA_advance_loc | 0x13)
      .D8(dwarf2reader::DW_CFA_offset | 0x3c).ULEB128(0x9a50)
      // At a third address, restore the original rule for register 0x3c.
      .D8(dwarf2reader::DW_CFA_advance_loc | 0x01)
      .D8(dwarf2reader::DW_CFA_restore | 0x3c)
      .FinishEntry();

  {
    InSequence s;
    EXPECT_CALL(handler,
                Entry(_, fde_start, fde_size, version, "", return_register))
        .WillOnce(Return(true));
    // CIE's CFA rule.
    EXPECT_CALL(handler,
                ValOffsetRule(fde_start, kCFARegister, 0x6ca1d50e, 0x372e38e8))
        .WillOnce(Return(true));
    // CIE's rule for register 0x3c.
    EXPECT_CALL(handler,
                OffsetRule(fde_start, 0x3c, kCFARegister, 0xb348 * data_factor))
        .WillOnce(Return(true));
    // FDE's rule for register 0x3c.
    EXPECT_CALL(handler,
                OffsetRule(fde_start + 0x13 * code_factor, 0x3c,
                           kCFARegister, 0x9a50 * data_factor))
        .WillOnce(Return(true));
    // Restore CIE's rule for register 0x3c.
    EXPECT_CALL(handler,
                OffsetRule(fde_start + (0x13 + 0x01) * code_factor, 0x3c,
                           kCFARegister, 0xb348 * data_factor))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End()).WillOnce(Return(true));
  }
    
  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_restoreNoRule) {
  CFISection section(kBigEndian, 4);
  code_factor = 0x005f78143c1c3b82ULL;
  data_factor = 0x25d0;
  return_register = 0xe8;
  version = 1;
  fde_start = 0x4062e30f;
  fde_size = 0x5302a389;
  Label cie;
  section
      .Mark(&cie)
      .CIEHeader(code_factor, data_factor, return_register, version, "")
      // Provide a CFA rule, because register rules require them.
      .D8(dwarf2reader::DW_CFA_def_cfa).ULEB128(0x470aa334).ULEB128(0x099ef127)
      .FinishEntry()
      // In the FDE...
      .FDEHeader(cie, fde_start, fde_size)
      // At a second address, provide an offset(N) rule for register 0x2c.
      .D8(dwarf2reader::DW_CFA_advance_loc | 0x7)
      .D8(dwarf2reader::DW_CFA_offset | 0x2c).ULEB128(0x1f47)
      // At a third address, restore the (missing) CIE rule for register 0x2c.
      .D8(dwarf2reader::DW_CFA_advance_loc | 0xb)
      .D8(dwarf2reader::DW_CFA_restore | 0x2c)
      .FinishEntry();

  {
    InSequence s;
    EXPECT_CALL(handler,
                Entry(_, fde_start, fde_size, version, "", return_register))
        .WillOnce(Return(true));
    // CIE's CFA rule.
    EXPECT_CALL(handler,
                ValOffsetRule(fde_start, kCFARegister, 0x470aa334, 0x099ef127))
        .WillOnce(Return(true));
    // FDE's rule for register 0x2c.
    EXPECT_CALL(handler,
                OffsetRule(fde_start + 0x7 * code_factor, 0x2c,
                           kCFARegister, 0x1f47 * data_factor))
        .WillOnce(Return(true));
    // Restore CIE's (missing) rule for register 0x2c.
    EXPECT_CALL(handler,
                SameValueRule(fde_start + (0x7 + 0xb) * code_factor, 0x2c))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End()).WillOnce(Return(true));
  }
    
  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_restore_extended) {
  CFISection section(kBigEndian, 4);
  code_factor = 0x126e;
  data_factor = -0xd8b;
  return_register = 0x77711787;
  version = 3;
  fde_start = 0x01f55a45;
  fde_size = 0x452adb80;
  Label cie;
  section
      .Mark(&cie)
      .CIEHeader(code_factor, data_factor, return_register, version,
                 "", true /* dwarf64 */ )
      // Provide a CFA rule, because register rules require them.
      .D8(dwarf2reader::DW_CFA_def_cfa).ULEB128(0x56fa0edd).ULEB128(0x097f78a5)
      // Provide an offset(N) rule for register 0x0f9b8a1c.
      .D8(dwarf2reader::DW_CFA_offset_extended)
          .ULEB128(0x0f9b8a1c).ULEB128(0xc979)
      .FinishEntry()
      // In the FDE...
      .FDEHeader(cie, fde_start, fde_size)
      // At a second address, provide a new offset(N) rule for reg 0x0f9b8a1c.
      .D8(dwarf2reader::DW_CFA_advance_loc | 0x3)
      .D8(dwarf2reader::DW_CFA_offset_extended)
          .ULEB128(0x0f9b8a1c).ULEB128(0x3b7b)
      // At a third address, restore the original rule for register 0x0f9b8a1c.
      .D8(dwarf2reader::DW_CFA_advance_loc | 0x04)
      .D8(dwarf2reader::DW_CFA_restore_extended).ULEB128(0x0f9b8a1c)
      .FinishEntry();

  {
    InSequence s;
    EXPECT_CALL(handler,
                Entry(_, fde_start, fde_size, version, "", return_register))
        .WillOnce(Return(true));
    // CIE's CFA rule.
    EXPECT_CALL(handler,
                ValOffsetRule(fde_start, kCFARegister, 0x56fa0edd, 0x097f78a5))
        .WillOnce(Return(true));
    // CIE's rule for register 0x0f9b8a1c.
    EXPECT_CALL(handler,
                OffsetRule(fde_start, 0x0f9b8a1c, kCFARegister,
                           0xc979 * data_factor))
        .WillOnce(Return(true));
    // FDE's rule for register 0x0f9b8a1c.
    EXPECT_CALL(handler,
                OffsetRule(fde_start + 0x3 * code_factor, 0x0f9b8a1c,
                           kCFARegister, 0x3b7b * data_factor))
        .WillOnce(Return(true));
    // Restore CIE's rule for register 0x0f9b8a1c.
    EXPECT_CALL(handler,
                OffsetRule(fde_start + (0x3 + 0x4) * code_factor, 0x0f9b8a1c,
                           kCFARegister, 0xc979 * data_factor))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End()).WillOnce(Return(true));
  }
    
  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_remember_and_restore_state) {
  CFISection section(kLittleEndian, 8);
  StockCIEAndFDE(&section);

  // We create a state, save it, modify it, and then restore. We
  // refer to the state that is overridden the restore as the
  // "outgoing" state, and the restored state the "incoming" state.
  //
  // Register         outgoing        incoming        expect
  // 1                offset(N)       no rule         new "same value" rule
  // 2                register(R)     offset(N)       report changed rule
  // 3                offset(N)       offset(M)       report changed offset
  // 4                offset(N)       offset(N)       no report
  // 5                offset(N)       no rule         new "same value" rule
  section
      // Create the "incoming" state, which we will save and later restore.
      .D8(dwarf2reader::DW_CFA_offset | 2).ULEB128(0x9806)
      .D8(dwarf2reader::DW_CFA_offset | 3).ULEB128(0x995d)
      .D8(dwarf2reader::DW_CFA_offset | 4).ULEB128(0x7055)
      .D8(dwarf2reader::DW_CFA_remember_state)
      // Advance to a new instruction; an implementation could legitimately
      // ignore all but the final rule for a given register at a given address.
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      // Create the "outgoing" state, which we will discard.
      .D8(dwarf2reader::DW_CFA_offset | 1).ULEB128(0xea1a)
      .D8(dwarf2reader::DW_CFA_register).ULEB128(2).ULEB128(0x1d2a3767)
      .D8(dwarf2reader::DW_CFA_offset | 3).ULEB128(0xdd29)
      .D8(dwarf2reader::DW_CFA_offset | 5).ULEB128(0xf1ce)
      // At a third address, restore the incoming state.
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_restore_state)
      .FinishEntry();

  uint64 addr = fde_start;

  // Expect the incoming rules to be reported.
  EXPECT_CALL(handler, OffsetRule(addr, 2, kCFARegister, 0x9806 * data_factor))
    .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, OffsetRule(addr, 3, kCFARegister, 0x995d * data_factor))
    .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, OffsetRule(addr, 4, kCFARegister, 0x7055 * data_factor))
    .InSequence(s).WillOnce(Return(true));

  addr += code_factor;

  // After the save, we establish the outgoing rule set.
  EXPECT_CALL(handler, OffsetRule(addr, 1, kCFARegister, 0xea1a * data_factor))
    .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, RegisterRule(addr, 2, 0x1d2a3767))
    .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, OffsetRule(addr, 3, kCFARegister, 0xdd29 * data_factor))
    .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, OffsetRule(addr, 5, kCFARegister, 0xf1ce * data_factor))
    .InSequence(s).WillOnce(Return(true));

  addr += code_factor;

  // Finally, after the restore, expect to see the differences from
  // the outgoing to the incoming rules reported.
  EXPECT_CALL(handler, SameValueRule(addr, 1))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, OffsetRule(addr, 2, kCFARegister, 0x9806 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, OffsetRule(addr, 3, kCFARegister, 0x995d * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, SameValueRule(addr, 5))
      .InSequence(s).WillOnce(Return(true));

  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

// Check that restoring a rule set reports changes to the CFA rule.
TEST_F(CFIInsn, DW_CFA_remember_and_restore_stateCFA) {
  CFISection section(kBigEndian, 4);
  StockCIEAndFDE(&section);

  section
      .D8(dwarf2reader::DW_CFA_remember_state)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_def_cfa_offset).ULEB128(0x90481102)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, ValOffsetRule(fde_start + code_factor, kCFARegister,
                                     cfa_base_register, 0x90481102))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, ValOffsetRule(fde_start + code_factor * 2, kCFARegister,
                                     cfa_base_register, cfa_offset))
      .InSequence(s).WillOnce(Return(true));

  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_nop) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_nop)
      .D8(dwarf2reader::DW_CFA_def_cfa).ULEB128(0x3fb8d4f1).ULEB128(0x078dc67b)
      .D8(dwarf2reader::DW_CFA_nop)
      .FinishEntry();

  EXPECT_CALL(handler,
              ValOffsetRule(fde_start, kCFARegister, 0x3fb8d4f1, 0x078dc67b))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_GNU_window_save) {
  CFISection section(kBigEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_GNU_window_save)
      .FinishEntry();

  // Don't include all the rules in any particular sequence.

  // The caller's %o0-%o7 have become the callee's %i0-%i7. This is
  // the GCC register numbering.
  for (int i = 8; i < 16; i++)
    EXPECT_CALL(handler, RegisterRule(fde_start, i, i + 16))
        .WillOnce(Return(true));
  // The caller's %l0-%l7 and %i0-%i7 have been saved at the top of
  // its frame.
  for (int i = 16; i < 32; i++)
    EXPECT_CALL(handler, OffsetRule(fde_start, i, kCFARegister, (i-16) * 4))
        .WillOnce(Return(true));

  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_GNU_args_size) {
  CFISection section(kLittleEndian, 8);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_GNU_args_size).ULEB128(0xeddfa520)
      // Verify that we see this, meaning we parsed the above properly.
      .D8(dwarf2reader::DW_CFA_offset | 0x23).ULEB128(0x269)
      .FinishEntry();

  EXPECT_CALL(handler,
              OffsetRule(fde_start, 0x23, kCFARegister, 0x269 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIInsn, DW_CFA_GNU_negative_offset_extended) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_GNU_negative_offset_extended)
      .ULEB128(0x430cc87a).ULEB128(0x613)
      .FinishEntry();

  EXPECT_CALL(handler,
              OffsetRule(fde_start, 0x430cc87a,
                         kCFARegister, -0x613 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).InSequence(s).WillOnce(Return(true));

  ParseSection(&section);
}

// Three FDEs: skip the second
TEST_F(CFIInsn, SkipFDE) {
  CFISection section(kBigEndian, 4);
  Label cie;
  section
      // CIE, used by all FDEs.
      .Mark(&cie)
      .CIEHeader(0x010269f2, 0x9177, 0xedca5849, 2, "")
      .D8(dwarf2reader::DW_CFA_def_cfa).ULEB128(0x42ed390b).ULEB128(0x98f43aad)
      .FinishEntry()
      // First FDE.
      .FDEHeader(cie, 0xa870ebdd, 0x60f6aa4)
      .D8(dwarf2reader::DW_CFA_register).ULEB128(0x3a860351).ULEB128(0x6c9a6bcf)
      .FinishEntry()
      // Second FDE.
      .FDEHeader(cie, 0xc534f7c0, 0xf6552e9, true /* dwarf64 */)
      .D8(dwarf2reader::DW_CFA_register).ULEB128(0x1b62c234).ULEB128(0x26586b18)
      .FinishEntry()
      // Third FDE.
      .FDEHeader(cie, 0xf681cfc8, 0x7e4594e)
      .D8(dwarf2reader::DW_CFA_register).ULEB128(0x26c53934).ULEB128(0x18eeb8a4)
      .FinishEntry();

  {
    InSequence s;

    // Process the first FDE.
    EXPECT_CALL(handler, Entry(_, 0xa870ebdd, 0x60f6aa4, 2, "", 0xedca5849))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, ValOffsetRule(0xa870ebdd, kCFARegister,
                                       0x42ed390b, 0x98f43aad))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, RegisterRule(0xa870ebdd, 0x3a860351, 0x6c9a6bcf))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End())
        .WillOnce(Return(true));

    // Skip the second FDE.
    EXPECT_CALL(handler, Entry(_, 0xc534f7c0, 0xf6552e9, 2, "", 0xedca5849))
        .WillOnce(Return(false));

    // Process the third FDE.
    EXPECT_CALL(handler, Entry(_, 0xf681cfc8, 0x7e4594e, 2, "", 0xedca5849))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, ValOffsetRule(0xf681cfc8, kCFARegister,
                                       0x42ed390b, 0x98f43aad))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, RegisterRule(0xf681cfc8, 0x26c53934, 0x18eeb8a4))
        .WillOnce(Return(true));
    EXPECT_CALL(handler, End())
        .WillOnce(Return(true));
  }

  ParseSection(&section);
}

// Quit processing in the middle of an entry's instructions.
TEST_F(CFIInsn, QuitMidentry) {
  CFISection section(kLittleEndian, 8);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_register).ULEB128(0xe0cf850d).ULEB128(0x15aab431)
      .D8(dwarf2reader::DW_CFA_expression).ULEB128(0x46750aa5).Block("meat")
      .FinishEntry();

  EXPECT_CALL(handler, RegisterRule(fde_start, 0xe0cf850d, 0x15aab431))
      .InSequence(s).WillOnce(Return(false));
  EXPECT_CALL(handler, End())
      .InSequence(s).WillOnce(Return(true));
  
  ParseSection(&section, false);
}

class CFIRestore: public CFIInsnFixture, public Test { };

TEST_F(CFIRestore, RestoreUndefinedRuleUnchanged) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_undefined).ULEB128(0x0bac878e)
      .D8(dwarf2reader::DW_CFA_remember_state)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, UndefinedRule(fde_start, 0x0bac878e))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIRestore, RestoreUndefinedRuleChanged) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_undefined).ULEB128(0x7dedff5f)
      .D8(dwarf2reader::DW_CFA_remember_state)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_same_value).ULEB128(0x7dedff5f)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, UndefinedRule(fde_start, 0x7dedff5f))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, SameValueRule(fde_start + code_factor, 0x7dedff5f))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, UndefinedRule(fde_start + 2 * code_factor, 0x7dedff5f))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIRestore, RestoreSameValueRuleUnchanged) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_same_value).ULEB128(0xadbc9b3a)
      .D8(dwarf2reader::DW_CFA_remember_state)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, SameValueRule(fde_start, 0xadbc9b3a))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIRestore, RestoreSameValueRuleChanged) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_same_value).ULEB128(0x3d90dcb5)
      .D8(dwarf2reader::DW_CFA_remember_state)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_undefined).ULEB128(0x3d90dcb5)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, SameValueRule(fde_start, 0x3d90dcb5))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, UndefinedRule(fde_start + code_factor, 0x3d90dcb5))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, SameValueRule(fde_start + 2 * code_factor, 0x3d90dcb5))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIRestore, RestoreOffsetRuleUnchanged) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_offset | 0x14).ULEB128(0xb6f)
      .D8(dwarf2reader::DW_CFA_remember_state)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, OffsetRule(fde_start, 0x14,
                                  kCFARegister, 0xb6f * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIRestore, RestoreOffsetRuleChanged) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_offset | 0x21).ULEB128(0xeb7)
      .D8(dwarf2reader::DW_CFA_remember_state)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_undefined).ULEB128(0x21)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, OffsetRule(fde_start, 0x21,
                                  kCFARegister, 0xeb7 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, UndefinedRule(fde_start + code_factor, 0x21))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, OffsetRule(fde_start + 2 * code_factor, 0x21,
                                  kCFARegister, 0xeb7 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIRestore, RestoreOffsetRuleChangedOffset) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_offset | 0x21).ULEB128(0x134)
      .D8(dwarf2reader::DW_CFA_remember_state)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_offset | 0x21).ULEB128(0xf4f)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, OffsetRule(fde_start, 0x21,
                                  kCFARegister, 0x134 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, OffsetRule(fde_start + code_factor, 0x21,
                                  kCFARegister, 0xf4f * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, OffsetRule(fde_start + 2 * code_factor, 0x21,
                                  kCFARegister, 0x134 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIRestore, RestoreValOffsetRuleUnchanged) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_val_offset).ULEB128(0x829caee6).ULEB128(0xe4c)
      .D8(dwarf2reader::DW_CFA_remember_state)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, ValOffsetRule(fde_start, 0x829caee6,
                                  kCFARegister, 0xe4c * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIRestore, RestoreValOffsetRuleChanged) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_val_offset).ULEB128(0xf17c36d6).ULEB128(0xeb7)
      .D8(dwarf2reader::DW_CFA_remember_state)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_undefined).ULEB128(0xf17c36d6)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, ValOffsetRule(fde_start, 0xf17c36d6,
                                  kCFARegister, 0xeb7 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, UndefinedRule(fde_start + code_factor, 0xf17c36d6))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, ValOffsetRule(fde_start + 2 * code_factor, 0xf17c36d6,
                                  kCFARegister, 0xeb7 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIRestore, RestoreValOffsetRuleChangedValOffset) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_val_offset).ULEB128(0x2cf0ab1b).ULEB128(0x562)
      .D8(dwarf2reader::DW_CFA_remember_state)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_val_offset).ULEB128(0x2cf0ab1b).ULEB128(0xe88)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, ValOffsetRule(fde_start, 0x2cf0ab1b,
                                  kCFARegister, 0x562 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, ValOffsetRule(fde_start + code_factor, 0x2cf0ab1b,
                                  kCFARegister, 0xe88 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, ValOffsetRule(fde_start + 2 * code_factor, 0x2cf0ab1b,
                                  kCFARegister, 0x562 * data_factor))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIRestore, RestoreRegisterRuleUnchanged) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_register).ULEB128(0x77514acc).ULEB128(0x464de4ce)
      .D8(dwarf2reader::DW_CFA_remember_state)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, RegisterRule(fde_start, 0x77514acc, 0x464de4ce))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIRestore, RestoreRegisterRuleChanged) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_register).ULEB128(0xe39acce5).ULEB128(0x095f1559)
      .D8(dwarf2reader::DW_CFA_remember_state)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_undefined).ULEB128(0xe39acce5)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, RegisterRule(fde_start, 0xe39acce5, 0x095f1559))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, UndefinedRule(fde_start + code_factor, 0xe39acce5))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, RegisterRule(fde_start + 2 * code_factor, 0xe39acce5,
                                    0x095f1559))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIRestore, RestoreRegisterRuleChangedRegister) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_register).ULEB128(0xd40e21b1).ULEB128(0x16607d6a)
      .D8(dwarf2reader::DW_CFA_remember_state)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_register).ULEB128(0xd40e21b1).ULEB128(0xbabb4742)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, RegisterRule(fde_start, 0xd40e21b1, 0x16607d6a))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, RegisterRule(fde_start + code_factor, 0xd40e21b1,
                                    0xbabb4742))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, RegisterRule(fde_start + 2 * code_factor, 0xd40e21b1,
                                    0x16607d6a))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIRestore, RestoreExpressionRuleUnchanged) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_expression).ULEB128(0x666ae152).Block("dwarf")
      .D8(dwarf2reader::DW_CFA_remember_state)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, ExpressionRule(fde_start, 0x666ae152, "dwarf"))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIRestore, RestoreExpressionRuleChanged) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_expression).ULEB128(0xb5ca5c46).Block("elf")
      .D8(dwarf2reader::DW_CFA_remember_state)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_undefined).ULEB128(0xb5ca5c46)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, ExpressionRule(fde_start, 0xb5ca5c46, "elf"))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, UndefinedRule(fde_start + code_factor, 0xb5ca5c46))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, ExpressionRule(fde_start + 2 * code_factor, 0xb5ca5c46,
                                      "elf"))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIRestore, RestoreExpressionRuleChangedExpression) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_expression).ULEB128(0x500f5739).Block("smurf")
      .D8(dwarf2reader::DW_CFA_remember_state)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_expression).ULEB128(0x500f5739).Block("orc")
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, ExpressionRule(fde_start, 0x500f5739, "smurf"))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, ExpressionRule(fde_start + code_factor, 0x500f5739,
                                      "orc"))
      .InSequence(s).WillOnce(Return(true));
  // Expectations are not wishes.
  EXPECT_CALL(handler, ExpressionRule(fde_start + 2 * code_factor, 0x500f5739,
                                      "smurf"))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIRestore, RestoreValExpressionRuleUnchanged) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_val_expression).ULEB128(0x666ae152)
      .Block("hideous")
      .D8(dwarf2reader::DW_CFA_remember_state)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, ValExpressionRule(fde_start, 0x666ae152, "hideous"))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIRestore, RestoreValExpressionRuleChanged) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_val_expression).ULEB128(0xb5ca5c46)
      .Block("revolting")
      .D8(dwarf2reader::DW_CFA_remember_state)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_undefined).ULEB128(0xb5ca5c46)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, ValExpressionRule(fde_start, 0xb5ca5c46, "revolting"))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, UndefinedRule(fde_start + code_factor, 0xb5ca5c46))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, ValExpressionRule(fde_start + 2 * code_factor, 0xb5ca5c46,
                                      "revolting"))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

TEST_F(CFIRestore, RestoreValExpressionRuleChangedValExpression) {
  CFISection section(kLittleEndian, 4);
  StockCIEAndFDE(&section);
  section
      .D8(dwarf2reader::DW_CFA_val_expression).ULEB128(0x500f5739)
      .Block("repulsive")
      .D8(dwarf2reader::DW_CFA_remember_state)
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_val_expression).ULEB128(0x500f5739)
      .Block("nauseous")
      .D8(dwarf2reader::DW_CFA_advance_loc | 1)
      .D8(dwarf2reader::DW_CFA_restore_state)
      .FinishEntry();

  EXPECT_CALL(handler, ValExpressionRule(fde_start, 0x500f5739, "repulsive"))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, ValExpressionRule(fde_start + code_factor, 0x500f5739,
                                      "nauseous"))
      .InSequence(s).WillOnce(Return(true));
  // Expectations are not wishes.
  EXPECT_CALL(handler, ValExpressionRule(fde_start + 2 * code_factor, 0x500f5739,
                                      "repulsive"))
      .InSequence(s).WillOnce(Return(true));
  EXPECT_CALL(handler, End()).WillOnce(Return(true));

  ParseSection(&section);
}

// These tests require manual inspection of the test output.
struct CFIReporterFixture {
  CFIReporterFixture() : reporter("test file name", "test section name") { }
  CallFrameInfo::Reporter reporter;
};

class CFIReporter: public CFIReporterFixture, public Test { };

TEST_F(CFIReporter, Incomplete) {
  reporter.Incomplete(0x0102030405060708ULL, CallFrameInfo::kUnknown);
}

TEST_F(CFIReporter, CIEPointerOutOfRange) {
  reporter.CIEPointerOutOfRange(0x0123456789abcdefULL, 0xfedcba9876543210ULL);
}

TEST_F(CFIReporter, BadCIEId) {
  reporter.BadCIEId(0x0123456789abcdefULL, 0xfedcba9876543210ULL);
}

TEST_F(CFIReporter, UnrecognizedVersion) {
  reporter.UnrecognizedVersion(0x0123456789abcdefULL, 43);
}

TEST_F(CFIReporter, UnrecognizedAugmentation) {
  reporter.UnrecognizedAugmentation(0x0123456789abcdefULL, "poodles");
}

TEST_F(CFIReporter, RestoreInCIE) {
  reporter.RestoreInCIE(0x0123456789abcdefULL, 0xfedcba9876543210ULL);
}

TEST_F(CFIReporter, BadInstruction) {
  reporter.BadInstruction(0x0123456789abcdefULL, CallFrameInfo::kFDE,
                          0xfedcba9876543210ULL);
}

TEST_F(CFIReporter, NoCFARule) {
  reporter.NoCFARule(0x0123456789abcdefULL, CallFrameInfo::kCIE,
                     0xfedcba9876543210ULL);
}

TEST_F(CFIReporter, EmptyStateStack) {
  reporter.EmptyStateStack(0x0123456789abcdefULL, CallFrameInfo::kFDE,
                           0xfedcba9876543210ULL);
}

TEST_F(CFIReporter, ClearingCFARule) {
  reporter.ClearingCFARule(0x0123456789abcdefULL, CallFrameInfo::kFDE,
                           0xfedcba9876543210ULL);
}
