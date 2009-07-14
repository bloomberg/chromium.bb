/*
 * Copyright 2008, Google Inc.
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

#include "nacl_sel/ldt.h"
#include <stdio.h>
#include "testing/base/gunit.h"

namespace {
// Test that installed descriptors function as expected.
// The RunGuarded tests also test that descriptors are properly installed and
// that the state is the same at the end as at the beginning of the test.
TEST(LdtTest, DescriptorsPermitAndProhibitAccess) {
  int foo;
  static const DWORD kEndUserAddr = reinterpret_cast<DWORD>(&foo + 1024);
  client3d_nacl_sel::AccessTest atest;
  // First, check that an all-access descriptor returns true for all.
  client3d_nacl_sel::Descriptor readwrite(
      static_cast<DWORD>(0), kEndUserAddr,
      client3d_nacl_sel::Descriptor::READ_WRITE);
  const bool rwresults[4] = { true, true, true, true };
  EXPECT_TRUE(atest.RunGuarded(readwrite, rwresults, NULL));
  // Then, check that a no data access descriptor returns false for all.
  client3d_nacl_sel::Descriptor execonly(
      static_cast<DWORD>(0), kEndUserAddr,
      client3d_nacl_sel::Descriptor::EXECUTE_ONLY);
  const bool xoresults[4] = { false, false, false, false };
  EXPECT_TRUE(atest.RunGuarded(execonly, xoresults, NULL));
}

// The first element of the expected results vector is the result of a read
// to the lower address.  Ensure that RunGuarded returns false if the
// result does not match the expected result for the read from the lower
// address.
TEST(LdtTest, LowAddressReadIsCorrectlyChecked) {
  int foo;
  static const DWORD kEndUserAddr = reinterpret_cast<DWORD>(&foo + 1024);
  client3d_nacl_sel::AccessTest atest;
  // First, check that an all-access descriptor returns true for all.
  client3d_nacl_sel::Descriptor readwrite(
      static_cast<DWORD>(0), kEndUserAddr,
      client3d_nacl_sel::Descriptor::READ_WRITE);
  const bool rwresults[4] = { false, true, true, true };
  EXPECT_FALSE(atest.RunGuarded(readwrite, rwresults, NULL));
  // Then, check that a no data access descriptor returns false for all.
  client3d_nacl_sel::Descriptor execonly(
      static_cast<DWORD>(0), kEndUserAddr,
      client3d_nacl_sel::Descriptor::EXECUTE_ONLY);
  const bool xoresults[4] = { true, false, false, false };
  EXPECT_FALSE(atest.RunGuarded(execonly, xoresults, NULL));
}

// The second element of the expected results vector is the result of a read
// to the higher address.  Ensure that RunGuarded returns false if the
// result does not match the expected result for the read from the higher
// address.
TEST(LdtTest, HighAddressReadIsCorrectlyChecked) {
  int foo;
  static const DWORD kEndUserAddr = reinterpret_cast<DWORD>(&foo + 1024);
  client3d_nacl_sel::AccessTest atest;
  // First, check that an all-access descriptor returns true for all.
  client3d_nacl_sel::Descriptor readwrite(
      static_cast<DWORD>(0), kEndUserAddr,
      client3d_nacl_sel::Descriptor::READ_WRITE);
  const bool rwresults[4] = { true, false, true, true };
  EXPECT_FALSE(atest.RunGuarded(readwrite, rwresults, NULL));
  // Then, check that a no data access descriptor returns false for all.
  client3d_nacl_sel::Descriptor execonly(
      static_cast<DWORD>(0), kEndUserAddr,
      client3d_nacl_sel::Descriptor::EXECUTE_ONLY);
  const bool xoresults[4] = { false, true, false, false };
  EXPECT_FALSE(atest.RunGuarded(execonly, xoresults, NULL));
}

// The third element of the expected results vector is the result of a write
// to the lower address.  Ensure that RunGuarded returns false if the
// result does not match the expected result for the write to the lower
// address.
TEST(LdtTest, LowAddressWriteIsCorrectlyChecked) {
  int foo;
  static const DWORD kEndUserAddr = reinterpret_cast<DWORD>(&foo + 1024);
  client3d_nacl_sel::AccessTest atest;
  // First, check that an all-access descriptor returns true for all.
  client3d_nacl_sel::Descriptor readwrite(
      static_cast<DWORD>(0), kEndUserAddr,
      client3d_nacl_sel::Descriptor::READ_WRITE);
  const bool rwresults[4] = { true, true, false, true };
  EXPECT_FALSE(atest.RunGuarded(readwrite, rwresults, NULL));
  // Then, check that a no data access descriptor returns false for all.
  client3d_nacl_sel::Descriptor execonly(
      static_cast<DWORD>(0), kEndUserAddr,
      client3d_nacl_sel::Descriptor::EXECUTE_ONLY);
  const bool xoresults[4] = { false, false, true, false };
  EXPECT_FALSE(atest.RunGuarded(execonly, xoresults, NULL));
}

// The fourth element of the expected results vector is the result of a write
// to the higher address.  Ensure that RunGuarded returns false if the
// result does not match the expected result for the write to the higher
// address.
TEST(LdtTest, HighAddressWriteIsCorrectlyChecked) {
  int foo;
  static const DWORD kEndUserAddr = reinterpret_cast<DWORD>(&foo + 1024);
  client3d_nacl_sel::AccessTest atest;
  // First, check that an all-access descriptor returns true for all.
  client3d_nacl_sel::Descriptor readwrite(
      static_cast<DWORD>(0), kEndUserAddr,
      client3d_nacl_sel::Descriptor::READ_WRITE);
  const bool rwresults[4] = { true, true, true, false };
  EXPECT_FALSE(atest.RunGuarded(readwrite, rwresults, NULL));
  // Then, check that a no data access descriptor returns false for all.
  client3d_nacl_sel::Descriptor execonly(
      static_cast<DWORD>(0), kEndUserAddr,
      client3d_nacl_sel::Descriptor::EXECUTE_ONLY);
  const bool xoresults[4] = { false, false, false, true };
  EXPECT_FALSE(atest.RunGuarded(execonly, xoresults, NULL));
}
};  // empty namespace

int main(int argc, char **argv) {
  return RUN_ALL_TESTS();
}
