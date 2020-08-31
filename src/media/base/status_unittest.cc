// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <string>

#include "base/json/json_writer.h"
#include "base/macros.h"
#include "media/base/media_serializers.h"
#include "media/base/status.h"
#include "media/base/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::HasSubstr;

namespace media {

class UselessThingToBeSerialized {
 public:
  explicit UselessThingToBeSerialized(const char* name) : name_(name) {}
  const char* name_;
};

namespace internal {

template <>
struct MediaSerializer<UselessThingToBeSerialized> {
  static base::Value Serialize(const UselessThingToBeSerialized& t) {
    return base::Value(t.name_);
  }
};

}  // namespace internal

// Friend class of MediaLog for access to internal constants.
class StatusTest : public testing::Test {
 public:
  Status DontFail() { return OkStatus(); }

  Status FailEasily() {
    return Status(StatusCode::kCodeOnlyForTesting, "Message");
  }

  Status FailRecursively(unsigned int count) {
    if (!count) {
      return FailEasily();
    }
    return FailRecursively(count - 1).AddHere();
  }

  template <typename T>
  Status FailWithData(const char* key, const T& t) {
    return Status(StatusCode::kCodeOnlyForTesting, "Message", FROM_HERE)
        .WithData(key, t);
  }

  Status FailWithCause() {
    Status err = FailEasily();
    return FailEasily().AddCause(std::move(err));
  }

  Status DoSomethingGiveItBack(Status me) {
    me.WithData("data", "Hey you! psst! Help me outta here! I'm trapped!");
    return me;
  }

  // Make sure that the typical usage of ErrorOr actually compiles.
  ErrorOr<std::unique_ptr<int>> TypicalErrorOrUsage(bool succeed) {
    if (succeed)
      return std::make_unique<int>(123);
    return Status(StatusCode::kCodeOnlyForTesting);
  }
};

TEST_F(StatusTest, StaticOKMethodGivesCorrectSerialization) {
  Status ok = DontFail();
  base::Value actual = MediaSerialize(ok);
  ASSERT_EQ(actual.GetString(), "Ok");
}

TEST_F(StatusTest, SingleLayerError) {
  Status failed = FailEasily();
  base::Value actual = MediaSerialize(failed);
  ASSERT_EQ(actual.DictSize(), 5ul);
  ASSERT_EQ(actual.FindIntPath("status_code"),
            static_cast<int32_t>(StatusCode::kCodeOnlyForTesting));
  ASSERT_EQ(*actual.FindStringPath("status_message"), "Message");
  ASSERT_EQ(actual.FindListPath("stack")->GetList().size(), 1ul);
  ASSERT_EQ(actual.FindListPath("causes")->GetList().size(), 0ul);
  ASSERT_EQ(actual.FindDictPath("data")->DictSize(), 0ul);

  const auto& stack = actual.FindListPath("stack")->GetList();
  ASSERT_EQ(stack[0].DictSize(), 2ul);  // line and file

  // This is a bit fragile, since it's dependent on the file layout.
  ASSERT_EQ(stack[0].FindIntPath("line").value_or(-1), 42);
  ASSERT_THAT(*stack[0].FindStringPath("file"),
              HasSubstr("status_unittest.cc"));
}

TEST_F(StatusTest, MultipleErrorLayer) {
  Status failed = FailRecursively(3);
  base::Value actual = MediaSerialize(failed);
  ASSERT_EQ(actual.DictSize(), 5ul);
  ASSERT_EQ(actual.FindIntPath("status_code").value_or(-1),
            static_cast<int32_t>(StatusCode::kCodeOnlyForTesting));
  ASSERT_EQ(*actual.FindStringPath("status_message"), "Message");
  ASSERT_EQ(actual.FindListPath("stack")->GetList().size(), 4ul);
  ASSERT_EQ(actual.FindListPath("causes")->GetList().size(), 0ul);
  ASSERT_EQ(actual.FindDictPath("data")->DictSize(), 0ul);

  const auto& stack = actual.FindListPath("stack")->GetList();
  ASSERT_EQ(stack[0].DictSize(), 2ul);  // line and file
}

TEST_F(StatusTest, CanHaveData) {
  Status failed = FailWithData("example", "data");
  base::Value actual = MediaSerialize(failed);
  ASSERT_EQ(actual.DictSize(), 5ul);
  ASSERT_EQ(actual.FindIntPath("status_code").value_or(-1),
            static_cast<int32_t>(StatusCode::kCodeOnlyForTesting));
  ASSERT_EQ(*actual.FindStringPath("status_message"), "Message");
  ASSERT_EQ(actual.FindListPath("stack")->GetList().size(), 1ul);
  ASSERT_EQ(actual.FindListPath("causes")->GetList().size(), 0ul);
  ASSERT_EQ(actual.FindDictPath("data")->DictSize(), 1ul);

  const auto& stack = actual.FindListPath("stack")->GetList();
  ASSERT_EQ(stack[0].DictSize(), 2ul);  // line and file

  ASSERT_EQ(*actual.FindDictPath("data")->FindStringPath("example"), "data");
}

TEST_F(StatusTest, CanUseCustomSerializer) {
  Status failed = FailWithData("example", UselessThingToBeSerialized("F"));
  base::Value actual = MediaSerialize(failed);
  ASSERT_EQ(actual.DictSize(), 5ul);
  ASSERT_EQ(actual.FindIntPath("status_code"),
            static_cast<int32_t>(StatusCode::kCodeOnlyForTesting));
  ASSERT_EQ(*actual.FindStringPath("status_message"), "Message");
  ASSERT_EQ(actual.FindListPath("stack")->GetList().size(), 1ul);
  ASSERT_EQ(actual.FindListPath("causes")->GetList().size(), 0ul);
  ASSERT_EQ(actual.FindDictPath("data")->DictSize(), 1ul);

  const auto& stack = actual.FindListPath("stack")->GetList();
  ASSERT_EQ(stack[0].DictSize(), 2ul);  // line and file

  ASSERT_EQ(*actual.FindDictPath("data")->FindStringPath("example"), "F");
}

TEST_F(StatusTest, CausedByHasVector) {
  Status causal = FailWithCause();
  base::Value actual = MediaSerialize(causal);
  ASSERT_EQ(actual.DictSize(), 5ul);
  ASSERT_EQ(actual.FindIntPath("status_code").value_or(-1),
            static_cast<int32_t>(StatusCode::kCodeOnlyForTesting));
  ASSERT_EQ(*actual.FindStringPath("status_message"), "Message");
  ASSERT_EQ(actual.FindListPath("stack")->GetList().size(), 1ul);
  ASSERT_EQ(actual.FindListPath("causes")->GetList().size(), 1ul);
  ASSERT_EQ(actual.FindDictPath("data")->DictSize(), 0ul);

  base::Value& nested = actual.FindListPath("causes")->GetList()[0];
  ASSERT_EQ(nested.DictSize(), 5ul);
  ASSERT_EQ(nested.FindIntPath("status_code").value_or(-1),
            static_cast<int32_t>(StatusCode::kCodeOnlyForTesting));
  ASSERT_EQ(*nested.FindStringPath("status_message"), "Message");
  ASSERT_EQ(nested.FindListPath("stack")->GetList().size(), 1ul);
  ASSERT_EQ(nested.FindListPath("causes")->GetList().size(), 0ul);
  ASSERT_EQ(nested.FindDictPath("data")->DictSize(), 0ul);
}

TEST_F(StatusTest, CanCopyEasily) {
  Status failed = FailEasily();
  Status withData = DoSomethingGiveItBack(failed);

  base::Value actual = MediaSerialize(failed);
  ASSERT_EQ(actual.DictSize(), 5ul);
  ASSERT_EQ(actual.FindIntPath("status_code"),
            static_cast<int32_t>(StatusCode::kCodeOnlyForTesting));
  ASSERT_EQ(*actual.FindStringPath("status_message"), "Message");
  ASSERT_EQ(actual.FindListPath("stack")->GetList().size(), 1ul);
  ASSERT_EQ(actual.FindListPath("causes")->GetList().size(), 0ul);
  ASSERT_EQ(actual.FindDictPath("data")->DictSize(), 0ul);

  actual = MediaSerialize(withData);
  ASSERT_EQ(actual.DictSize(), 5ul);
  ASSERT_EQ(actual.FindIntPath("status_code"),
            static_cast<int32_t>(StatusCode::kCodeOnlyForTesting));
  ASSERT_EQ(*actual.FindStringPath("status_message"), "Message");
  ASSERT_EQ(actual.FindListPath("stack")->GetList().size(), 1ul);
  ASSERT_EQ(actual.FindListPath("causes")->GetList().size(), 0ul);
  ASSERT_EQ(actual.FindDictPath("data")->DictSize(), 1ul);
}

TEST_F(StatusTest, ErrorOrTypicalUsage) {
  // Mostly so we have some code coverage on the default usage.
  EXPECT_TRUE(TypicalErrorOrUsage(true).has_value());
  EXPECT_FALSE(TypicalErrorOrUsage(true).has_error());
  EXPECT_FALSE(TypicalErrorOrUsage(false).has_value());
  EXPECT_TRUE(TypicalErrorOrUsage(false).has_error());
}

TEST_F(StatusTest, ErrorOrWithMoveOnlyType) {
  ErrorOr<std::unique_ptr<int>> error_or(std::make_unique<int>(123));
  EXPECT_TRUE(error_or.has_value());
  EXPECT_FALSE(error_or.has_error());
  std::unique_ptr<int> result = std::move(error_or.value());
  EXPECT_EQ(error_or.value(), nullptr);
  EXPECT_NE(result.get(), nullptr);
  EXPECT_EQ(*result, 123);
}

TEST_F(StatusTest, ErrorOrWithCopyableType) {
  ErrorOr<int> error_or(123);
  EXPECT_TRUE(error_or.has_value());
  EXPECT_FALSE(error_or.has_error());
  int result = std::move(error_or.value());
  EXPECT_EQ(result, 123);
  // Should be unaffected by the move.
  EXPECT_EQ(error_or.value(), 123);
}

TEST_F(StatusTest, ErrorOrMoveConstructionAndAssignment) {
  // Make sure that we can move-construct and move-assign a move-only value.
  ErrorOr<std::unique_ptr<int>> error_or_0(std::make_unique<int>(123));

  ErrorOr<std::unique_ptr<int>> error_or_1(std::move(error_or_0));
  EXPECT_EQ(error_or_0.value(), nullptr);

  ErrorOr<std::unique_ptr<int>> error_or_2 = std::move(error_or_1);
  EXPECT_EQ(error_or_1.value(), nullptr);

  // |error_or_2| should have gotten the original.
  std::unique_ptr<int> value = std::move(error_or_2.value());
  EXPECT_EQ(*value, 123);
}

TEST_F(StatusTest, ErrorOrCopyWorks) {
  // Make sure that we can move-construct and move-assign a move-only value.
  ErrorOr<int> error_or_0(123);
  ErrorOr<int> error_or_1(std::move(error_or_0));
  ErrorOr<int> error_or_2 = std::move(error_or_1);
  EXPECT_EQ(error_or_2.value(), 123);
}

}  // namespace media
