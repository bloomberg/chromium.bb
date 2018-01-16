// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "mojo/common/string16.mojom-blink.h"
#include "mojo/common/test_common_custom_types.mojom-blink.h"
#include "mojo/public/cpp/base/big_buffer_struct_traits.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "platform/wtf/text/WTFString.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/platform/mojo/CommonCustomTypesStructTraits.h"
#include "third_party/WebKit/Source/platform/wtf/Vector.h"
#include "third_party/WebKit/Source/platform/wtf/text/WTFString.h"

namespace blink {

TEST(CommonCustomTypesStructTraitsTest, String16) {
  // |str| is 8-bit.
  String str = String::FromUTF8("hello world");
  String output;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojo::common::mojom::blink::String16>(
          &str, &output));
  ASSERT_EQ(str, output);

  // Replace the "o"s in "hello world" with "o"s with acute, so that |str| is
  // 16-bit.
  str = String::FromUTF8("hell\xC3\xB3 w\xC3\xB3rld");

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojo::common::mojom::blink::String16>(
          &str, &output));
  ASSERT_EQ(str, output);
}

TEST(CommonCustomTypesStructTraitsTest, EmptyString16) {
  String str = String::FromUTF8("");
  String output;

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojo::common::mojom::blink::String16>(
          &str, &output));
  ASSERT_EQ(str, output);
}

TEST(CommonCustomTypesStructTraitsTest, BigString16_Empty) {
  String str = String::FromUTF8("");
  String output;

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
              mojo::common::mojom::blink::BigString16>(&str, &output));
  ASSERT_EQ(str, output);
}

TEST(CommonCustomTypesStructTraitsTest, BigString16_Short) {
  String str = String::FromUTF8("hello world");
  ASSERT_TRUE(str.Is8Bit());
  String output;

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
              mojo::common::mojom::blink::BigString16>(&str, &output));
  ASSERT_EQ(str, output);

  // Replace the "o"s in "hello world" with "o"s with acute, so that |str| is
  // 16-bit.
  str = String::FromUTF8("hell\xC3\xB3 w\xC3\xB3rld");

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
              mojo::common::mojom::blink::BigString16>(&str, &output));
  ASSERT_EQ(str, output);
}

TEST(CommonCustomTypesStructTraitsTest, BigString16_Long) {
  WTF::Vector<char> random_latin1_string(1024 * 1024);
  base::RandBytes(random_latin1_string.data(), random_latin1_string.size());

  String str(random_latin1_string.data(), random_latin1_string.size());
  String output;

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<
              mojo::common::mojom::blink::BigString16>(&str, &output));
  ASSERT_EQ(str, output);
}

}  // namespace blink
