// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebVector.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Vector.h"

namespace blink {

TEST(WebVectorTest, Iterators) {
  Vector<int> input;
  for (int i = 0; i < 5; ++i)
    input.push_back(i);

  WebVector<int> web_vector(input);
  const WebVector<int>& const_web_vector = web_vector;
  Vector<int> output;

  ASSERT_EQ(input.size(), web_vector.size());

  // Use begin()/end() iterators directly.
  for (WebVector<int>::iterator it = web_vector.begin(); it != web_vector.end();
       ++it)
    output.push_back(*it);
  ASSERT_EQ(input.size(), output.size());
  for (size_t i = 0; i < input.size(); ++i)
    EXPECT_EQ(input[i], output[i]);

  // Use begin()/end() const_iterators directly.
  output.Clear();
  for (WebVector<int>::const_iterator it = const_web_vector.begin();
       it != const_web_vector.end(); ++it)
    output.push_back(*it);
  ASSERT_EQ(input.size(), output.size());
  for (size_t i = 0; i < input.size(); ++i)
    EXPECT_EQ(input[i], output[i]);

  // Use range-based for loop.
  output.Clear();
  for (int x : web_vector)
    output.push_back(x);
  ASSERT_EQ(input.size(), output.size());
  for (size_t i = 0; i < input.size(); ++i)
    EXPECT_EQ(input[i], output[i]);
}

TEST(WebVectorTest, IsEmpty) {
  WebVector<int> vector;
  ASSERT_TRUE(vector.IsEmpty());
  int value = 1;
  vector.Assign(&value, 1);
  ASSERT_EQ(1u, vector.size());
  ASSERT_FALSE(vector.IsEmpty());
}

TEST(WebVectorTest, Swap) {
  const int kFirstData[] = {1, 2, 3, 4, 5};
  const int kSecondData[] = {6, 5, 8};
  const size_t k_first_data_length = WTF_ARRAY_LENGTH(kFirstData);
  const size_t k_second_data_length = WTF_ARRAY_LENGTH(kSecondData);

  WebVector<int> first(kFirstData, k_first_data_length);
  WebVector<int> second(kSecondData, k_second_data_length);
  ASSERT_EQ(k_first_data_length, first.size());
  ASSERT_EQ(k_second_data_length, second.size());
  first.Swap(second);
  ASSERT_EQ(k_second_data_length, first.size());
  ASSERT_EQ(k_first_data_length, second.size());
  for (size_t i = 0; i < first.size(); ++i)
    EXPECT_EQ(kSecondData[i], first[i]);
  for (size_t i = 0; i < second.size(); ++i)
    EXPECT_EQ(kFirstData[i], second[i]);
}

TEST(WebVectorTest, CreateFromPointer) {
  const int kValues[] = {1, 2, 3, 4, 5};

  WebVector<int> vector(kValues, 3);
  ASSERT_EQ(3u, vector.size());
  ASSERT_EQ(1, vector[0]);
  ASSERT_EQ(2, vector[1]);
  ASSERT_EQ(3, vector[2]);
}

TEST(WebVectorTest, CreateFromWtfVector) {
  Vector<int> input;
  for (int i = 0; i < 5; ++i)
    input.push_back(i);

  WebVector<int> vector(input);
  ASSERT_EQ(input.size(), vector.size());
  for (size_t i = 0; i < vector.size(); ++i)
    EXPECT_EQ(input[i], vector[i]);

  WebVector<int> copy(input);
  ASSERT_EQ(input.size(), copy.size());
  for (size_t i = 0; i < copy.size(); ++i)
    EXPECT_EQ(input[i], copy[i]);

  WebVector<int> assigned;
  assigned = copy;
  ASSERT_EQ(input.size(), assigned.size());
  for (size_t i = 0; i < assigned.size(); ++i)
    EXPECT_EQ(input[i], assigned[i]);
}

TEST(WebVectorTest, CreateFromStdVector) {
  std::vector<int> input;
  for (int i = 0; i < 5; ++i)
    input.push_back(i);

  WebVector<int> vector(input);
  ASSERT_EQ(input.size(), vector.size());
  for (size_t i = 0; i < vector.size(); ++i)
    EXPECT_EQ(input[i], vector[i]);

  WebVector<int> assigned;
  assigned = input;
  ASSERT_EQ(input.size(), assigned.size());
  for (size_t i = 0; i < assigned.size(); ++i)
    EXPECT_EQ(input[i], assigned[i]);
}

}  // namespace blink
