/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// Tests for the bitfield helper class.

#include "gtest/gtest.h"
#include "command_buffer/common/bitfield_helpers.h"

namespace command_buffer {

// Tests that BitField<>::Get returns the right bits.
TEST(BitFieldTest, TestGet) {
  unsigned int value = 0x12345678u;
  EXPECT_EQ(0x8u, (BitField<0, 4>::Get(value)));
  EXPECT_EQ(0x45u, (BitField<12, 8>::Get(value)));
  EXPECT_EQ(0x12345678u, (BitField<0, 32>::Get(value)));
}

// Tests that BitField<>::MakeValue generates the right bits.
TEST(BitFieldTest, TestMakeValue) {
  EXPECT_EQ(0x00000003u, (BitField<0, 4>::MakeValue(0x3)));
  EXPECT_EQ(0x00023000u, (BitField<12, 8>::MakeValue(0x123)));
  EXPECT_EQ(0x87654321u, (BitField<0, 32>::MakeValue(0x87654321)));
}

// Tests that BitField<>::Set modifies the right bits.
TEST(BitFieldTest, TestSet) {
  unsigned int value = 0x12345678u;
  BitField<0, 4>::Set(&value, 0x9);
  EXPECT_EQ(0x12345679u, value);
  BitField<12, 8>::Set(&value, 0x123);
  EXPECT_EQ(0x12323679u, value);
  BitField<0, 32>::Set(&value, 0x87654321);
  EXPECT_EQ(0x87654321u, value);
}

}  // namespace command_buffer
