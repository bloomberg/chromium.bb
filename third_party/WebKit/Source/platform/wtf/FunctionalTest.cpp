/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/wtf/Functional.h"

#include <memory>
#include <utility>
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/WTFTestHelper.h"
#include "platform/wtf/WeakPtr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

class UnwrappedClass {
 public:
  explicit UnwrappedClass(int value) : value_(value) {}

  int Value() const { return value_; }

 private:
  int value_;
};

// This class must be wrapped in bind() and unwrapped in closure execution.
class ClassToBeWrapped {
  WTF_MAKE_NONCOPYABLE(ClassToBeWrapped);

 public:
  explicit ClassToBeWrapped(int value) : value_(value) {}

  int Value() const { return value_; }

 private:
  int value_;
};

class WrappedClass {
 public:
  WrappedClass(const ClassToBeWrapped& to_be_wrapped)
      : value_(to_be_wrapped.Value()) {}

  explicit WrappedClass(int value) : value_(value) {}

  UnwrappedClass Unwrap() const { return UnwrappedClass(value_); }

 private:
  int value_;
};

template <>
struct ParamStorageTraits<ClassToBeWrapped> {
  using StorageType = WrappedClass;
};

class HasWeakPtrSupport {
 public:
  HasWeakPtrSupport() : weak_ptr_factory_(this) {}

  WTF::WeakPtr<HasWeakPtrSupport> CreateWeakPtr() {
    return weak_ptr_factory_.CreateWeakPtr();
  }

  void RevokeAll() { weak_ptr_factory_.RevokeAll(); }

  void Increment(int* counter) { ++*counter; }

 private:
  WTF::WeakPtrFactory<HasWeakPtrSupport> weak_ptr_factory_;
};

}  // namespace WTF

namespace base {

template <>
struct BindUnwrapTraits<WTF::WrappedClass> {
  static WTF::UnwrappedClass Unwrap(const WTF::WrappedClass& wrapped) {
    return wrapped.Unwrap();
  }
};

}  // namespace base

namespace WTF {
namespace {

int ReturnFortyTwo() {
  return 42;
}

TEST(FunctionalTest, Basic) {
  Function<int()> return_forty_two_function = Bind(&ReturnFortyTwo);
  EXPECT_EQ(42, return_forty_two_function());
}

int MultiplyByTwo(int n) {
  return n * 2;
}

double MultiplyByOneAndAHalf(double d) {
  return d * 1.5;
}

TEST(FunctionalTest, UnaryBind) {
  Function<int()> multiply_four_by_two_function = Bind(MultiplyByTwo, 4);
  EXPECT_EQ(8, multiply_four_by_two_function());

  Function<double()> multiply_by_one_and_a_half_function =
      Bind(MultiplyByOneAndAHalf, 3);
  EXPECT_EQ(4.5, multiply_by_one_and_a_half_function());
}

TEST(FunctionalTest, UnaryPartBind) {
  Function<int(int)> multiply_by_two_function = Bind(MultiplyByTwo);
  EXPECT_EQ(8, multiply_by_two_function(4));

  Function<double(double)> multiply_by_one_and_a_half_function =
      Bind(MultiplyByOneAndAHalf);
  EXPECT_EQ(4.5, multiply_by_one_and_a_half_function(3));
}

int Multiply(int x, int y) {
  return x * y;
}

int Subtract(int x, int y) {
  return x - y;
}

TEST(FunctionalTest, BinaryBind) {
  Function<int()> multiply_four_by_two_function = Bind(Multiply, 4, 2);
  EXPECT_EQ(8, multiply_four_by_two_function());

  Function<int()> subtract_two_from_four_function = Bind(Subtract, 4, 2);
  EXPECT_EQ(2, subtract_two_from_four_function());
}

TEST(FunctionalTest, BinaryPartBind) {
  Function<int(int)> multiply_four_function = Bind(Multiply, 4);
  EXPECT_EQ(8, multiply_four_function(2));
  Function<int(int, int)> multiply_function = Bind(Multiply);
  EXPECT_EQ(8, multiply_function(4, 2));

  Function<int(int)> subtract_from_four_function = Bind(Subtract, 4);
  EXPECT_EQ(2, subtract_from_four_function(2));
  Function<int(int, int)> subtract_function = Bind(Subtract);
  EXPECT_EQ(2, subtract_function(4, 2));
}

void SixArgFunc(int a, double b, char c, int* d, double* e, char* f) {
  *d = a;
  *e = b;
  *f = c;
}

void AssertArgs(int actual_int,
                double actual_double,
                char actual_char,
                int expected_int,
                double expected_double,
                char expected_char) {
  EXPECT_EQ(expected_int, actual_int);
  EXPECT_EQ(expected_double, actual_double);
  EXPECT_EQ(expected_char, actual_char);
}

TEST(FunctionalTest, MultiPartBind) {
  int a = 0;
  double b = 0.5;
  char c = 'a';

  Function<void(int, double, char, int*, double*, char*)> unbound =
      Bind(SixArgFunc);
  unbound(1, 1.5, 'b', &a, &b, &c);
  AssertArgs(a, b, c, 1, 1.5, 'b');

  Function<void(double, char, int*, double*, char*)> one_bound =
      Bind(SixArgFunc, 2);
  one_bound(2.5, 'c', &a, &b, &c);
  AssertArgs(a, b, c, 2, 2.5, 'c');

  Function<void(char, int*, double*, char*)> two_bound =
      Bind(SixArgFunc, 3, 3.5);
  two_bound('d', &a, &b, &c);
  AssertArgs(a, b, c, 3, 3.5, 'd');

  Function<void(int*, double*, char*)> three_bound =
      Bind(SixArgFunc, 4, 4.5, 'e');
  three_bound(&a, &b, &c);
  AssertArgs(a, b, c, 4, 4.5, 'e');

  Function<void(double*, char*)> four_bound =
      Bind(SixArgFunc, 5, 5.5, 'f', WTF::Unretained(&a));
  four_bound(&b, &c);
  AssertArgs(a, b, c, 5, 5.5, 'f');

  Function<void(char*)> five_bound =
      Bind(SixArgFunc, 6, 6.5, 'g', WTF::Unretained(&a), WTF::Unretained(&b));
  five_bound(&c);
  AssertArgs(a, b, c, 6, 6.5, 'g');

  Function<void()> six_bound =
      Bind(SixArgFunc, 7, 7.5, 'h', WTF::Unretained(&a), WTF::Unretained(&b),
           WTF::Unretained(&c));
  six_bound();
  AssertArgs(a, b, c, 7, 7.5, 'h');
}

class A {
 public:
  explicit A(int i) : i_(i) {}
  virtual ~A() {}

  int F() { return i_; }
  int AddF(int j) { return i_ + j; }
  virtual int Overridden() { return 42; }

 private:
  int i_;
};

class B : public A {
 public:
  explicit B(int i) : A(i) {}

  int F() { return A::F() + 1; }
  int AddF(int j) { return A::AddF(j) + 1; }
  int Overridden() override { return 43; }
};

TEST(FunctionalTest, MemberFunctionBind) {
  A a(10);
  Function<int()> function1 = Bind(&A::F, WTF::Unretained(&a));
  EXPECT_EQ(10, function1());

  Function<int()> function2 = Bind(&A::AddF, WTF::Unretained(&a), 15);
  EXPECT_EQ(25, function2());

  Function<int()> function3 = Bind(&A::Overridden, WTF::Unretained(&a));
  EXPECT_EQ(42, function3());
}

TEST(FunctionalTest, MemberFunctionBindWithSubclassPointer) {
  B b(10);
  Function<int()> function1 = Bind(&A::F, WTF::Unretained(&b));
  EXPECT_EQ(10, function1());

  Function<int()> function2 = Bind(&A::AddF, WTF::Unretained(&b), 15);
  EXPECT_EQ(25, function2());

  Function<int()> function3 = Bind(&A::Overridden, WTF::Unretained(&b));
  EXPECT_EQ(43, function3());

  Function<int()> function4 = Bind(&B::F, WTF::Unretained(&b));
  EXPECT_EQ(11, function4());

  Function<int()> function5 = Bind(&B::AddF, WTF::Unretained(&b), 15);
  EXPECT_EQ(26, function5());
}

TEST(FunctionalTest, MemberFunctionBindWithSubclassPointerWithCast) {
  B b(10);
  Function<int()> function1 = Bind(&A::F, WTF::Unretained(static_cast<A*>(&b)));
  EXPECT_EQ(10, function1());

  Function<int()> function2 =
      Bind(&A::AddF, WTF::Unretained(static_cast<A*>(&b)), 15);
  EXPECT_EQ(25, function2());

  Function<int()> function3 =
      Bind(&A::Overridden, WTF::Unretained(static_cast<A*>(&b)));
  EXPECT_EQ(43, function3());
}

TEST(FunctionalTest, MemberFunctionPartBind) {
  A a(10);
  Function<int(class A*)> function1 = Bind(&A::F);
  EXPECT_EQ(10, function1(&a));

  Function<int(class A*, int)> unbound_function2 = Bind(&A::AddF);
  EXPECT_EQ(25, unbound_function2(&a, 15));
  Function<int(int)> object_bound_function2 =
      Bind(&A::AddF, WTF::Unretained(&a));
  EXPECT_EQ(25, object_bound_function2(15));
}

TEST(FunctionalTest, MemberFunctionBindByUniquePtr) {
  Function<int()> function1 = WTF::Bind(&A::F, std::make_unique<A>(10));
  EXPECT_EQ(10, function1());
}

TEST(FunctionalTest, MemberFunctionBindByPassedUniquePtr) {
  Function<int()> function1 =
      WTF::Bind(&A::F, WTF::Passed(std::make_unique<A>(10)));
  EXPECT_EQ(10, function1());
}

class Number {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();
  static RefPtr<Number> Create(int value) {
    return WTF::AdoptRef(new Number(value));
  }

  ~Number() { value_ = 0; }

  int Value() const { return value_; }

  int RefCount() const { return ref_count_; }
  bool HasOneRef() const { return ref_count_ == 1; }

  // For RefPtr support.
  void Adopted() const {}
  void AddRef() const { ++ref_count_; }
  void Release() const {
    if (--ref_count_ == 0)
      delete this;
  }

 private:
  explicit Number(int value) : value_(value) {}

  int value_;
  mutable int ref_count_ = 1;
};

int MultiplyNumberByTwo(Number* number) {
  return number->Value() * 2;
}

TEST(FunctionalTest, RefCountedStorage) {
  RefPtr<Number> five = Number::Create(5);
  EXPECT_TRUE(five->HasOneRef());
  Function<int()> multiply_five_by_two_function =
      WTF::Bind(MultiplyNumberByTwo, five);
  EXPECT_EQ(2, five->RefCount());
  EXPECT_EQ(10, multiply_five_by_two_function());
  EXPECT_EQ(2, five->RefCount());

  Function<int()> multiply_four_by_two_function =
      WTF::Bind(MultiplyNumberByTwo, Number::Create(4));
  EXPECT_EQ(8, multiply_four_by_two_function());

  RefPtr<Number> six = Number::Create(6);
  Function<int()> multiply_six_by_two_function =
      WTF::Bind(MultiplyNumberByTwo, std::move(six));
  EXPECT_FALSE(six);
  EXPECT_EQ(12, multiply_six_by_two_function());
}

TEST(FunctionalTest, UnretainedWithRefCounted) {
  RefPtr<Number> five = Number::Create(5);
  EXPECT_EQ(1, five->RefCount());
  Function<int()> multiply_five_by_two_function =
      WTF::Bind(MultiplyNumberByTwo, WTF::Unretained(five.get()));
  EXPECT_EQ(1, five->RefCount());
  EXPECT_EQ(10, multiply_five_by_two_function());
  EXPECT_EQ(1, five->RefCount());
}

int ProcessUnwrappedClass(const UnwrappedClass& u, int v) {
  return u.Value() * v;
}

// Tests that:
// - ParamStorageTraits's wrap()/unwrap() are called, and
// - bind()'s arguments are passed as references to ParamStorageTraits::wrap()
//   with no additional copies.
// This test would fail in compile if something is wrong,
// rather than in runtime.
TEST(FunctionalTest, WrapUnwrap) {
  Function<int()> function =
      Bind(ProcessUnwrappedClass, ClassToBeWrapped(3), 7);
  EXPECT_EQ(21, function());
}

TEST(FunctionalTest, WrapUnwrapInPartialBind) {
  Function<int(int)> partially_bound_function =
      Bind(ProcessUnwrappedClass, ClassToBeWrapped(3));
  EXPECT_EQ(21, partially_bound_function(7));
}

bool LotsOfArguments(int first,
                     int second,
                     int third,
                     int fourth,
                     int fifth,
                     int sixth,
                     int seventh,
                     int eighth,
                     int ninth,
                     int tenth) {
  return first == 1 && second == 2 && third == 3 && fourth == 4 && fifth == 5 &&
         sixth == 6 && seventh == 7 && eighth == 8 && ninth == 9 && tenth == 10;
}

TEST(FunctionalTest, LotsOfBoundVariables) {
  Function<bool(int, int)> eight_bound =
      Bind(LotsOfArguments, 1, 2, 3, 4, 5, 6, 7, 8);
  EXPECT_TRUE(eight_bound(9, 10));

  Function<bool(int)> nine_bound =
      Bind(LotsOfArguments, 1, 2, 3, 4, 5, 6, 7, 8, 9);
  EXPECT_TRUE(nine_bound(10));

  Function<bool()> all_bound =
      Bind(LotsOfArguments, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
  EXPECT_TRUE(all_bound());
}

int SingleMoveOnlyByRvalueReference(MoveOnly&& move_only) {
  int value = move_only.Value();
  MoveOnly(std::move(move_only));
  return value;
}

int TripleMoveOnlysByRvalueReferences(MoveOnly&& first,
                                      MoveOnly&& second,
                                      MoveOnly&& third) {
  int value = first.Value() + second.Value() + third.Value();
  MoveOnly(std::move(first));
  MoveOnly(std::move(second));
  MoveOnly(std::move(third));
  return value;
}

int SingleMoveOnlyByValue(MoveOnly move_only) {
  return move_only.Value();
}

int TripleMoveOnlysByValues(MoveOnly first, MoveOnly second, MoveOnly third) {
  return first.Value() + second.Value() + third.Value();
}

TEST(FunctionalTest, BindMoveOnlyObjects) {
  MoveOnly one(1);
  Function<int()> bound =
      Bind(SingleMoveOnlyByRvalueReference, WTF::Passed(std::move(one)));
  EXPECT_EQ(0, one.Value());  // Should be moved away.
  EXPECT_EQ(1, bound());
  // The stored value must be cleared in the first function call.
  EXPECT_EQ(0, bound());

  bound = Bind(SingleMoveOnlyByValue, WTF::Passed(MoveOnly(1)));
  EXPECT_EQ(1, bound());
  EXPECT_EQ(0, bound());

  bound = Bind(TripleMoveOnlysByRvalueReferences, WTF::Passed(MoveOnly(1)),
               WTF::Passed(MoveOnly(2)), WTF::Passed(MoveOnly(3)));
  EXPECT_EQ(6, bound());
  EXPECT_EQ(0, bound());

  bound = Bind(TripleMoveOnlysByValues, WTF::Passed(MoveOnly(1)),
               WTF::Passed(MoveOnly(2)), WTF::Passed(MoveOnly(3)));
  EXPECT_EQ(6, bound());
  EXPECT_EQ(0, bound());
}

class CountGeneration {
 public:
  CountGeneration() : copies_(0) {}
  CountGeneration(const CountGeneration& other) : copies_(other.copies_ + 1) {}

  int Copies() const { return copies_; }

 private:
  // Copy/move-assignment is not needed in the test.
  CountGeneration& operator=(const CountGeneration&) = delete;
  CountGeneration& operator=(CountGeneration&&) = delete;

  int copies_;
};

int TakeCountCopyAsConstReference(const CountGeneration& counter) {
  return counter.Copies();
}

int TakeCountCopyAsValue(CountGeneration counter) {
  return counter.Copies();
}

TEST(FunctionalTest, CountCopiesOfBoundArguments) {
  CountGeneration lvalue;
  Function<int()> bound = Bind(TakeCountCopyAsConstReference, lvalue);
  EXPECT_EQ(2, bound());  // wrapping and unwrapping.

  bound = Bind(TakeCountCopyAsConstReference, CountGeneration());  // Rvalue.
  EXPECT_EQ(2, bound());

  bound = Bind(TakeCountCopyAsValue, lvalue);
  // wrapping, unwrapping and copying in the final function argument.
  EXPECT_EQ(3, bound());

  bound = Bind(TakeCountCopyAsValue, CountGeneration());
  EXPECT_EQ(3, bound());
}

TEST(FunctionalTest, MoveUnboundArgumentsByRvalueReference) {
  Function<int(MoveOnly &&)> bound_single =
      Bind(SingleMoveOnlyByRvalueReference);
  EXPECT_EQ(1, bound_single(MoveOnly(1)));
  EXPECT_EQ(42, bound_single(MoveOnly(42)));

  Function<int(MoveOnly&&, MoveOnly&&, MoveOnly &&)> bound_triple =
      Bind(TripleMoveOnlysByRvalueReferences);
  EXPECT_EQ(6, bound_triple(MoveOnly(1), MoveOnly(2), MoveOnly(3)));
  EXPECT_EQ(666, bound_triple(MoveOnly(111), MoveOnly(222), MoveOnly(333)));

  Function<int(MoveOnly)> bound_single_by_value = Bind(SingleMoveOnlyByValue);
  EXPECT_EQ(1, bound_single_by_value(MoveOnly(1)));

  Function<int(MoveOnly, MoveOnly, MoveOnly)> bound_triple_by_value =
      Bind(TripleMoveOnlysByValues);
  EXPECT_EQ(6, bound_triple_by_value(MoveOnly(1), MoveOnly(2), MoveOnly(3)));
}

TEST(FunctionalTest, CountCopiesOfUnboundArguments) {
  CountGeneration lvalue;
  Function<int(const CountGeneration&)> bound1 =
      Bind(TakeCountCopyAsConstReference);
  EXPECT_EQ(0, bound1(lvalue));  // No copies!
  EXPECT_EQ(0, bound1(CountGeneration()));

  Function<int(CountGeneration)> bound2 = Bind(TakeCountCopyAsValue);
  // At Function::operator(), at Callback::Run() and at the destination
  // function.
  EXPECT_EQ(3, bound2(lvalue));
  // Compiler is allowed to optimize one copy away if the argument is rvalue.
  EXPECT_LE(bound2(CountGeneration()), 2);
}

TEST(FunctionalTest, WeakPtr) {
  HasWeakPtrSupport obj;
  int counter = 0;
  WTF::Closure bound =
      WTF::Bind(&HasWeakPtrSupport::Increment, obj.CreateWeakPtr(),
                WTF::Unretained(&counter));

  bound();
  EXPECT_FALSE(bound.IsCancelled());
  EXPECT_EQ(1, counter);

  obj.RevokeAll();
  EXPECT_TRUE(bound.IsCancelled());
  bound();
  EXPECT_EQ(1, counter);
}

}  // anonymous namespace

}  // namespace WTF
