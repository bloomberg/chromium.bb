// Copyright 2015 Google Inc.
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

#ifndef CLIENT_LINUX_HANDLER_MICRODUMP_EXTRA_INFO_H_
#define CLIENT_LINUX_HANDLER_MICRODUMP_EXTRA_INFO_H_

namespace google_breakpad {

struct MicrodumpExtraInfo {
  // Strings pointed to by this struct are not copied, and are
  // expected to remain valid for the lifetime of the process.
  const char* build_fingerprint;
  const char* product_info;
  const char* gpu_fingerprint;
  const char* process_type;

  // |interest_range_start| and |interest_range_end| specify a range
  // in the target process address space. Microdumps are only
  // generated if the PC or a word on the captured stack point into
  // this range, or |suppress_microdump_based_on_interest_range| is
  // false.
  bool suppress_microdump_based_on_interest_range;
  uintptr_t interest_range_start;
  uintptr_t interest_range_end;

  MicrodumpExtraInfo()
      : build_fingerprint(NULL),
        product_info(NULL),
        gpu_fingerprint(NULL),
        process_type(NULL),
        suppress_microdump_based_on_interest_range(false),
        interest_range_start(0),
        interest_range_end(0) {}
};

}

#endif  // CLIENT_LINUX_HANDLER_MICRODUMP_EXTRA_INFO_H_
