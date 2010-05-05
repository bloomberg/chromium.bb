// Copyright (c) 2010 Google Inc.
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

// stabs_reader_unittest.cc: Unit tests for google_breakpad::StabsReader.

#include <cassert>
#include <cerrno>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stab.h>

#include "breakpad_googletest_includes.h"
#include "common/stabs_reader.h"
#include "common/test_assembler.h"

using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::_;
using google_breakpad::StabsHandler;
using google_breakpad::StabsReader;
using google_breakpad::TestAssembler::Label;
using google_breakpad::TestAssembler::Section;
using google_breakpad::TestAssembler::kBigEndian;
using google_breakpad::TestAssembler::kLittleEndian;
using std::map;
using std::string;

namespace {

// A StringAssembler is a class for generating .stabstr sections to present
// as input to the STABS parser.
class StringAssembler: public Section {
 public:
  StringAssembler() : in_cu_(false) { StartCU(); }

  // Add the string S to this StringAssembler, and return the string's
  // offset within this compilation unit's strings. If S has been added
  // already, this returns the offset of its first instance.
  size_t Add(const string &s) {
    map<string, size_t>::iterator it = added_.find(s);
    if (it != added_.end())
      return it->second;
    size_t offset = Size() - cu_start_;
    AppendCString(s);
    added_[s] = offset;
    return offset;
  }

  // Start a fresh compilation unit string collection.
  void StartCU() {
    // Ignore duplicate calls to StartCU. Our test data don't always call
    // StartCU at all, meaning that our constructor has to take care of it,
    // meaning that tests that *do* call StartCU call it twice at the
    // beginning.  This is not worth smoothing out.
    if (in_cu_) return;

    added_.clear();
    cu_start_ = Size();

    // Each compilation unit's strings start with an empty string.
    AppendCString("");
    added_[""] = 0;

    in_cu_ = true;
  }
  
  // Finish off the current CU's strings.
  size_t EndCU() { 
    assert(in_cu_);
    in_cu_ = false;
    return Size() - cu_start_;
  }

 private:
  // The offset of the start of this compilation unit's strings.
  size_t cu_start_;

  // True if we're in a CU.
  bool in_cu_;

  // A map from the strings that have been added to this section to
  // their starting indices within their compilation unit.
  map<string, size_t> added_;
};

// A StabsAssembler is a class for generating .stab sections to present as
// test input for the STABS parser.
class StabsAssembler: public Section {
 public:
  // Create a StabsAssembler that uses StringAssembler for its strings.
  StabsAssembler(StringAssembler *string_assembler) 
      : Section(string_assembler->endianness()),
        string_assembler_(string_assembler),
        value_size_(0),
        entry_count_(0),
        cu_header_(NULL) { }
  ~StabsAssembler() { assert(!cu_header_); }

  // Accessor and setter for value_size_.
  size_t value_size() const { return value_size_; }
  StabsAssembler &set_value_size(size_t value_size) {
    value_size_ = value_size;
    return *this;
  }

  // Append a STAB entry to the end of this section with the given
  // characteristics. NAME is the offset of this entry's name string within
  // its compilation unit's portion of the .stabstr section; this can be a
  // value generated by a StringAssembler. Return a reference to this
  // StabsAssembler.
  StabsAssembler &Stab(uint8_t type, uint8_t other, Label descriptor,
                       Label value, Label name) {
    D32(name);
    D8(type);
    D8(other);
    D16(descriptor);
    Append(endianness(), value_size_, value);
    entry_count_++;
    return *this;
  }

  // As above, but automatically add NAME to our StringAssembler.
  StabsAssembler &Stab(uint8_t type, uint8_t other, Label descriptor,
                       Label value, const string &name) {
    return Stab(type, other, descriptor, value, string_assembler_->Add(name));
  }

  // Start a compilation unit named NAME, with an N_UNDF symbol to start
  // it, and its own portion of the string section. Return a reference to
  // this StabsAssembler.
  StabsAssembler &StartCU(const string &name) {
    assert(!cu_header_);
    cu_header_ = new CUHeader;
    string_assembler_->StartCU();
    entry_count_ = 0;
    return Stab(N_UNDF, 0,
                cu_header_->final_entry_count,
                cu_header_->final_string_size,
                string_assembler_->Add(name));
  }

  // Close off the current compilation unit. Return a reference to this
  // StabsAssembler.
  StabsAssembler &EndCU() {
    assert(cu_header_);
    cu_header_->final_entry_count = entry_count_;
    cu_header_->final_string_size = string_assembler_->EndCU();
    delete cu_header_;
    cu_header_ = NULL;
    return *this;
  }
  
 private:
  // Data used in a compilation unit header STAB that we won't know until
  // we've finished the compilation unit.
  struct CUHeader {
    // The final number of entries this compilation unit will hold.
    Label final_entry_count;

    // The final size of this compilation unit's strings.
    Label final_string_size;
  };

  // The strings for our STABS entries.
  StringAssembler *string_assembler_;

  // The size of the 'value' field of stabs entries in this section.
  size_t value_size_;

  // The number of entries in this compilation unit so far.
  size_t entry_count_;

  // Header labels for this compilation unit, if we've started one but not
  // finished it.
  CUHeader *cu_header_;
};

class MockStabsReaderHandler: public StabsHandler {
 public:
  MOCK_METHOD3(StartCompilationUnit,
               bool(const char *, uint64_t, const char *));
  MOCK_METHOD1(EndCompilationUnit, bool(uint64_t));
  MOCK_METHOD2(StartFunction, bool(const std::string &, uint64_t));
  MOCK_METHOD1(EndFunction, bool(uint64_t));
  MOCK_METHOD3(Line, bool(uint64_t, const char *, int));
  void Warning(const char *format, ...) { MockWarning(format); }
  MOCK_METHOD1(MockWarning, void(const char *));
};

// Create a StabsReader to parse the mock stabs data in STABS_ASSEMBLER and
// STRINGS_ASSEMBLER, and pass the parsed information to HANDLER. Use the
// endianness and value size of STABS_ASSEMBLER to parse the data. If all
// goes well, return the result of calling the reader's Process member
// function. Otherwise, return false.
static bool ApplyHandlerToMockStabsData(StabsAssembler *stabs_assembler,
                                        StringAssembler *strings_assembler,
                                        StabsHandler *handler) {
  string stab, stabstr;
  if (!stabs_assembler->GetContents(&stab) ||
      !strings_assembler->GetContents(&stabstr))
    return false;

  // Run the parser on the test input, passing whatever we find to HANDLER.
  StabsReader reader(
      reinterpret_cast<const uint8_t *>(stab.data()),    stab.size(),
      reinterpret_cast<const uint8_t *>(stabstr.data()), stabstr.size(),
      stabs_assembler->endianness() == kBigEndian,
      stabs_assembler->value_size(),
      handler);
  return reader.Process();
}

TEST(StabsReader, MockStabsInput) {
  StringAssembler strings;
  StabsAssembler stabs(&strings);
  stabs.set_endianness(kLittleEndian);
  stabs.set_value_size(4);
  stabs
      .Stab(N_SO,      149, 40232, 0x18a2a72bU, "builddir/")
      .Stab(N_FUN,      83, 50010, 0x91a5353fU, 
            "not the SO with source file name we expected ")
      .Stab(N_SO,      165, 24791, 0xfe69d23cU, "")
      .Stab(N_SO,      184, 34178, 0xca4d883aU, "builddir1/")
      .Stab(N_SO,       83, 40859, 0xd2fe5df3U, "file1.c")
      .Stab(N_LSYM,    147, 39565, 0x60d4bb8aU, "not the FUN we're looking for")
      .Stab(N_FUN,     120, 50271, 0xa049f4b1U, "fun1")
      .Stab(N_BINCL,   150, 15694, 0xef65c659U, 
            "something to ignore in a FUN body")
      .Stab(N_SLINE,   147,  4967, 0xd904b3f, "")
      .Stab(N_SOL,     177, 56135, 0xbd97b1dcU, "header.h")
      .Stab(N_SLINE,   130, 24610, 0x90f145b, "")
      .Stab(N_FUN,      45, 32441, 0xbf27cf93U, 
            "fun2:some stabs type info here:to trim from the name")
      .Stab(N_SLINE,   138, 39002, 0x8148b87, "")
      .Stab(N_SOL,      60, 49318, 0x1d06e025U, "file1.c")
      .Stab(N_SLINE,    29, 52163, 0x6eebbb7, "")
      .Stab(N_SO,      167,  4647, 0xd04b7448U, "")
      .Stab(N_LSYM,     58, 37837, 0xe6b14d37U, "")
      .Stab(N_SO,      152,  7810, 0x11759f10U, "file3.c")
      .Stab(N_SO,      218, 12447, 0x11cfe4b5U, "");

  MockStabsReaderHandler mock_handler;

  {
    InSequence s;

    EXPECT_CALL(mock_handler,
                StartCompilationUnit(StrEq("file1.c"), 0xd2fe5df3U,
                                     StrEq("builddir1/")))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler, StartFunction(StrEq("fun1"), 0xa049f4b1U))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler,
                Line(0xa049f4b1U + 0xd904b3f, StrEq("file1.c"), 4967))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler,
                Line(0xa049f4b1U + 0x90f145b, StrEq("header.h"), 24610))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler, EndFunction(0xbf27cf93U))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler, StartFunction(StrEq("fun2"), 0xbf27cf93U))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler,
                Line(0xbf27cf93U + 0x8148b87, StrEq("header.h"), 39002))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler,
                Line(0xbf27cf93U + 0x6eebbb7, StrEq("file1.c"), 52163))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler, EndFunction(0xd04b7448U))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler, EndCompilationUnit(0xd04b7448U))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler, StartCompilationUnit(StrEq("file3.c"), 
                                                   0x11759f10U, NULL))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler, EndCompilationUnit(0x11cfe4b5U))
        .WillOnce(Return(true));
  }

  ASSERT_TRUE(ApplyHandlerToMockStabsData(&stabs, &strings, &mock_handler));
}

TEST(StabsReader, AbruptCU) {
  StringAssembler strings;
  StabsAssembler stabs(&strings);
  stabs.set_endianness(kBigEndian);
  stabs.set_value_size(4);
  stabs.Stab(N_SO, 177, 23446, 0xbf10d5e4, "file2-1.c");

  MockStabsReaderHandler mock_handler;

  {
    InSequence s;

    EXPECT_CALL(mock_handler,
                StartCompilationUnit(StrEq("file2-1.c"), 0xbf10d5e4, NULL))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler, EndCompilationUnit(NULL))
        .WillOnce(Return(true));
  }

  ASSERT_TRUE(ApplyHandlerToMockStabsData(&stabs, &strings, &mock_handler));
}

TEST(StabsReader, AbruptFunction) {
  MockStabsReaderHandler mock_handler;
  StringAssembler strings;
  StabsAssembler stabs(&strings);
  stabs.set_endianness(kLittleEndian);
  stabs.set_value_size(8);
  stabs
      .Stab(N_SO,      218,   26631,   0xb83ddf10U, "file3-1.c")
      .Stab(N_FUN,     113,   24765,   0xbbd4a145U, "fun3_1");

  {
    InSequence s;

    EXPECT_CALL(mock_handler,
                StartCompilationUnit(StrEq("file3-1.c"), 0xb83ddf10U, NULL))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler, StartFunction(StrEq("fun3_1"), 0xbbd4a145U))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler, EndFunction(NULL))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler, EndCompilationUnit(NULL))
        .WillOnce(Return(true));
  }

  ASSERT_TRUE(ApplyHandlerToMockStabsData(&stabs, &strings, &mock_handler));
}

TEST(StabsReader, NoCU) {
  StringAssembler strings;
  StabsAssembler stabs(&strings);
  stabs.set_endianness(kBigEndian);
  stabs.set_value_size(8);
  stabs.Stab(N_SO, 161, 25673, 0x8f676e7bU, "build-directory/");

  MockStabsReaderHandler mock_handler;

  EXPECT_CALL(mock_handler, StartCompilationUnit(_, _, _))
      .Times(0);
  EXPECT_CALL(mock_handler, StartFunction(_, _))
      .Times(0);

  ASSERT_TRUE(ApplyHandlerToMockStabsData(&stabs, &strings, &mock_handler));
}

TEST(StabsReader, NoCUEnd) {
  StringAssembler strings;
  StabsAssembler stabs(&strings);
  stabs.set_endianness(kBigEndian);
  stabs.set_value_size(8);
  stabs
      .Stab(N_SO,      116,   58280,   0x2f7493c9U, "file5-1.c")
      .Stab(N_SO,      224,   23057,   0xf9f1d50fU, "file5-2.c");

  MockStabsReaderHandler mock_handler;

  {
    InSequence s;

    EXPECT_CALL(mock_handler,
                StartCompilationUnit(StrEq("file5-1.c"), 0x2f7493c9U, NULL))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler, EndCompilationUnit(NULL))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler,
                StartCompilationUnit(StrEq("file5-2.c"), 0xf9f1d50fU, NULL))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler, EndCompilationUnit(NULL))
        .WillOnce(Return(true));
  }

  ASSERT_TRUE(ApplyHandlerToMockStabsData(&stabs, &strings, &mock_handler));
}

TEST(StabsReader, MultipleCUs) {
  StringAssembler strings;
  StabsAssembler stabs(&strings);
  stabs.set_endianness(kBigEndian);
  stabs.set_value_size(4);
  stabs
      .StartCU("antimony")
      .Stab(N_SO,   49, 26043, 0x7e259f1aU, "antimony")
      .Stab(N_FUN, 101, 63253, 0x7fbcccaeU, "arsenic")
      .Stab(N_SO,  124, 37175, 0x80b0014cU, "")
      .EndCU()
      .StartCU("aluminum")
      .Stab(N_SO,   72, 23084, 0x86756839U, "aluminum")
      .Stab(N_FUN,  59,  3305, 0xa8e120b0U, "selenium")
      .Stab(N_SO,  178, 56949, 0xbffff983U, "")
      .EndCU();

  MockStabsReaderHandler mock_handler;

  {
    InSequence s;
    EXPECT_CALL(mock_handler,
                StartCompilationUnit(StrEq("antimony"), 0x7e259f1aU, NULL))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler, StartFunction(Eq("arsenic"), 0x7fbcccaeU))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler, EndFunction(0x80b0014cU))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler, EndCompilationUnit(0x80b0014cU))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler,
                StartCompilationUnit(StrEq("aluminum"), 0x86756839U, NULL))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler, StartFunction(Eq("selenium"), 0xa8e120b0U))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler, EndFunction(0xbffff983U))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler, EndCompilationUnit(0xbffff983U))
        .WillOnce(Return(true));
  }

  ASSERT_TRUE(ApplyHandlerToMockStabsData(&stabs, &strings, &mock_handler));
}

TEST_F(Stabs, FunctionEnd) {
  stabs.set_endianness(kLittleEndian);
  stabs.set_value_size(8);
  stabs
      .Stab(N_SO,    102, 62362, 0x52a830d644cd6942ULL, "compilation unit")
      // This function is terminated by the start of the next function.
      .Stab(N_FUN,   216, 38405, 0xbb5ab70ecdd23bfeULL, "function 1")
      // This function is terminated by an explicit end-of-function stab,
      // whose value is a size in bytes.
      .Stab(N_FUN,   240, 10973, 0xc954de9b8fb3e5e2ULL, "function 2")
      .Stab(N_FUN,    14, 36749, 0xc1ab,     "")
      // This function is terminated by the end of the compilation unit.
      .Stab(N_FUN,   143, 64514, 0xdff98c9a35386e1fULL, "function 3")
      .Stab(N_SO,    164, 60142, 0xfdacb856e78bbf57ULL, "");

  {
    InSequence s;
    EXPECT_CALL(mock_handler,
                StartCompilationUnit(StrEq("compilation unit"),
                                     0x52a830d644cd6942ULL, NULL))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler,
                StartFunction(Eq("function 1"), 0xbb5ab70ecdd23bfeULL))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler, EndFunction(0xc954de9b8fb3e5e2ULL))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler,
                StartFunction(Eq("function 2"), 0xc954de9b8fb3e5e2ULL))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler, EndFunction(0xc954de9b8fb3e5e2ULL + 0xc1ab))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler,
                StartFunction(Eq("function 3"), 0xdff98c9a35386e1fULL))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler, EndFunction(0xfdacb856e78bbf57ULL))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_handler, EndCompilationUnit(0xfdacb856e78bbf57ULL))
        .WillOnce(Return(true));
  }

  ASSERT_TRUE(ApplyHandlerToMockStabsData());
}

// name duplication

} // anonymous namespace
