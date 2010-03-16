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

// cfi_assembler.cc: Implementation of google_breakpad::CFISection class.
// See cfi_assembler.h for details.

#include <cassert>

#include "common/dwarf/cfi_assembler.h"
#include "common/dwarf/dwarf2enums.h"

namespace google_breakpad {
  
CFISection &CFISection::CIEHeader(u_int64_t code_alignment_factor,
                                  int data_alignment_factor,
                                  unsigned return_address_register,
                                  u_int8_t version,
                                  const string &augmentation,
                                  bool dwarf64) {
  assert(!entry_length_);
  entry_length_ = new PendingLength();

  if (dwarf64) {
    D32(0xffffffff);
    D64(entry_length_->length);
    entry_length_->start = Here();
    D64(0xffffffffffffffffULL);            // CIE distinguished value
  } else {
    D32(entry_length_->length);
    entry_length_->start = Here();
    D32(0xffffffff);                    // CIE distinguished value
  }
  D8(version);
  AppendCString(augmentation);
  ULEB128(code_alignment_factor);
  LEB128(data_alignment_factor);
  if (version == 1)
    D8(return_address_register);
  else
    ULEB128(return_address_register);
  return *this;
}

CFISection &CFISection::FDEHeader(Label cie_pointer,
                                  u_int64_t initial_location,
                                  u_int64_t address_range,
                                  bool dwarf64) {
  assert(!entry_length_);
  entry_length_ = new PendingLength();

  if (dwarf64) {
    D32(0xffffffff);
    D64(entry_length_->length);
    entry_length_->start = Here();
    D64(cie_pointer);
  } else {
    D32(entry_length_->length);
    entry_length_->start = Here();
    D32(cie_pointer);
  }
  Append(endianness(), address_size_, initial_location);
  Append(endianness(), address_size_, address_range);
  return *this;
}

CFISection &CFISection::FinishEntry() {
  assert(entry_length_);
  Align(address_size_, dwarf2reader::DW_CFA_nop);
  entry_length_->length = Here() - entry_length_->start;
  delete entry_length_;
  entry_length_ = NULL;
  return *this;
}

};
