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

#include "wtf/Functional.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/OwnPtr.h"
#include "wtf/RefCounted.h"
#include <utility>

namespace WTF {

class UnwrappedClass {
public:
    explicit UnwrappedClass(int value)
        : m_value(value)
    {
    }

    int value() const { return m_value; }

private:
    int m_value;
};

class WrappedClass {
public:
    explicit WrappedClass(int value)
        : m_value(value)
    {
    }

    UnwrappedClass unwrap() const { return UnwrappedClass(m_value); }

private:
    int m_value;
};

// This class must be wrapped in bind() and unwrapped in closure execution.
class ClassToBeWrapped {
    WTF_MAKE_NONCOPYABLE(ClassToBeWrapped);
public:
    explicit ClassToBeWrapped(int value)
        : m_value(value)
    {
    }

    WrappedClass wrap() const { return WrappedClass(m_value); }

private:
    int m_value;
};

template<> struct ParamStorageTraits<ClassToBeWrapped> {
    using StorageType = WrappedClass;
    static StorageType wrap(const ClassToBeWrapped& value) { return value.wrap(); }
    static UnwrappedClass unwrap(const StorageType& value) { return value.unwrap(); }
};

namespace {

int returnFortyTwo()
{
    return 42;
}

TEST(FunctionalTest, Basic)
{
    OwnPtr<Function<int()>> returnFortyTwoFunction = bind(returnFortyTwo);
    EXPECT_EQ(42, (*returnFortyTwoFunction)());
}

int multiplyByTwo(int n)
{
    return n * 2;
}

double multiplyByOneAndAHalf(double d)
{
    return d * 1.5;
}

TEST(FunctionalTest, UnaryBind)
{
    OwnPtr<Function<int()>> multiplyFourByTwoFunction = bind(multiplyByTwo, 4);
    EXPECT_EQ(8, (*multiplyFourByTwoFunction)());

    OwnPtr<Function<double()>> multiplyByOneAndAHalfFunction = bind(multiplyByOneAndAHalf, 3);
    EXPECT_EQ(4.5, (*multiplyByOneAndAHalfFunction)());
}

TEST(FunctionalTest, UnaryPartBind)
{
    OwnPtr<Function<int(int)>> multiplyByTwoFunction = bind<int>(multiplyByTwo);
    EXPECT_EQ(8, (*multiplyByTwoFunction)(4));

    OwnPtr<Function<double(double)>> multiplyByOneAndAHalfFunction = bind<double>(multiplyByOneAndAHalf);
    EXPECT_EQ(4.5, (*multiplyByOneAndAHalfFunction)(3));
}

int multiply(int x, int y)
{
    return x * y;
}

int subtract(int x, int y)
{
    return x - y;
}

TEST(FunctionalTest, BinaryBind)
{
    OwnPtr<Function<int()>> multiplyFourByTwoFunction = bind(multiply, 4, 2);
    EXPECT_EQ(8, (*multiplyFourByTwoFunction)());

    OwnPtr<Function<int()>> subtractTwoFromFourFunction = bind(subtract, 4, 2);
    EXPECT_EQ(2, (*subtractTwoFromFourFunction)());
}

TEST(FunctionalTest, BinaryPartBind)
{
    OwnPtr<Function<int(int)>> multiplyFourFunction = bind<int>(multiply, 4);
    EXPECT_EQ(8, (*multiplyFourFunction)(2));
    OwnPtr<Function<int(int, int)>> multiplyFunction = bind<int, int>(multiply);
    EXPECT_EQ(8, (*multiplyFunction)(4, 2));

    OwnPtr<Function<int(int)>> subtractFromFourFunction = bind<int>(subtract, 4);
    EXPECT_EQ(2, (*subtractFromFourFunction)(2));
    OwnPtr<Function<int(int, int)>> subtractFunction = bind<int, int>(subtract);
    EXPECT_EQ(2, (*subtractFunction)(4, 2));
}

void sixArgFunc(int a, double b, char c, int* d, double* e, char* f)
{
    *d = a;
    *e = b;
    *f = c;
}

void assertArgs(int actualInt, double actualDouble, char actualChar, int expectedInt, double expectedDouble, char expectedChar)
{
    EXPECT_EQ(expectedInt, actualInt);
    EXPECT_EQ(expectedDouble, actualDouble);
    EXPECT_EQ(expectedChar, actualChar);
}

TEST(FunctionalTest, MultiPartBind)
{
    int a = 0;
    double b = 0.5;
    char c = 'a';

    OwnPtr<Function<void(int, double, char, int*, double*, char*)>> unbound =
        bind<int, double, char, int*, double*, char*>(sixArgFunc);
    (*unbound)(1, 1.5, 'b', &a, &b, &c);
    assertArgs(a, b, c, 1, 1.5, 'b');

    OwnPtr<Function<void(double, char, int*, double*, char*)>> oneBound =
        bind<double, char, int*, double*, char*>(sixArgFunc, 2);
    (*oneBound)(2.5, 'c', &a, &b, &c);
    assertArgs(a, b, c, 2, 2.5, 'c');

    OwnPtr<Function<void(char, int*, double*, char*)>> twoBound =
        bind<char, int*, double*, char*>(sixArgFunc, 3, 3.5);
    (*twoBound)('d', &a, &b, &c);
    assertArgs(a, b, c, 3, 3.5, 'd');

    OwnPtr<Function<void(int*, double*, char*)>> threeBound =
        bind<int*, double*, char*>(sixArgFunc, 4, 4.5, 'e');
    (*threeBound)(&a, &b, &c);
    assertArgs(a, b, c, 4, 4.5, 'e');

    OwnPtr<Function<void(double*, char*)>> fourBound =
        bind<double*, char*>(sixArgFunc, 5, 5.5, 'f', &a);
    (*fourBound)(&b, &c);
    assertArgs(a, b, c, 5, 5.5, 'f');

    OwnPtr<Function<void(char*)>> fiveBound =
        bind<char*>(sixArgFunc, 6, 6.5, 'g', &a, &b);
    (*fiveBound)(&c);
    assertArgs(a, b, c, 6, 6.5, 'g');

    OwnPtr<Function<void()>> sixBound =
        bind(sixArgFunc, 7, 7.5, 'h', &a, &b, &c);
    (*sixBound)();
    assertArgs(a, b, c, 7, 7.5, 'h');
}

class A {
public:
    explicit A(int i)
        : m_i(i)
    {
    }

    int f() { return m_i; }
    int addF(int j) { return m_i + j; }

private:
    int m_i;
};

TEST(FunctionalTest, MemberFunctionBind)
{
    A a(10);
    OwnPtr<Function<int()>> function1 = bind(&A::f, &a);
    EXPECT_EQ(10, (*function1)());

    OwnPtr<Function<int()>> function2 = bind(&A::addF, &a, 15);
    EXPECT_EQ(25, (*function2)());
}

TEST(FunctionalTest, MemberFunctionPartBind)
{
    A a(10);
    OwnPtr<Function<int(class A*)>> function1 = bind<class A*>(&A::f);
    EXPECT_EQ(10, (*function1)(&a));

    OwnPtr<Function<int(class A*, int)>> unboundFunction2 =
        bind<class A*, int>(&A::addF);
    EXPECT_EQ(25, (*unboundFunction2)(&a, 15));
    OwnPtr<Function<int(int)>> objectBoundFunction2 =
        bind<int>(&A::addF, &a);
    EXPECT_EQ(25, (*objectBoundFunction2)(15));
}

class Number : public RefCounted<Number> {
public:
    static PassRefPtr<Number> create(int value)
    {
        return adoptRef(new Number(value));
    }

    ~Number()
    {
        m_value = 0;
    }

    int value() const { return m_value; }

private:
    explicit Number(int value)
        : m_value(value)
    {
    }

    int m_value;
};

int multiplyNumberByTwo(Number* number)
{
    return number->value() * 2;
}

TEST(FunctionalTest, RefCountedStorage)
{
    RefPtr<Number> five = Number::create(5);
    OwnPtr<Function<int()>> multiplyFiveByTwoFunction = bind(multiplyNumberByTwo, five);
    EXPECT_EQ(10, (*multiplyFiveByTwoFunction)());

    OwnPtr<Function<int()>> multiplyFourByTwoFunction = bind(multiplyNumberByTwo, Number::create(4));
    EXPECT_EQ(8, (*multiplyFourByTwoFunction)());

    RefPtr<Number> six = Number::create(6);
    OwnPtr<Function<int()>> multiplySixByTwoFunction = bind(multiplyNumberByTwo, six.release());
    EXPECT_FALSE(six);
    EXPECT_EQ(12, (*multiplySixByTwoFunction)());
}

int processUnwrappedClass(const UnwrappedClass& u, int v)
{
    return u.value() * v;
}

// Tests that:
// - ParamStorageTraits's wrap()/unwrap() are called, and
// - bind()'s arguments are passed as references to ParamStorageTraits::wrap()
//   with no additional copies.
// This test would fail in compile if something is wrong,
// rather than in runtime.
TEST(FunctionalTest, WrapUnwrap)
{
    OwnPtr<Function<int()>> function = bind(processUnwrappedClass, ClassToBeWrapped(3), 7);
    EXPECT_EQ(21, (*function)());
}

TEST(FunctionalTest, WrapUnwrapInPartialBind)
{
    OwnPtr<Function<int(int)>> partiallyBoundFunction = bind<int>(processUnwrappedClass, ClassToBeWrapped(3));
    EXPECT_EQ(21, (*partiallyBoundFunction)(7));
}

bool lotsOfArguments(int first, int second, int third, int fourth, int fifth, int sixth, int seventh, int eighth, int ninth, int tenth)
{
    return first == 1 && second == 2 && third == 3 && fourth == 4 && fifth == 5 && sixth == 6 && seventh == 7 && eighth == 8 && ninth == 9 && tenth == 10;
}

TEST(FunctionalTest, LotsOfBoundVariables)
{
    OwnPtr<Function<bool(int, int)>> eightBound = bind<int, int>(lotsOfArguments, 1, 2, 3, 4, 5, 6, 7, 8);
    EXPECT_TRUE((*eightBound)(9, 10));

    OwnPtr<Function<bool(int)>> nineBound = bind<int>(lotsOfArguments, 1, 2, 3, 4, 5, 6, 7, 8, 9);
    EXPECT_TRUE((*nineBound)(10));

    OwnPtr<Function<bool()>> allBound = bind(lotsOfArguments, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    EXPECT_TRUE((*allBound)());
}

class MoveOnly {
public:
    explicit MoveOnly(int value) : m_value(value) { }
    MoveOnly(MoveOnly&& other)
        : m_value(other.m_value)
    {
        // Reset the value on move.
        other.m_value = 0;
    }

    int value() const { return m_value; }

private:
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;

    // Disable move-assignment, since it isn't used within bind().
    MoveOnly& operator=(MoveOnly&&) = delete;

    int m_value;
};

int singleMoveOnlyByRvalueReference(MoveOnly&& moveOnly)
{
    int value = moveOnly.value();
    MoveOnly(std::move(moveOnly));
    return value;
}

int tripleMoveOnlysByRvalueReferences(MoveOnly&& first, MoveOnly&& second, MoveOnly&& third)
{
    int value = first.value() + second.value() + third.value();
    MoveOnly(std::move(first));
    MoveOnly(std::move(second));
    MoveOnly(std::move(third));
    return value;
}

int singleMoveOnlyByValue(MoveOnly moveOnly)
{
    return moveOnly.value();
}

int tripleMoveOnlysByValues(MoveOnly first, MoveOnly second, MoveOnly third)
{
    return first.value() + second.value() + third.value();
}

TEST(FunctionalTest, BindMoveOnlyObjects)
{
    MoveOnly one(1);
    OwnPtr<Function<int()>> bound = bind(singleMoveOnlyByRvalueReference, passed(std::move(one)));
    EXPECT_EQ(0, one.value()); // Should be moved away.
    EXPECT_EQ(1, (*bound)());
    EXPECT_EQ(0, (*bound)()); // The stored value must be cleared in the first function call.

    bound = bind(singleMoveOnlyByValue, passed(MoveOnly(1)));
    EXPECT_EQ(1, (*bound)());
    EXPECT_EQ(0, (*bound)());

    bound = bind(tripleMoveOnlysByRvalueReferences, passed(MoveOnly(1)), passed(MoveOnly(2)), passed(MoveOnly(3)));
    EXPECT_EQ(6, (*bound)());
    EXPECT_EQ(0, (*bound)());

    bound = bind(tripleMoveOnlysByValues, passed(MoveOnly(1)), passed(MoveOnly(2)), passed(MoveOnly(3)));
    EXPECT_EQ(6, (*bound)());
    EXPECT_EQ(0, (*bound)());
}

class CountCopy {
public:
    CountCopy() : m_copies(0) { }
    CountCopy(const CountCopy& other) : m_copies(other.m_copies + 1) { }

    int copies() const { return m_copies; }

private:
    // Copy/move-assignment is not needed in the test.
    CountCopy& operator=(const CountCopy&) = delete;
    CountCopy& operator=(CountCopy&&) = delete;

    int m_copies;
};

int takeCountCopyAsConstReference(const CountCopy& counter)
{
    return counter.copies();
}

int takeCountCopyAsValue(CountCopy counter)
{
    return counter.copies();
}

TEST(FunctionalTest, CountCopiesOfBoundArguments)
{
    CountCopy lvalue;
    OwnPtr<Function<int()>> bound = bind(takeCountCopyAsConstReference, lvalue);
    EXPECT_EQ(2, (*bound)()); // wrapping and unwrapping.

    bound = bind(takeCountCopyAsConstReference, CountCopy()); // Rvalue.
    EXPECT_EQ(2, (*bound)());

    bound = bind(takeCountCopyAsValue, lvalue);
    EXPECT_EQ(3, (*bound)()); // wrapping, unwrapping and copying in the final function argument.

    bound = bind(takeCountCopyAsValue, CountCopy());
    EXPECT_EQ(3, (*bound)());
}

TEST(FunctionalTest, MoveUnboundArgumentsByRvalueReference)
{
    OwnPtr<Function<int(MoveOnly&&)>> boundSingle = bind<MoveOnly&&>(singleMoveOnlyByRvalueReference);
    EXPECT_EQ(1, (*boundSingle)(MoveOnly(1)));
    EXPECT_EQ(42, (*boundSingle)(MoveOnly(42)));

    OwnPtr<Function<int(MoveOnly&&, MoveOnly&&, MoveOnly&&)>> boundTriple = bind<MoveOnly&&, MoveOnly&&, MoveOnly&&>(tripleMoveOnlysByRvalueReferences);
    EXPECT_EQ(6, (*boundTriple)(MoveOnly(1), MoveOnly(2), MoveOnly(3)));
    EXPECT_EQ(666, (*boundTriple)(MoveOnly(111), MoveOnly(222), MoveOnly(333)));

    OwnPtr<Function<int(MoveOnly)>> boundSingleByValue = bind<MoveOnly>(singleMoveOnlyByValue);
    EXPECT_EQ(1, (*boundSingleByValue)(MoveOnly(1)));

    OwnPtr<Function<int(MoveOnly, MoveOnly, MoveOnly)>> boundTripleByValue = bind<MoveOnly, MoveOnly, MoveOnly>(tripleMoveOnlysByValues);
    EXPECT_EQ(6, (*boundTripleByValue)(MoveOnly(1), MoveOnly(2), MoveOnly(3)));
}

TEST(FunctionalTest, CountCopiesOfUnboundArguments)
{
    CountCopy lvalue;
    OwnPtr<Function<int(const CountCopy&)>> bound1 = bind<const CountCopy&>(takeCountCopyAsConstReference);
    EXPECT_EQ(0, (*bound1)(lvalue)); // No copies!
    EXPECT_EQ(0, (*bound1)(CountCopy()));

    OwnPtr<Function<int(CountCopy)>> bound2 = bind<CountCopy>(takeCountCopyAsValue);
    EXPECT_EQ(2, (*bound2)(lvalue)); // At PartBoundFunctionImpl::operator() and at the destination function.
    EXPECT_LE((*bound2)(CountCopy()), 2); // Compiler is allowed to optimize one copy away if the argument is rvalue.
}

} // anonymous namespace

} // namespace WTF
