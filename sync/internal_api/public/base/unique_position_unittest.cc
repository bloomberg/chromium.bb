// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/base/unique_position.h"

#include <algorithm>
#include <string>

#include "base/base64.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/sha1.h"
#include "base/string_number_conversions.h"
#include "sync/protocol/unique_position.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class UniquePositionTest : public ::testing::Test {
 protected:
  // Accessor to fetch the length of the position's internal representation
  // We try to avoid having any test expectations on it because this is an
  // implementation detail.
  //
  // If you run the tests with --v=1, we'll print out some of the lengths
  // so you can see how well the algorithm performs in various insertion
  // scenarios.
  size_t GetLength(const UniquePosition& pos) {
    sync_pb::UniquePosition proto;
    pos.ToProto(&proto);
    return proto.ByteSize();
  }
};

// This function exploits internal knowledge of how the protobufs are serialized
// to help us build UniquePositions from strings described in this file.
static UniquePosition FromBytes(const std::string& bytes) {
  sync_pb::UniquePosition proto;
  proto.set_value(bytes);
  return UniquePosition::FromProto(proto);
}

const size_t kMinLength = UniquePosition::kSuffixLength;
const size_t kGenericPredecessorLength = kMinLength + 2;
const size_t kGenericSuccessorLength = kMinLength + 1;
const size_t kBigPositionLength = kMinLength;
const size_t kSmallPositionLength = kMinLength;

// Be careful when adding more prefixes to this list.
// We have to manually ensure each has a unique suffix.
const UniquePosition kGenericPredecessor = FromBytes(
    (std::string(kGenericPredecessorLength, '\x23') + '\xFF'));
const UniquePosition kGenericSuccessor = FromBytes(
    std::string(kGenericSuccessorLength, '\xAB') + '\xFF');
const UniquePosition kBigPosition = FromBytes(
    std::string(kBigPositionLength - 1, '\xFF') + '\xFE' + '\xFF');
const UniquePosition kBigPositionLessTwo = FromBytes(
    std::string(kBigPositionLength - 1, '\xFF') + '\xFC' + '\xFF');
const UniquePosition kBiggerPosition = FromBytes(
    std::string(kBigPositionLength, '\xFF') + '\xFF');
const UniquePosition kSmallPosition = FromBytes(
    std::string(kSmallPositionLength - 1, '\x00') + '\x01' + '\xFF');
const UniquePosition kSmallPositionPlusOne = FromBytes(
    std::string(kSmallPositionLength - 1, '\x00') + '\x02' + '\xFF');

const std::string kMinSuffix =
    std::string(UniquePosition::kSuffixLength - 1, '\x00') + '\x01';
const std::string kMaxSuffix(UniquePosition::kSuffixLength, '\xFF');
const std::string kNormalSuffix(UniquePosition::kSuffixLength, '\xAB');

::testing::AssertionResult LessThan(const char* m_expr,
                                    const char* n_expr,
                                    const UniquePosition &m,
                                    const UniquePosition &n) {
  if (m.LessThan(n))
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure()
      << m_expr << " is not less than " << n_expr
      << " (" << m.ToDebugString() << " and " << n.ToDebugString() << ")";
}

TEST_F(UniquePositionTest, SerializeAndDeserialize) {
  UniquePosition pos = kGenericPredecessor;
  sync_pb::UniquePosition proto;

  pos.ToProto(&proto);
  UniquePosition deserialized = UniquePosition::FromProto(proto);

  EXPECT_TRUE(pos.Equals(deserialized));
}

class RelativePositioningTest : public UniquePositionTest { };

const UniquePosition kPositionArray[] = {
  kGenericPredecessor,
  kGenericSuccessor,
  kBigPosition,
  kBigPositionLessTwo,
  kBiggerPosition,
  kSmallPosition,
  kSmallPositionPlusOne,
};

const UniquePosition kSortedPositionArray[] = {
  kSmallPosition,
  kSmallPositionPlusOne,
  kGenericPredecessor,
  kGenericSuccessor,
  kBigPositionLessTwo,
  kBigPosition,
  kBiggerPosition,
};

static const size_t kNumPositions = arraysize(kPositionArray);
static const size_t kNumSortedPositions = arraysize(kSortedPositionArray);

struct PositionLessThan {
  bool operator()(const UniquePosition& a, const UniquePosition& b) {
    return a.LessThan(b);
  }
};

// Returns true iff the given position's suffix matches the input parameter.
static bool IsSuffixInUse(
    const UniquePosition& pos, const std::string& suffix) {
  return pos.GetSuffixForTest() == suffix;
}

// Test some basic properties of comparison and equality.
TEST_F(RelativePositioningTest, ComparisonSanityTest1) {
  const UniquePosition& a = kPositionArray[0];
  ASSERT_TRUE(a.IsValid());

  // Necessarily true for any non-invalid positions.
  EXPECT_TRUE(a.Equals(a));
  EXPECT_FALSE(a.LessThan(a));
}

// Test some more properties of comparison and equality.
TEST_F(RelativePositioningTest, ComparisonSanityTest2) {
  const UniquePosition& a = kPositionArray[0];
  const UniquePosition& b = kPositionArray[1];

  // These should pass for the specific a and b we have chosen (a < b).
  EXPECT_FALSE(a.Equals(b));
  EXPECT_TRUE(a.LessThan(b));
  EXPECT_FALSE(b.LessThan(a));
}

// Exercise comparision functions by sorting and re-sorting the list.
TEST_F(RelativePositioningTest, SortPositions) {
  ASSERT_EQ(kNumPositions, kNumSortedPositions);
  UniquePosition positions[arraysize(kPositionArray)];
  for (size_t i = 0; i < kNumPositions; ++i) {
    positions[i] = kPositionArray[i];
  }

  std::sort(&positions[0], &positions[kNumPositions], PositionLessThan());
  for (size_t i = 0; i < kNumPositions; ++i) {
    EXPECT_TRUE(positions[i].Equals(kSortedPositionArray[i]))
        << "i: " << i << ", "
        << positions[i].ToDebugString() << " != "
        << kSortedPositionArray[i].ToDebugString();
  }

}

// Some more exercise for the comparison function.
TEST_F(RelativePositioningTest, ReverseSortPositions) {
  ASSERT_EQ(kNumPositions, kNumSortedPositions);
  UniquePosition positions[arraysize(kPositionArray)];
  for (size_t i = 0; i < kNumPositions; ++i) {
    positions[i] = kPositionArray[i];
  }

  std::reverse(&positions[0], &positions[kNumPositions]);
  std::sort(&positions[0], &positions[kNumPositions], PositionLessThan());
  for (size_t i = 0; i < kNumPositions; ++i) {
    EXPECT_TRUE(positions[i].Equals(kSortedPositionArray[i]))
        << "i: " << i << ", "
        << positions[i].ToDebugString() << " != "
        << kSortedPositionArray[i].ToDebugString();
  }
}

class PositionInsertTest :
    public RelativePositioningTest,
    public ::testing::WithParamInterface<std::string> { };

// Exercise InsertBetween with various insertion operations.
TEST_P(PositionInsertTest, InsertBetween) {
  const std::string suffix = GetParam();
  ASSERT_TRUE(UniquePosition::IsValidSuffix(suffix));

  for (size_t i = 0; i < kNumSortedPositions; ++i) {
    const UniquePosition& predecessor = kSortedPositionArray[i];
    // Verify our suffixes are unique before we continue.
    if (IsSuffixInUse(predecessor, suffix))
      continue;

    for (size_t j = i + 1; j < kNumSortedPositions; ++j) {
      const UniquePosition& successor = kSortedPositionArray[j];

      // Another guard against non-unique suffixes.
      if (IsSuffixInUse(successor, suffix))
        continue;

      UniquePosition midpoint =
          UniquePosition::Between(predecessor, successor, suffix);

      EXPECT_PRED_FORMAT2(LessThan, predecessor, midpoint);
      EXPECT_PRED_FORMAT2(LessThan, midpoint, successor);
    }
  }
}

TEST_P(PositionInsertTest, InsertBefore) {
  const std::string suffix = GetParam();
  for (size_t i = 0; i < kNumSortedPositions; ++i) {
    const UniquePosition& successor = kSortedPositionArray[i];
    // Verify our suffixes are unique before we continue.
    if (IsSuffixInUse(successor, suffix))
      continue;

    UniquePosition before = UniquePosition::Before(successor, suffix);

    EXPECT_PRED_FORMAT2(LessThan, before, successor);
  }
}

TEST_P(PositionInsertTest, InsertAfter) {
  const std::string suffix = GetParam();
  for (size_t i = 0; i < kNumSortedPositions; ++i) {
    const UniquePosition& predecessor = kSortedPositionArray[i];
    // Verify our suffixes are unique before we continue.
    if (IsSuffixInUse(predecessor, suffix))
      continue;

    UniquePosition after = UniquePosition::After(predecessor, suffix);

    EXPECT_PRED_FORMAT2(LessThan, predecessor, after);
  }
}

TEST_P(PositionInsertTest, StressInsertAfter) {
  // Use two different suffixes to not violate our suffix uniqueness guarantee.
  const std::string& suffix_a = GetParam();
  std::string suffix_b = suffix_a;
  suffix_b[10] = suffix_b[10] ^ 0xff;

  UniquePosition pos = UniquePosition::InitialPosition(suffix_a);
  for (int i = 0; i < 1024; i++) {
    const std::string& suffix = (i % 2 == 0) ? suffix_b : suffix_a;
    UniquePosition next_pos = UniquePosition::After(pos, suffix);
    ASSERT_PRED_FORMAT2(LessThan, pos, next_pos);
    pos = next_pos;
  }

  VLOG(1) << "Length: " << GetLength(pos);
}

TEST_P(PositionInsertTest, StressInsertBefore) {
  // Use two different suffixes to not violate our suffix uniqueness guarantee.
  const std::string& suffix_a = GetParam();
  std::string suffix_b = suffix_a;
  suffix_b[10] = suffix_b[10] ^ 0xff;

  UniquePosition pos = UniquePosition::InitialPosition(suffix_a);
  for (int i = 0; i < 1024; i++) {
    const std::string& suffix = (i % 2 == 0) ? suffix_b : suffix_a;
    UniquePosition prev_pos = UniquePosition::Before(pos, suffix);
    ASSERT_PRED_FORMAT2(LessThan, prev_pos, pos);
    pos = prev_pos;
  }

  VLOG(1) << "Length: " << GetLength(pos);
}

TEST_P(PositionInsertTest, StressLeftInsertBetween) {
  // Use different suffixes to not violate our suffix uniqueness guarantee.
  const std::string& suffix_a = GetParam();
  std::string suffix_b = suffix_a;
  suffix_b[10] = suffix_b[10] ^ 0xff;
  std::string suffix_c = suffix_a;
  suffix_c[10] = suffix_c[10] ^ 0xf0;

  UniquePosition right_pos = UniquePosition::InitialPosition(suffix_c);
  UniquePosition left_pos = UniquePosition::Before(right_pos, suffix_a);
  for (int i = 0; i < 1024; i++) {
    const std::string& suffix = (i % 2 == 0) ? suffix_b : suffix_a;
    UniquePosition new_pos =
        UniquePosition::Between(left_pos, right_pos, suffix);
    ASSERT_PRED_FORMAT2(LessThan, left_pos, new_pos);
    ASSERT_PRED_FORMAT2(LessThan, new_pos, right_pos);
    left_pos = new_pos;
  }

  VLOG(1) << "Lengths: " << GetLength(left_pos) << ", " << GetLength(right_pos);
}

TEST_P(PositionInsertTest, StressRightInsertBetween) {
  // Use different suffixes to not violate our suffix uniqueness guarantee.
  const std::string& suffix_a = GetParam();
  std::string suffix_b = suffix_a;
  suffix_b[10] = suffix_b[10] ^ 0xff;
  std::string suffix_c = suffix_a;
  suffix_c[10] = suffix_c[10] ^ 0xf0;

  UniquePosition right_pos = UniquePosition::InitialPosition(suffix_a);
  UniquePosition left_pos = UniquePosition::Before(right_pos, suffix_c);
  for (int i = 0; i < 1024; i++) {
    const std::string& suffix = (i % 2 == 0) ? suffix_b : suffix_a;
    UniquePosition new_pos =
        UniquePosition::Between(left_pos, right_pos, suffix);
    ASSERT_PRED_FORMAT2(LessThan, left_pos, new_pos);
    ASSERT_PRED_FORMAT2(LessThan, new_pos, right_pos);
    right_pos = new_pos;
  }

  VLOG(1) << "Lengths: " << GetLength(left_pos) << ", " << GetLength(right_pos);
}

// Generates suffixes similar to those generated by the directory.
// This may become obsolete if the suffix generation code is modified.
class SuffixGenerator {
 public:
  explicit SuffixGenerator(const std::string& cache_guid)
      : cache_guid_(cache_guid),
        next_id_(-65535) {
  }

  std::string NextSuffix() {
    // This is not entirely realistic, but that should be OK.  The current
    // suffix format is a base64'ed SHA1 hash, which should be fairly close to
    // random anyway.
    std::string input = cache_guid_ + base::Int64ToString(next_id_--);
    std::string output;
    CHECK(base::Base64Encode(base::SHA1HashString(input), &output));
    return output;
  }

 private:
  const std::string cache_guid_;
  int64 next_id_;
};

// Cache guids generated in the same style as real clients.
static const char kCacheGuidStr1[] = "tuiWdG8hV+8y4RT9N5Aikg==";
static const char kCacheGuidStr2[] = "yaKb7zHtY06aue9a0vlZgw==";

class PositionScenariosTest : public UniquePositionTest {
 public:
  PositionScenariosTest()
      : generator1_(std::string(kCacheGuidStr1, arraysize(kCacheGuidStr1)-1)),
        generator2_(std::string(kCacheGuidStr2, arraysize(kCacheGuidStr2)-1)) {
  }

  std::string NextClient1Suffix() {
    return generator1_.NextSuffix();
  }

  std::string NextClient2Suffix() {
    return generator2_.NextSuffix();
  }

 private:
  SuffixGenerator generator1_;
  SuffixGenerator generator2_;
};

// One client creating new bookmarks, always inserting at the end.
TEST_F(PositionScenariosTest, OneClientInsertAtEnd) {
  UniquePosition pos =
      UniquePosition::InitialPosition(NextClient1Suffix());
  for (int i = 0; i < 1024; i++) {
    const std::string suffix = NextClient1Suffix();
    UniquePosition new_pos = UniquePosition::After(pos, suffix);
    ASSERT_PRED_FORMAT2(LessThan, pos, new_pos);
    pos = new_pos;
  }

  VLOG(1) << "Length: " << GetLength(pos);

  // Normally we wouldn't want to make an assertion about the internal
  // representation of our data, but we make an exception for this case.
  // If this scenario causes lengths to explode, we have a big problem.
  EXPECT_LT(GetLength(pos), 500U);
}

// Two clients alternately inserting entries at the end, with a strong
// bias towards insertions by the first client.
TEST_F(PositionScenariosTest, TwoClientsInsertAtEnd_A) {
  UniquePosition pos =
      UniquePosition::InitialPosition(NextClient1Suffix());
  for (int i = 0; i < 1024; i++) {
    std::string suffix;
    if (i % 5 == 0) {
      suffix = NextClient2Suffix();
    } else {
      suffix = NextClient1Suffix();
    }

    UniquePosition new_pos = UniquePosition::After(pos, suffix);
    ASSERT_PRED_FORMAT2(LessThan, pos, new_pos);
    pos = new_pos;
  }

  VLOG(1) << "Length: " << GetLength(pos);
  EXPECT_LT(GetLength(pos), 500U);
}

// Two clients alternately inserting entries at the end.
TEST_F(PositionScenariosTest, TwoClientsInsertAtEnd_B) {
  UniquePosition pos =
      UniquePosition::InitialPosition(NextClient1Suffix());
  for (int i = 0; i < 1024; i++) {
    std::string suffix;
    if (i % 2 == 0) {
      suffix = NextClient1Suffix();
    } else {
      suffix = NextClient2Suffix();
    }

    UniquePosition new_pos = UniquePosition::After(pos, suffix);
    ASSERT_PRED_FORMAT2(LessThan, pos, new_pos);
    pos = new_pos;
  }

  VLOG(1) << "Length: " << GetLength(pos);
  EXPECT_LT(GetLength(pos), 500U);
}

INSTANTIATE_TEST_CASE_P(MinSuffix, PositionInsertTest,
                        ::testing::Values(kMinSuffix));
INSTANTIATE_TEST_CASE_P(MaxSuffix, PositionInsertTest,
                        ::testing::Values(kMaxSuffix));
INSTANTIATE_TEST_CASE_P(NormalSuffix, PositionInsertTest,
                        ::testing::Values(kNormalSuffix));

class PositionFromIntTest : public UniquePositionTest {
 public:
  PositionFromIntTest()
      : generator_(std::string(kCacheGuidStr1, arraysize(kCacheGuidStr1)-1)) {
  }

 protected:
  std::string NextSuffix() {
    return generator_.NextSuffix();
  }

 private:
  SuffixGenerator generator_;
};

const int64 kTestValues[] = {
  0LL,
  1LL, -1LL,
  2LL, -2LL,
  3LL, -3LL,
  0x79LL, -0x79LL,
  0x80LL, -0x80LL,
  0x81LL, -0x81LL,
  0xFELL, -0xFELL,
  0xFFLL, -0xFFLL,
  0x100LL, -0x100LL,
  0x101LL, -0x101LL,
  0xFA1AFELL, -0xFA1AFELL,
  0xFFFFFFFELL, -0xFFFFFFFELL,
  0xFFFFFFFFLL, -0xFFFFFFFFLL,
  0x100000000LL, -0x100000000LL,
  0x100000001LL, -0x100000001LL,
  0xFFFFFFFFFFLL, -0xFFFFFFFFFFLL,
  0x112358132134LL, -0x112358132134LL,
  0xFEFFBEEFABC1234LL, -0xFEFFBEEFABC1234LL,
  kint64max,
  kint64min,
  kint64min + 1,
  kint64max - 1
};

const size_t kNumTestValues = arraysize(kTestValues);

TEST_F(PositionFromIntTest, IsValid) {
  for (size_t i = 0; i < kNumTestValues; ++i) {
    const UniquePosition pos =
        UniquePosition::FromInt64(kTestValues[i], NextSuffix());
    EXPECT_TRUE(pos.IsValid()) << "i = " << i << "; " << pos.ToDebugString();
  }
}

TEST_F(PositionFromIntTest, RoundTripConversion) {
  for (size_t i = 0; i < kNumTestValues; ++i) {
    const int64 expected_value = kTestValues[i];
    const UniquePosition pos =
        UniquePosition::FromInt64(kTestValues[i], NextSuffix());
    const int64 value = pos.ToInt64();
    EXPECT_EQ(expected_value, value) << "i = " << i;
  }
}

template <typename T, typename LessThan = std::less<T> >
class IndexedLessThan {
 public:
  IndexedLessThan(const T* values) : values_(values) {}

  bool operator()(int i1, int i2) {
    return less_than_(values_[i1], values_[i2]);
  }

 private:
  const T* values_;
  LessThan less_than_;
};

TEST_F(PositionFromIntTest, ConsistentOrdering) {
  UniquePosition positions[kNumTestValues];
  std::vector<int> original_ordering(kNumTestValues);
  std::vector<int> int64_ordering(kNumTestValues);
  std::vector<int> position_ordering(kNumTestValues);
  for (size_t i = 0; i < kNumTestValues; ++i) {
    positions[i] = UniquePosition::FromInt64(
        kTestValues[i], NextSuffix());
    original_ordering[i] = int64_ordering[i] = position_ordering[i] = i;
  }

  std::sort(int64_ordering.begin(), int64_ordering.end(),
            IndexedLessThan<int64>(kTestValues));
  std::sort(position_ordering.begin(), position_ordering.end(),
            IndexedLessThan<UniquePosition, PositionLessThan>(positions));
  EXPECT_NE(original_ordering, int64_ordering);
  EXPECT_EQ(int64_ordering, position_ordering);
}

}  // namespace

}  // namespace syncer
