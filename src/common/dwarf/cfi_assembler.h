// -*- mode: C++ -*-

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

// cfi-assembler.h: Define CFISection, a class for creating properly
// (and improperly) formatted DWARF CFI data for unit tests.

#ifndef PROCESSOR_CFI_ASSEMBLER_H_
#define PROCESSOR_CFI_ASSEMBLER_H_

#include <string>

#include "google_breakpad/common/breakpad_types.h"
#include "processor/test_assembler.h"

namespace google_breakpad {

using google_breakpad::TestAssembler::Endianness;
using google_breakpad::TestAssembler::Label;
using google_breakpad::TestAssembler::Section;
using std::string;

class CFISection: public Section {
 public:
  // Create a CFISection whose endianness is ENDIANNESS, and where
  // machine addresses are ADDRESS_SIZE bytes long.
  CFISection(Endianness endianness, size_t address_size)
      : Section(endianness), address_size_(address_size),
        entry_length_(NULL) {
    // The 'start', 'Here', and 'Mark' members of a CFISection all refer
    // to section offsets.
    start() = 0;
  }

  // Return this CFISection's address size.
  size_t AddressSize() const { return address_size_; }

  // Append a Common Information Entry header to this section with the
  // given values. If dwarf64 is true, use the 64-bit DWARF initial
  // length format for the CIE's initial length. Return a reference to
  // this section. You should call FinishEntry after writing the last
  // instruction for the CIE.
  //
  // Before calling this function, you will typically want to use Mark
  // or Here to make a label to pass to FDEHeader that refers to this
  // CIE's position in the section.
  CFISection &CIEHeader(u_int64_t code_alignment_factor,
                        int data_alignment_factor,
                        unsigned return_address_register,
                        u_int8_t version = 3,
                        const string &augmentation = "",
                        bool dwarf64 = false);

  // Append a Frame Description Entry header to this section with the
  // given values. If dwarf64 is true, use the 64-bit DWARF initial
  // length format for the CIE's initial length. Return a reference to
  // this section. You should call FinishEntry after writing the last
  // instruction for the CIE.
  //
  // This function doesn't support entries that are longer than
  // 0xffffff00 bytes. (The "initial length" is always a 32-bit
  // value.) Nor does it support .debug_frame sections longer than
  // 0xffffff00 bytes.
  CFISection &FDEHeader(Label cie_pointer,
                        u_int64_t initial_location,
                        u_int64_t address_range,
                        bool dwarf64 = false);

  // Note the current position as the end of the last CIE or FDE we
  // started, after padding with DW_CFA_nops for alignment. This
  // defines the label representing the entry's length, cited in the
  // entry's header. Return a reference to this section.
  CFISection &FinishEntry();

  // Append the contents of BLOCK as a DW_FORM_block value: an
  // unsigned LEB128 length, followed by that many bytes of data.
  CFISection &Block(const string &block) {
    ULEB128(block.size());
    Append(block);
    return *this;
  }

  // Restate some member functions, to keep chaining working nicely.
  CFISection &Mark(Label *label)   { Section::Mark(label); return *this; }
  CFISection &D8(u_int8_t v)       { Section::D8(v);       return *this; }
  CFISection &D16(u_int16_t v)     { Section::D16(v);      return *this; }
  CFISection &D16(Label v)         { Section::D16(v);      return *this; }
  CFISection &D32(u_int32_t v)     { Section::D32(v);      return *this; }
  CFISection &D32(const Label &v)  { Section::D32(v);      return *this; }
  CFISection &D64(u_int64_t v)     { Section::D64(v);      return *this; }
  CFISection &D64(const Label &v)  { Section::D64(v);      return *this; }
  CFISection &LEB128(long long v)  { Section::LEB128(v);   return *this; }
  CFISection &ULEB128(u_int64_t v) { Section::ULEB128(v);  return *this; }

 private:
  // A length value that we've appended to the section, but is not yet
  // known. LENGTH is the appended value; START is a label referring
  // to the start of the data whose length was cited.
  struct PendingLength {
    Label length;
    Label start;
  };

  // The size of a machine address for the data in this section.
  size_t address_size_;

  // The length value for the current entry.
  //
  // Oddly, this must be dynamically allocated. Labels never get new
  // values; they only acquire constraints on the value they already
  // have, or assert if you assign them something incompatible. So
  // each header needs truly fresh Label objects to cite in their
  // headers and track their positions. The alternative is explicit
  // destructor invocation and a placement new. Ick.
  PendingLength *entry_length_;
};

}  // namespace google_breakpad

#endif  // PROCESSOR_CFI_ASSEMBLER_H_
