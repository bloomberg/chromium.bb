/* -*- c++ -*- */
/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/serialization/serialization.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "native_client/src/shared/platform/nacl_check.h"

int main(void) {
  nacl::SerializationBuffer buf;

  int8_t i8 = -128;
  uint8_t u8 = 127;
  int16_t i16 = -32768;
  uint16_t u16 = 65535;
  int32_t i32 = -2147483647 - 1;
  uint32_t u32 = 4294967295U;
  int64_t i64 = -2147483649LL;
  uint64_t u64 = 18446744073709551615ULL;

  // Check basic serialization/deserialization -- we get back what we
  // put in -- with various basic types and vectors.  Test with some
  // extreme numerical values.
  CHECK(buf.Serialize<int16_t>(10));
  CHECK(buf.Serialize(i8));
  CHECK(buf.Serialize(u8));
  CHECK(buf.Serialize(i16));
  CHECK(buf.Serialize(u16));
  CHECK(buf.Serialize(i32));
  CHECK(buf.Serialize(u32));
  CHECK(buf.Serialize(i64));
  CHECK(buf.Serialize(u64));

  int8_t ci8 = 0;
  uint8_t cu8 = 0;
  int16_t ci16 = 0;
  uint16_t cu16 = 0;
  int32_t ci32 = 0;
  uint32_t cu32 = 0;
  int64_t ci64 = 0;
  uint64_t cu64 = 0;

  CHECK(buf.Deserialize(&ci16));
  CHECK(ci16 == 10);
#define D(v) do {                               \
    CHECK(buf.Deserialize(&c ## v));            \
    CHECK(c ## v == v);                         \
  } while (0)
  D(i8);
  D(u8);
  D(i16);
  D(u16);
  D(i32);
  D(u32);
  D(i64);
  D(u64);

  CHECK(!buf.Deserialize(&ci8));
  buf.rewind();

  std::vector<int32_t> v;
  v.push_back(i32);
  v.push_back(3);
  v.push_back(1);
  v.push_back(4);
  v.push_back(1);
  v.push_back(5);
  v.push_back(9);

  CHECK(buf.Serialize(v));

  CHECK(buf.Serialize("Hello world"));
  CHECK(buf.Serialize(
            "When in the Course of human events, it becomes necessary for"
            " one people to dissolve the political bands which have"
            " connected them with another, and to assume among the powers"
            " of the earth, the separate and equal station to which the"
            " Laws of Nature and of Nature's God entitle them, a decent"
            " respect to the opinions of mankind requires that they should"
            " declare the causes which impel them to the separation."));

  std::string msg("Goodbye cruel world");

  CHECK(buf.Serialize(msg));

  std::vector<std::string> vs;

  vs.push_back("When in the Course of human events, it becomes necessary for");
  vs.push_back(" one people to dissolve the political bands which have");
  vs.push_back(" connected them with another, and to assume among the powers");
  vs.push_back(" of the earth, the separate and equal station to which the");
  vs.push_back(" Laws of Nature and of Nature's God entitle them, a decent");
  vs.push_back(" respect to the opinions of mankind requires that they should");
  vs.push_back(" declare the causes which impel them to the separation.");

  CHECK(buf.Serialize(vs));

  buf.rewind();

  CHECK(buf.Deserialize(&ci16));
  CHECK(ci16 == 10);
  D(i8);
  D(u8);
  D(i16);
  D(u16);
  D(i32);
  D(u32);
  D(i64);
  D(u64);

  std::vector<int32_t> v2;
  CHECK(buf.Deserialize(&v2));
  CHECK(v.size() == v2.size());
  for (size_t ix = 0; ix < v.size(); ++ix) {
    CHECK(v[ix] == v2[ix]);
  }
  char buffer[64];
  size_t nbytes = sizeof buffer;
  CHECK(buf.Deserialize(buffer, &nbytes));
  CHECK(nbytes == strlen("Hello world") + 1);
  CHECK(!strcmp(buffer,  "Hello world"));
  char *obuf;
  CHECK(buf.Deserialize(&obuf));
  CHECK(!strcmp(obuf,
                "When in the Course of human events, it becomes necessary for"
                " one people to dissolve the political bands which have"
                " connected them with another, and to assume among the powers"
                " of the earth, the separate and equal station to which the"
                " Laws of Nature and of Nature's God entitle them, a decent"
                " respect to the opinions of mankind requires that they should"
                " declare the causes which impel them to the separation."));

  delete[] obuf;

  std::string msg2;
  CHECK(buf.Deserialize(&msg2));
  CHECK(msg2 == "Goodbye cruel world");

  std::vector<std::string> vs2;

  CHECK(buf.Deserialize(&vs2));

  CHECK(vs.size() == vs2.size());
  for (size_t ix = 0; ix < vs.size(); ++ix) {
    CHECK(vs[ix] == vs2[ix]);
  }

  // Check the ability to construct a SerializationBuffer from
  // "received data".

  buf.reset();
  CHECK(buf.Serialize(i8));
  CHECK(buf.Serialize(u8));
  CHECK(buf.Serialize(i16));
  CHECK(buf.Serialize(u16));
  CHECK(buf.Serialize(i32));
  CHECK(buf.Serialize(u32));
  CHECK(buf.Serialize(i64));
  CHECK(buf.Serialize(u64));
  CHECK(buf.Serialize(vs));

  nacl::SerializationBuffer buf2(buf.data(), buf.num_bytes());
#define D2(v) do {                    \
    CHECK(buf2.Deserialize(&c ## v)); \
    CHECK(c ## v == v);               \
  } while (0)
  D2(i8);
  D2(u8);
  D2(i16);
  D2(u16);
  D2(i32);
  D2(u32);
  D2(i64);
  D2(u64);

  vs2.clear();
  CHECK(buf2.Deserialize(&vs2));

  CHECK(vs.size() == vs2.size());
  for (size_t ix = 0; ix < vs.size(); ++ix) {
    CHECK(vs[ix] == vs2[ix]);
  }

  // Tests that use exposed implementation details.

  // Verify that the space needed to serialize vectors of a basic type
  // grows at the size of the basic type.

  buf.reset();
  CHECK(buf.Serialize(v));
  size_t v_size = buf.num_bytes();
  buf.reset();
  v.push_back(100);
  CHECK(buf.Serialize(v));
  v.pop_back();
  size_t v_prime_size = buf.num_bytes();
  CHECK(v_size + sizeof(int32_t) == v_prime_size);

  // Check typetag is offset based
  CHECK(buf.data()[0] >= nacl::kVectorOffset);

  CHECK(nacl::kRecursiveVector < nacl::kVectorOffset);

  // Check typetag is fixed, using recursive serialization
  buf.reset();
  std::vector<std::vector<int32_t> > vv;
  vv.push_back(v);
  vv.push_back(v);
  CHECK(buf.Serialize(vv));
  CHECK(buf.data()[0] == nacl::kRecursiveVector);

  // Check that the encoding space usage grows as expected, with the
  // nested vector also tagged etc.  TODO(bsy): omit this test? this
  // may be too much implementation detail.
  size_t vv_size = buf.num_bytes();
  std::vector<int32_t> v_singleton;
  v_singleton.push_back(42);
  vv.push_back(v_singleton);
  buf.reset();
  CHECK(buf.Serialize(vv));
  size_t vv_plus_1_size = buf.num_bytes();
  CHECK(vv_size + nacl::SerializationBuffer::kTagBytes + 2 * sizeof(int32_t)
        == vv_plus_1_size);

  return 0;
}
