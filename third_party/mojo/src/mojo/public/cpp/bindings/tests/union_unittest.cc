// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/fixed_buffer.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/interfaces/bindings/tests/test_unions.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {

TEST(UnionTest, PlainOldDataGetterSetter) {
  PodUnionPtr pod(PodUnion::New());

  pod->set_f_int8(10);
  EXPECT_EQ(10, pod->get_f_int8());
  EXPECT_TRUE(pod->is_f_int8());
  EXPECT_FALSE(pod->is_f_int8_other());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_INT8);

  pod->set_f_uint8(11);
  EXPECT_EQ(11, pod->get_f_uint8());
  EXPECT_TRUE(pod->is_f_uint8());
  EXPECT_FALSE(pod->is_f_int8());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_UINT8);

  pod->set_f_int16(12);
  EXPECT_EQ(12, pod->get_f_int16());
  EXPECT_TRUE(pod->is_f_int16());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_INT16);

  pod->set_f_uint16(13);
  EXPECT_EQ(13, pod->get_f_uint16());
  EXPECT_TRUE(pod->is_f_uint16());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_UINT16);

  pod->set_f_int32(14);
  EXPECT_EQ(14, pod->get_f_int32());
  EXPECT_TRUE(pod->is_f_int32());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_INT32);

  pod->set_f_uint32(static_cast<uint32_t>(15));
  EXPECT_EQ(static_cast<uint32_t>(15), pod->get_f_uint32());
  EXPECT_TRUE(pod->is_f_uint32());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_UINT32);

  pod->set_f_int64(16);
  EXPECT_EQ(16, pod->get_f_int64());
  EXPECT_TRUE(pod->is_f_int64());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_INT64);

  pod->set_f_uint64(static_cast<uint64_t>(17));
  EXPECT_EQ(static_cast<uint64_t>(17), pod->get_f_uint64());
  EXPECT_TRUE(pod->is_f_uint64());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_UINT64);

  pod->set_f_float(1.5);
  EXPECT_EQ(1.5, pod->get_f_float());
  EXPECT_TRUE(pod->is_f_float());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_FLOAT);

  pod->set_f_double(1.9);
  EXPECT_EQ(1.9, pod->get_f_double());
  EXPECT_TRUE(pod->is_f_double());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_DOUBLE);

  pod->set_f_bool(true);
  EXPECT_TRUE(pod->get_f_bool());
  pod->set_f_bool(false);
  EXPECT_FALSE(pod->get_f_bool());
  EXPECT_TRUE(pod->is_f_bool());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_BOOL);
}

TEST(UnionTest, PodEquals) {
  PodUnionPtr pod1(PodUnion::New());
  PodUnionPtr pod2(PodUnion::New());

  pod1->set_f_int8(10);
  pod2->set_f_int8(10);
  EXPECT_TRUE(pod1.Equals(pod2));

  pod2->set_f_int8(11);
  EXPECT_FALSE(pod1.Equals(pod2));

  pod2->set_f_int8_other(10);
  EXPECT_FALSE(pod1.Equals(pod2));
}

TEST(UnionTest, PodClone) {
  PodUnionPtr pod(PodUnion::New());
  pod->set_f_int8(10);

  PodUnionPtr pod_clone = pod.Clone();
  EXPECT_EQ(10, pod_clone->get_f_int8());
  EXPECT_TRUE(pod_clone->is_f_int8());
  EXPECT_EQ(pod_clone->which(), PodUnion::Tag::F_INT8);
}

TEST(UnionTest, SerializationPod) {
  PodUnionPtr pod1(PodUnion::New());
  pod1->set_f_int8(10);

  size_t size = GetSerializedSize_(pod1);
  EXPECT_EQ(16U, size);

  mojo::internal::FixedBuffer buf(size);
  internal::PodUnion_Data* data;
  Serialize_(pod1.Pass(), &buf, &data);

  PodUnionPtr pod2;
  Deserialize_(data, &pod2);

  EXPECT_EQ(10, pod2->get_f_int8());
  EXPECT_TRUE(pod2->is_f_int8());
  EXPECT_EQ(pod2->which(), PodUnion::Tag::F_INT8);
}

TEST(UnionTest, StringGetterSetter) {
  PodUnionPtr pod(PodUnion::New());

  String hello("hello world");
  pod->set_f_string(hello);
  EXPECT_EQ(hello, pod->get_f_string());
  EXPECT_TRUE(pod->is_f_string());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_STRING);
}

TEST(UnionTest, StringEquals) {
  PodUnionPtr pod1(PodUnion::New());
  PodUnionPtr pod2(PodUnion::New());

  pod1->set_f_string("hello world");
  pod2->set_f_string("hello world");
  EXPECT_TRUE(pod1.Equals(pod2));

  pod2->set_f_string("hello universe");
  EXPECT_FALSE(pod1.Equals(pod2));
}

TEST(UnionTest, StringClone) {
  PodUnionPtr pod(PodUnion::New());

  String hello("hello world");
  pod->set_f_string(hello);
  PodUnionPtr pod_clone = pod.Clone();
  EXPECT_EQ(hello, pod_clone->get_f_string());
  EXPECT_TRUE(pod_clone->is_f_string());
  EXPECT_EQ(pod_clone->which(), PodUnion::Tag::F_STRING);
}

TEST(UnionTest, StringSerialization) {
  PodUnionPtr pod1(PodUnion::New());

  String hello("hello world");
  pod1->set_f_string(hello);

  size_t size = GetSerializedSize_(pod1);
  mojo::internal::FixedBuffer buf(size);
  internal::PodUnion_Data* data;
  Serialize_(pod1.Pass(), &buf, &data);

  PodUnionPtr pod2;
  Deserialize_(data, &pod2);
  EXPECT_EQ(hello, pod2->get_f_string());
  EXPECT_TRUE(pod2->is_f_string());
  EXPECT_EQ(pod2->which(), PodUnion::Tag::F_STRING);
}
}  // namespace test
}  // namespace mojo
