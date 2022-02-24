// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <string>

#include "base/json/json_writer.h"
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

struct NoOkStatusTypeTraits {
  enum class Codes : StatusCodeType { kFoo = 0, kBar = 1, kBaz = 2 };
  static constexpr StatusGroupType Group() { return "NoDefaultNoOkType"; }
};

struct ZeroValueOkTypeTraits {
  enum class Codes : StatusCodeType { kOk = 0, kFoo = 1, kBar = 2, kBaz = 3 };
  static constexpr StatusGroupType Group() { return "ZeroValueOkTypeTraits"; }
};

struct NonZeroOkTypeTraits {
  enum class Codes : StatusCodeType {
    kOk = 100,
    kFoo = 0,
  };
  static constexpr StatusGroupType Group() { return "GroupWithNonZeroOkType"; }
};

struct MapValueCodeTraits {
  enum class Codes { kBadStartCode, kBadPtr, kLTZ, kNotSquare };
  static constexpr StatusGroupType Group() {
    return "MapValueTestingCodesGroup";
  }
};

struct TraitsWithCustomUKMSerializer {
  enum class Codes { kFoo, kBar };
  static constexpr StatusGroupType Group() { return "UKMSerializerCode"; }
  static uint32_t PackExtraData(const internal::StatusData& info) {
    auto maybe_key = info.data.FindIntKey("might_exist_key");
    if (maybe_key.has_value())
      return *maybe_key * 77;
    return 0;
  }
};

class StatusTest : public testing::Test {
 public:
  using NormalStatus = TypedStatus<ZeroValueOkTypeTraits>;

  NormalStatus DontFail() { return OkStatus(); }

  // Return a failure, with a line number.  Record the lower and upper line
  // number limits so that we can make sure that the error's line is bounded.
  constexpr static int lower_line_limit_ = __LINE__;
  NormalStatus FailEasily() {
    return NormalStatus(NormalStatus::Codes::kFoo, "Message");
  }
  constexpr static int upper_line_limit_ = __LINE__;

  NormalStatus FailRecursively(unsigned int count) {
    if (!count) {
      return FailEasily();
    }
    return FailRecursively(count - 1).AddHere();
  }

  template <typename T>
  NormalStatus FailWithData(const char* key, const T& t) {
    return NormalStatus(NormalStatus::Codes::kFoo, "Message", FROM_HERE)
        .WithData(key, t);
  }

  NormalStatus FailWithCause() {
    NormalStatus err = FailEasily();
    return FailEasily().AddCause(std::move(err));
  }

  NormalStatus DoSomethingGiveItBack(NormalStatus me) {
    me.WithData("data", "Hey you! psst! Help me outta here! I'm trapped!");
    return me;
  }

  NormalStatus::Or<std::unique_ptr<int>> TypicalStatusOrUsage(bool succeed) {
    if (succeed)
      return std::make_unique<int>(123);
    return NormalStatus::Codes::kFoo;
  }

  // Helpers for the Map test case.
  static TypedStatus<MapValueCodeTraits>::Or<std::unique_ptr<int>>
  GetStartingValue(int key) {
    switch (key) {
      case 0:
        return std::make_unique<int>(36);
      case 1:
        return std::make_unique<int>(40);
      case 2:
        return std::make_unique<int>(-10);
      case 3: {
        std::unique_ptr<int> ret = nullptr;
        return ret;
      }
      case 4:
        return std::make_unique<int>(81);
      default:
        return MapValueCodeTraits::Codes::kBadStartCode;
    }
  }

  static TypedStatus<MapValueCodeTraits>::Or<int> UnwrapPtr(
      std::unique_ptr<int> v) {
    if (!v)
      return MapValueCodeTraits::Codes::kBadPtr;
    return *v;
  }

  static TypedStatus<MapValueCodeTraits>::Or<int> FindIntSqrt(int v) {
    if (v < 0)
      return MapValueCodeTraits::Codes::kLTZ;
    int floor = sqrt(v);
    if (floor * floor != v)
      return MapValueCodeTraits::Codes::kNotSquare;
    return floor;
  }
};

TEST_F(StatusTest, StaticOKMethodGivesCorrectSerialization) {
  NormalStatus ok = DontFail();
  base::Value actual = MediaSerialize(ok);
  ASSERT_EQ(actual.GetString(), "Ok");
}

TEST_F(StatusTest, SingleLayerError) {
  NormalStatus failed = FailEasily();
  base::Value actual = MediaSerialize(failed);

  ASSERT_EQ(actual.DictSize(), 5ul);
  ASSERT_EQ(*actual.FindStringPath("message"), "Message");
  ASSERT_EQ(actual.FindListPath("stack")->GetListDeprecated().size(), 1ul);
  ASSERT_EQ(actual.FindKey("cause"), nullptr);
  ASSERT_EQ(actual.FindDictPath("data")->DictSize(), 0ul);

  const auto& stack = actual.FindListPath("stack")->GetListDeprecated();
  ASSERT_EQ(stack[0].DictSize(), 2ul);  // line and file

  // This is a bit fragile, since it's dependent on the file layout.  Just check
  // that it's somewhere in the `FailEasily`` function.
  int line = stack[0].FindIntPath("line").value_or(-1);
  ASSERT_GT(line, lower_line_limit_);
  ASSERT_LT(line, upper_line_limit_);
  ASSERT_THAT(*stack[0].FindStringPath("file"),
              HasSubstr("status_unittest.cc"));
}

TEST_F(StatusTest, MultipleErrorLayer) {
  NormalStatus failed = FailRecursively(3);
  base::Value actual = MediaSerialize(failed);
  ASSERT_EQ(actual.DictSize(), 5ul);
  ASSERT_EQ(*actual.FindStringPath("message"), "Message");
  ASSERT_EQ(actual.FindListPath("stack")->GetListDeprecated().size(), 4ul);
  ASSERT_EQ(actual.FindKey("cause"), nullptr);
  ASSERT_EQ(actual.FindDictPath("data")->DictSize(), 0ul);

  const auto& stack = actual.FindListPath("stack")->GetListDeprecated();
  ASSERT_EQ(stack[0].DictSize(), 2ul);  // line and file
}

TEST_F(StatusTest, CanHaveData) {
  NormalStatus failed = FailWithData("example", "data");
  base::Value actual = MediaSerialize(failed);
  ASSERT_EQ(actual.DictSize(), 5ul);
  ASSERT_EQ(*actual.FindStringPath("message"), "Message");
  ASSERT_EQ(actual.FindListPath("stack")->GetListDeprecated().size(), 1ul);
  ASSERT_EQ(actual.FindKey("cause"), nullptr);
  ASSERT_EQ(actual.FindDictPath("data")->DictSize(), 1ul);

  const auto& stack = actual.FindListPath("stack")->GetListDeprecated();
  ASSERT_EQ(stack[0].DictSize(), 2ul);  // line and file

  ASSERT_EQ(*actual.FindDictPath("data")->FindStringPath("example"), "data");
}

TEST_F(StatusTest, CanUseCustomSerializer) {
  NormalStatus failed =
      FailWithData("example", UselessThingToBeSerialized("F"));
  base::Value actual = MediaSerialize(failed);
  ASSERT_EQ(actual.DictSize(), 5ul);
  ASSERT_EQ(*actual.FindStringPath("message"), "Message");
  ASSERT_EQ(actual.FindListPath("stack")->GetListDeprecated().size(), 1ul);
  ASSERT_EQ(actual.FindKey("cause"), nullptr);
  ASSERT_EQ(actual.FindDictPath("data")->DictSize(), 1ul);

  const auto& stack = actual.FindListPath("stack")->GetListDeprecated();
  ASSERT_EQ(stack[0].DictSize(), 2ul);  // line and file

  ASSERT_EQ(*actual.FindDictPath("data")->FindStringPath("example"), "F");
}

TEST_F(StatusTest, CausedByHasVector) {
  NormalStatus causal = FailWithCause();
  base::Value actual = MediaSerialize(causal);
  ASSERT_EQ(actual.DictSize(), 6ul);
  ASSERT_EQ(*actual.FindStringPath("message"), "Message");
  ASSERT_EQ(actual.FindListPath("stack")->GetListDeprecated().size(), 1ul);
  ASSERT_EQ(actual.FindDictPath("data")->DictSize(), 0ul);
  ASSERT_NE(actual.FindKey("cause"), nullptr);

  base::Value* nested = actual.FindDictPath("cause");
  ASSERT_NE(nested, nullptr);
  ASSERT_EQ(nested->DictSize(), 5ul);
  ASSERT_EQ(*nested->FindStringPath("message"), "Message");
  ASSERT_EQ(nested->FindListPath("stack")->GetListDeprecated().size(), 1ul);
  ASSERT_EQ(nested->FindKey("cause"), nullptr);
  ASSERT_EQ(nested->FindDictPath("data")->DictSize(), 0ul);
}

TEST_F(StatusTest, CausedByCanAssignCopy) {
  NormalStatus causal = FailWithCause();
  NormalStatus copy_causal = causal;
  base::Value causal_serialized = MediaSerialize(causal);
  base::Value copy_causal_serialized = MediaSerialize(copy_causal);

  base::Value* original = causal_serialized.FindDictPath("cause");
  ASSERT_EQ(original->DictSize(), 5ul);
  ASSERT_EQ(*original->FindStringPath("message"), "Message");
  ASSERT_EQ(original->FindListPath("stack")->GetListDeprecated().size(), 1ul);
  ASSERT_EQ(original->FindKey("cause"), nullptr);
  ASSERT_EQ(original->FindDictPath("data")->DictSize(), 0ul);

  base::Value* copied = copy_causal_serialized.FindDictPath("cause");
  ASSERT_EQ(copied->DictSize(), 5ul);
  ASSERT_EQ(*copied->FindStringPath("message"), "Message");
  ASSERT_EQ(copied->FindListPath("stack")->GetListDeprecated().size(), 1ul);
  ASSERT_EQ(copied->FindKey("cause"), nullptr);
  ASSERT_EQ(copied->FindDictPath("data")->DictSize(), 0ul);
}

TEST_F(StatusTest, CanCopyEasily) {
  NormalStatus failed = FailEasily();
  NormalStatus withData = DoSomethingGiveItBack(failed);

  base::Value actual = MediaSerialize(failed);
  ASSERT_EQ(actual.DictSize(), 5ul);
  ASSERT_EQ(*actual.FindStringPath("message"), "Message");
  ASSERT_EQ(actual.FindListPath("stack")->GetListDeprecated().size(), 1ul);
  ASSERT_EQ(actual.FindKey("cause"), nullptr);
  ASSERT_EQ(actual.FindDictPath("data")->DictSize(), 0ul);

  actual = MediaSerialize(withData);
  ASSERT_EQ(actual.DictSize(), 5ul);
  ASSERT_EQ(*actual.FindStringPath("message"), "Message");
  ASSERT_EQ(actual.FindListPath("stack")->GetListDeprecated().size(), 1ul);
  ASSERT_EQ(actual.FindKey("cause"), nullptr);
  ASSERT_EQ(actual.FindDictPath("data")->DictSize(), 1ul);
}

TEST_F(StatusTest, StatusOrTypicalUsage) {
  // Mostly so we have some code coverage on the default usage.
  EXPECT_TRUE(TypicalStatusOrUsage(true).has_value());
  EXPECT_FALSE(TypicalStatusOrUsage(true).has_error());
  EXPECT_FALSE(TypicalStatusOrUsage(false).has_value());
  EXPECT_TRUE(TypicalStatusOrUsage(false).has_error());
}

TEST_F(StatusTest, StatusOrWithMoveOnlyType) {
  NormalStatus::Or<std::unique_ptr<int>> status_or(std::make_unique<int>(123));
  EXPECT_TRUE(status_or.has_value());
  EXPECT_FALSE(status_or.has_error());
  std::unique_ptr<int> result = std::move(status_or).value();
  EXPECT_NE(result.get(), nullptr);
  EXPECT_EQ(*result, 123);
}

TEST_F(StatusTest, StatusOrWithCopyableType) {
  NormalStatus::Or<int> status_or(123);
  EXPECT_TRUE(status_or.has_value());
  EXPECT_FALSE(status_or.has_error());
  int result = std::move(status_or).value();
  EXPECT_EQ(result, 123);
}

TEST_F(StatusTest, StatusOrMoveConstructionAndAssignment) {
  // Make sure that we can move-construct and move-assign a move-only value.
  NormalStatus::Or<std::unique_ptr<int>> status_or_0(
      std::make_unique<int>(123));

  NormalStatus::Or<std::unique_ptr<int>> status_or_1(std::move(status_or_0));

  NormalStatus::Or<std::unique_ptr<int>> status_or_2 = std::move(status_or_1);

  // |status_or_2| should have gotten the original.
  std::unique_ptr<int> value = std::move(status_or_2).value();
  EXPECT_EQ(*value, 123);
}

TEST_F(StatusTest, StatusOrCopyWorks) {
  // Make sure that we can move-construct and move-assign a move-only value.
  NormalStatus::Or<int> status_or_0(123);
  NormalStatus::Or<int> status_or_1(std::move(status_or_0));
  NormalStatus::Or<int> status_or_2 = std::move(status_or_1);
  EXPECT_EQ(std::move(status_or_2).value(), 123);
}

TEST_F(StatusTest, StatusOrCodeIsOkWithValue) {
  NormalStatus::Or<int> status_or(123);
  EXPECT_EQ(status_or.code(), NormalStatus::Codes::kOk);
}

TEST_F(StatusTest, TypedStatusWithNoDefaultAndNoOk) {
  using NDStatus = TypedStatus<NoOkStatusTypeTraits>;

  NDStatus foo = NDStatus::Codes::kFoo;
  EXPECT_EQ(foo.code(), NDStatus::Codes::kFoo);
  EXPECT_FALSE(foo.is_ok());

  NDStatus bar = NDStatus::Codes::kBar;
  EXPECT_EQ(bar.code(), NDStatus::Codes::kBar);
  EXPECT_FALSE(bar.is_ok());

  NDStatus::Or<std::string> err = NDStatus::Codes::kBaz;
  NDStatus::Or<std::string> ok = std::string("kBaz");

  EXPECT_TRUE(err.has_error());
  EXPECT_FALSE(err.has_value());
  // One cannot call err.code() without an okay type.
  EXPECT_EQ(std::move(err).error().code(), NDStatus::Codes::kBaz);

  EXPECT_FALSE(ok.has_error());
  EXPECT_TRUE(ok.has_value());
  // One cannot call ok.code() without an okay type.

  base::Value actual = MediaSerialize(bar);
  EXPECT_EQ(*actual.FindIntPath("code"), static_cast<int>(bar.code()));
}

TEST_F(StatusTest, TypedStatusWithNoDefaultHasOk) {
  using NDStatus = TypedStatus<ZeroValueOkTypeTraits>;

  NDStatus foo = NDStatus::Codes::kFoo;
  EXPECT_EQ(foo.code(), NDStatus::Codes::kFoo);
  EXPECT_FALSE(foo.is_ok());

  NDStatus bar = NDStatus::Codes::kBar;
  EXPECT_EQ(bar.code(), NDStatus::Codes::kBar);
  EXPECT_FALSE(bar.is_ok());

  NDStatus bat = NDStatus::Codes::kOk;
  EXPECT_EQ(bat.code(), NDStatus::Codes::kOk);
  EXPECT_TRUE(bat.is_ok());

  NDStatus::Or<std::string> err = NDStatus::Codes::kBaz;
  NDStatus::Or<std::string> ok = std::string("kBaz");

  EXPECT_TRUE(err.has_error());
  EXPECT_FALSE(err.has_value());
  EXPECT_EQ(err.code(), NDStatus::Codes::kBaz);

  EXPECT_FALSE(ok.has_error());
  EXPECT_TRUE(ok.has_value());
  EXPECT_EQ(ok.code(), NDStatus::Codes::kOk);

  base::Value actual = MediaSerialize(bar);
  EXPECT_EQ(*actual.FindIntPath("code"), static_cast<int>(bar.code()));
}

TEST_F(StatusTest, Okayness) {
  EXPECT_FALSE(
      TypedStatus<NoOkStatusTypeTraits>(NoOkStatusTypeTraits::Codes::kFoo)
          .is_ok());

  EXPECT_FALSE(
      TypedStatus<ZeroValueOkTypeTraits>(ZeroValueOkTypeTraits::Codes::kFoo)
          .is_ok());
  EXPECT_TRUE(
      TypedStatus<ZeroValueOkTypeTraits>(ZeroValueOkTypeTraits::Codes::kOk)
          .is_ok());

  EXPECT_FALSE(
      TypedStatus<NonZeroOkTypeTraits>(NonZeroOkTypeTraits::Codes::kFoo)
          .is_ok());
  EXPECT_TRUE(TypedStatus<NonZeroOkTypeTraits>(NonZeroOkTypeTraits::Codes::kOk)
                  .is_ok());
}

TEST_F(StatusTest, CanConvertOkToCode) {
  // OkStatus() should also be convertible to the enum directly.
  ZeroValueOkTypeTraits::Codes code = OkStatus();
  EXPECT_EQ(code, ZeroValueOkTypeTraits::Codes::kOk);
}

TEST_F(StatusTest, OkStatusInitializesToOk) {
  // Construction from the return value of OkStatus() should be `kOk`, for any
  // status traits that has `kOk`.  We only test explicit construction, though
  // this is probably used as an implicit construction in practice when it's
  // a return value.
  EXPECT_EQ(TypedStatus<ZeroValueOkTypeTraits>(OkStatus()).code(),
            ZeroValueOkTypeTraits::Codes::kOk);
  EXPECT_EQ(TypedStatus<NonZeroOkTypeTraits>(OkStatus()).code(),
            NonZeroOkTypeTraits::Codes::kOk);
}

TEST_F(StatusTest, StatusOrEqOp) {
  // Test the case of a non-default (non-ok) status
  NormalStatus::Or<std::string> failed = FailEasily();
  ASSERT_TRUE(failed == NormalStatus::Codes::kFoo);
  ASSERT_FALSE(failed == NormalStatus::Codes::kOk);
  ASSERT_TRUE(failed != NormalStatus::Codes::kOk);
  ASSERT_FALSE(failed != NormalStatus::Codes::kFoo);

  NormalStatus::Or<std::string> success = std::string("Kirkland > Seattle");
  ASSERT_TRUE(success != NormalStatus::Codes::kFoo);
  ASSERT_FALSE(success != NormalStatus::Codes::kOk);
  ASSERT_TRUE(success == NormalStatus::Codes::kOk);
  ASSERT_FALSE(success == NormalStatus::Codes::kFoo);
}

TEST_F(StatusTest, OrTypeMapping) {
  NormalStatus::Or<std::string> failed = FailEasily();
  NormalStatus::Or<int> failed_int = std::move(failed).MapValue(
      [](std::string value) { return atoi(value.c_str()); });
  ASSERT_TRUE(failed_int == NormalStatus::Codes::kFoo);

  // Try it with a c++ lambda
  NormalStatus::Or<std::string> success = std::string("12345");
  NormalStatus::Or<int> success_int = std::move(success).MapValue(
      [](std::string value) { return atoi(value.c_str()); });
  ASSERT_TRUE(success_int == NormalStatus::Codes::kOk);
  ASSERT_EQ(std::move(success_int).value(), 12345);

  // try it with a lambda returning-lambda
  auto finder = [](char search) {
    return [search](std::string seq) -> NormalStatus::Or<int> {
      auto count = std::count(seq.begin(), seq.end(), search);
      if (count == 0)
        return NormalStatus::Codes::kFoo;
      return count;
    };
  };
  NormalStatus::Or<std::string> hw = std::string("hello world");

  NormalStatus::Or<int> success_count = std::move(hw).MapValue(finder('l'));
  ASSERT_TRUE(success_count == NormalStatus::Codes::kOk);
  ASSERT_EQ(std::move(success_count).value(), 3);

  hw = std::string("hello world");
  NormalStatus::Or<int> fail_count = std::move(hw).MapValue(finder('x'));
  ASSERT_TRUE(fail_count == NormalStatus::Codes::kFoo);

  // Test it chained together! the return type should cascade through.
  auto case_0 = GetStartingValue(0).MapValue(UnwrapPtr).MapValue(FindIntSqrt);
  ASSERT_TRUE(case_0.has_value());
  ASSERT_EQ(std::move(case_0).value(), 6);

  auto case_1 = GetStartingValue(1).MapValue(UnwrapPtr).MapValue(FindIntSqrt);
  ASSERT_TRUE(case_1 == MapValueCodeTraits::Codes::kNotSquare);

  auto case_2 = GetStartingValue(2).MapValue(UnwrapPtr).MapValue(FindIntSqrt);
  ASSERT_TRUE(case_2 == MapValueCodeTraits::Codes::kLTZ);

  auto case_3 = GetStartingValue(3).MapValue(UnwrapPtr).MapValue(FindIntSqrt);
  ASSERT_TRUE(case_3 == MapValueCodeTraits::Codes::kBadPtr);

  auto case_4 = GetStartingValue(4)
                    .MapValue(UnwrapPtr)
                    .MapValue(FindIntSqrt)
                    .MapValue(FindIntSqrt);
  ASSERT_TRUE(case_4.has_value());
  ASSERT_EQ(std::move(case_4).value(), 3);

  auto case_5 = GetStartingValue(5).MapValue(UnwrapPtr).MapValue(FindIntSqrt);
  ASSERT_TRUE(case_5 == MapValueCodeTraits::Codes::kBadStartCode);
}

TEST_F(StatusTest, OrTypeMappingToOtherOrType) {
  using A = TypedStatus<NoOkStatusTypeTraits>;
  using B = TypedStatus<MapValueCodeTraits>;

  auto unwrap = [](std::unique_ptr<int> ptr) -> A::Or<int> {
    if (!ptr)
      return A::Codes::kFoo;
    return *ptr;
  };

  // Returns a valid unique ptr, maps unwraps, and is successful
  B::Or<std::unique_ptr<int>> b1 = GetStartingValue(0);
  A::Or<int> a1 = std::move(b1).MapValue(unwrap, A::Codes::kBar);
  ASSERT_TRUE(a1.has_value() && std::move(a1).value() == 36);

  // Returns a nullptr, not and error. so the unwrapper gives a kFoo.
  B::Or<std::unique_ptr<int>> b2 = GetStartingValue(3);
  A::Or<int> a2 = std::move(b2).MapValue(unwrap, A::Codes::kBar);
  ASSERT_TRUE(a2.has_error());
  ASSERT_TRUE(a2 == A::Codes::kFoo);

  // b3 is an error here, so Mapping it will wrap it in kBar.
  B::Or<std::unique_ptr<int>> b3 = GetStartingValue(5);
  A::Or<int> a3 = std::move(b3).MapValue(unwrap, A::Codes::kBar);
  ASSERT_TRUE(a3.has_error());
  ASSERT_TRUE(a3 == A::Codes::kBar);
}

TEST_F(StatusTest, UKMSerializerTest) {
  using SerializeStatus = TypedStatus<TraitsWithCustomUKMSerializer>;
  struct MockUkmBuilder {
    internal::UKMPackHelper status, root;
    void SetStatus(uint64_t data) { status.packed = data; }
    void SetRootCause(uint64_t data) { root.packed = data; }
  };

  MockUkmBuilder builder;

  // Normal status without PackExtraData won't have anything in |.extra_data|,
  // but if it has a cause, that will be serialized properly.
  NormalStatus causal = FailWithCause();
  causal.ToUKM(builder);

  ASSERT_NE(builder.root.packed, 0lu);
  ASSERT_EQ(builder.status.bits.code,
            static_cast<StatusCodeType>(NormalStatus::Codes::kFoo));
  ASSERT_EQ(builder.root.bits.code,
            static_cast<StatusCodeType>(NormalStatus::Codes::kFoo));
  ASSERT_EQ(builder.status.bits.extra_data, 0u);
  ASSERT_EQ(builder.root.bits.extra_data, 0u);

  // Make a status that supports PackExtraData, but doesn't have the key
  // it's lookinf for, and returns 0 instead.
  SerializeStatus result = SerializeStatus::Codes::kFoo;
  result.ToUKM(builder);
  ASSERT_EQ(builder.status.bits.code,
            static_cast<StatusCodeType>(SerializeStatus::Codes::kFoo));
  ASSERT_EQ(builder.status.bits.extra_data, 0u);

  // Add the special key, and demonstrate that |PackExtraData| is called.
  result.WithData("might_exist_key", 2);
  result.ToUKM(builder);
  ASSERT_EQ(builder.status.bits.code,
            static_cast<StatusCodeType>(SerializeStatus::Codes::kFoo));
  ASSERT_EQ(builder.status.bits.extra_data, 154u);

  // Wrap the code with extra data to ensure that |.root.extra_data| is
  // serialized.
  SerializeStatus wraps = SerializeStatus::Codes::kBar;
  wraps.AddCause(std::move(result));
  wraps.ToUKM(builder);
  ASSERT_NE(builder.root.packed, 0u);
  ASSERT_EQ(builder.root.bits.code,
            static_cast<StatusCodeType>(SerializeStatus::Codes::kFoo));
  ASSERT_EQ(builder.root.bits.extra_data, 154u);
  ASSERT_NE(builder.status.packed, 0u);
  ASSERT_EQ(builder.status.bits.code,
            static_cast<StatusCodeType>(SerializeStatus::Codes::kBar));
  ASSERT_EQ(builder.status.bits.extra_data, 0u);

  // Make a copy, and ensure that the root cause is carried over.
  SerializeStatus moved = wraps;
  moved.ToUKM(builder);
  ASSERT_NE(builder.root.packed, 0u);
  ASSERT_EQ(builder.root.bits.code,
            static_cast<StatusCodeType>(SerializeStatus::Codes::kFoo));
  ASSERT_EQ(builder.root.bits.extra_data, 154u);
  ASSERT_NE(builder.status.packed, 0u);
  ASSERT_EQ(builder.status.bits.code,
            static_cast<StatusCodeType>(SerializeStatus::Codes::kBar));
  ASSERT_EQ(builder.status.bits.extra_data, 0u);
}

}  // namespace media
