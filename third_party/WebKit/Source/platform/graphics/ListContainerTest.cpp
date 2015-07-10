// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/ListContainer.h"

#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"
#include <algorithm>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using testing::Test;

namespace blink {
namespace {

// Element class having derived classes.
class DerivedElement {
public:
    virtual ~DerivedElement() {}

protected:
    bool boolValues[1];
    char charValues[1];
    int intValues[1];
    long longValues[1];
};

class DerivedElement1 : public DerivedElement {
protected:
    bool boolValues1[1];
    char charValues1[1];
    int intValues1[1];
    long longValues1[1];
};

class DerivedElement2 : public DerivedElement {
protected:
    bool boolValues2[2];
    char charValues2[2];
    int intValues2[2];
    long longValues2[2];
};

class DerivedElement3 : public DerivedElement {
protected:
    bool boolValues3[3];
    char charValues3[3];
    int intValues3[3];
    long longValues3[3];
};

const size_t kLargestDerivedElementSize = sizeof(DerivedElement3);

size_t largestDerivedElementSize()
{
    static_assert(sizeof(DerivedElement1) <= kLargestDerivedElementSize,
        "Largest Derived Element size needs update. DerivedElement1 is currently largest.");
    static_assert(sizeof(DerivedElement2) <= kLargestDerivedElementSize,
        "Largest Derived Element size needs update. DerivedElement2 is currently largest.");

    return kLargestDerivedElementSize;
}

// Element class having no derived classes.
class NonDerivedElement {
public:
    NonDerivedElement() {}
    ~NonDerivedElement() {}

    int intValues[1];
};

bool isConstNonDerivedElementPointer(const NonDerivedElement* ptr)
{
    return true;
}

bool isConstNonDerivedElementPointer(NonDerivedElement* ptr)
{
    return false;
}

const int kMagicNumberToUseForSimpleDerivedElementOne = 42;
const int kMagicNumberToUseForSimpleDerivedElementTwo = 314;
const int kMagicNumberToUseForSimpleDerivedElementThree = 1618;

class SimpleDerivedElement : public DerivedElement {
public:
    ~SimpleDerivedElement() override {}
    void setValue(int val) { value = val; }
    int getValue() { return value; }

private:
    int value;
};

class SimpleDerivedElementConstructMagicNumberOne : public SimpleDerivedElement {
public:
    SimpleDerivedElementConstructMagicNumberOne()
    {
        setValue(kMagicNumberToUseForSimpleDerivedElementOne);
    }
};

class SimpleDerivedElementConstructMagicNumberTwo : public SimpleDerivedElement {
public:
    SimpleDerivedElementConstructMagicNumberTwo()
    {
        setValue(kMagicNumberToUseForSimpleDerivedElementTwo);
    }
};

class SimpleDerivedElementConstructMagicNumberThree : public SimpleDerivedElement {
public:
    SimpleDerivedElementConstructMagicNumberThree()
    {
        setValue(kMagicNumberToUseForSimpleDerivedElementThree);
    }
};

class MockDerivedElement : public SimpleDerivedElementConstructMagicNumberOne {
public:
    ~MockDerivedElement() override { Destruct(); }
    MOCK_METHOD0(Destruct, void());
};

class MockDerivedElementSubclass : public MockDerivedElement {
public:
    MockDerivedElementSubclass()
    {
        setValue(kMagicNumberToUseForSimpleDerivedElementTwo);
    }
};

const size_t kCurrentLargestDerivedElementSize = std::max(largestDerivedElementSize(), sizeof(MockDerivedElementSubclass));

TEST(ListContainerTest, ConstructorCalledInAllocateAndConstruct)
{
    ListContainer<DerivedElement> list(kCurrentLargestDerivedElementSize);

    size_t size = 2;
    SimpleDerivedElementConstructMagicNumberOne* derivedElement1 = list.allocateAndConstruct<SimpleDerivedElementConstructMagicNumberOne>();
    SimpleDerivedElementConstructMagicNumberTwo* derivedElement2 = list.allocateAndConstruct<SimpleDerivedElementConstructMagicNumberTwo>();

    EXPECT_EQ(size, list.size());
    EXPECT_EQ(derivedElement1, list.front());
    EXPECT_EQ(derivedElement2, list.back());

    EXPECT_EQ(kMagicNumberToUseForSimpleDerivedElementOne, derivedElement1->getValue());
    EXPECT_EQ(kMagicNumberToUseForSimpleDerivedElementTwo, derivedElement2->getValue());
}

TEST(ListContainerTest, AllocateAndConstructWithArguments)
{
    // Create a new OwnPtr<SimpleDerivedElement> by passing in a new SimpleDerivedElement.
    ListContainer<OwnPtr<SimpleDerivedElement>> list1(kCurrentLargestDerivedElementSize);
    auto* sdeOwnPtr1 = list1.allocateAndConstruct<OwnPtr<SimpleDerivedElement>>(adoptPtr(new SimpleDerivedElementConstructMagicNumberOne()));
    EXPECT_EQ(1u, list1.size());
    EXPECT_EQ(sdeOwnPtr1, list1.back());
    EXPECT_EQ(kMagicNumberToUseForSimpleDerivedElementOne, (*sdeOwnPtr1)->getValue());
    (*sdeOwnPtr1)->setValue(kMagicNumberToUseForSimpleDerivedElementTwo);

    // Transfer an OwnPtr<SimpleDerivedElement> to another list.
    ListContainer<OwnPtr<SimpleDerivedElement>> list2(kCurrentLargestDerivedElementSize);
    auto* sdeOwnPtr2 = list2.allocateAndConstruct<OwnPtr<SimpleDerivedElement>>(sdeOwnPtr1->release());
    EXPECT_EQ(1u, list2.size());
    EXPECT_EQ(sdeOwnPtr2, list2.back());
    EXPECT_EQ(kMagicNumberToUseForSimpleDerivedElementTwo, (*sdeOwnPtr2)->getValue());

    // Verify the first OwnPtr is null after moving.
    EXPECT_EQ(nullptr, list1.back()->get());
}

TEST(ListContainerTest, DestructorCalled)
{
    ListContainer<DerivedElement> list(kCurrentLargestDerivedElementSize);

    size_t size = 1;
    MockDerivedElement* derivedElement1 = list.allocateAndConstruct<MockDerivedElement>();

    EXPECT_CALL(*derivedElement1, Destruct());
    EXPECT_EQ(size, list.size());
    EXPECT_EQ(derivedElement1, list.front());
}

TEST(ListContainerTest, DestructorCalledOnceWhenClear)
{
    ListContainer<DerivedElement> list(kCurrentLargestDerivedElementSize);
    size_t size = 1;
    MockDerivedElement* derivedElement1 = list.allocateAndConstruct<MockDerivedElement>();

    EXPECT_EQ(size, list.size());
    EXPECT_EQ(derivedElement1, list.front());

    // Make sure destructor is called once during clear, and won't be called
    // again.
    testing::MockFunction<void()> separator;
    {
        testing::InSequence s;
        EXPECT_CALL(*derivedElement1, Destruct());
        EXPECT_CALL(separator, Call());
        EXPECT_CALL(*derivedElement1, Destruct()).Times(0);
    }

    list.clear();
    separator.Call();
}

TEST(ListContainerTest, ClearDoesNotMalloc)
{
    const size_t reserve = 10;
    ListContainer<DerivedElement> list(kCurrentLargestDerivedElementSize, reserve);

    // Memory from the initial inner list that should be re-used after clear().
    Vector<DerivedElement*> reservedElementPointers;
    for (size_t i = 0; i < reserve; i++) {
        DerivedElement* element = list.allocateAndConstruct<DerivedElement>();
        reservedElementPointers.append(element);
    }
    EXPECT_EQ(0u, list.availableSizeWithoutAnotherAllocationForTesting());

    // Allocate more than the reserve count, forcing new capacity to be added.
    list.allocateAndConstruct<DerivedElement>();
    EXPECT_NE(0u, list.availableSizeWithoutAnotherAllocationForTesting());

    // Clear should free all memory except the first |reserve| elements.
    list.clear();
    EXPECT_EQ(reserve, list.availableSizeWithoutAnotherAllocationForTesting());

    // Verify the first |reserve| elements are re-used after clear().
    for (size_t i = 0; i < reserve; i++) {
        DerivedElement* element = list.allocateAndConstruct<DerivedElement>();
        EXPECT_EQ(element, reservedElementPointers[i]);
    }
    EXPECT_EQ(0u, list.availableSizeWithoutAnotherAllocationForTesting());

    // Verify that capacity can still grow properly.
    list.allocateAndConstruct<DerivedElement>();
    EXPECT_NE(0u, list.availableSizeWithoutAnotherAllocationForTesting());
}

TEST(ListContainerTest, ReplaceExistingElement)
{
    ListContainer<DerivedElement> list(kCurrentLargestDerivedElementSize);
    size_t size = 1;
    MockDerivedElement* derivedElement1 = list.allocateAndConstruct<MockDerivedElement>();

    EXPECT_EQ(size, list.size());
    EXPECT_EQ(derivedElement1, list.front());

    // Make sure destructor is called once during clear, and won't be called
    // again.
    testing::MockFunction<void()> separator;
    {
        testing::InSequence s;
        EXPECT_CALL(*derivedElement1, Destruct());
        EXPECT_CALL(separator, Call());
        EXPECT_CALL(*derivedElement1, Destruct()).Times(0);
    }

    list.replaceExistingElement<MockDerivedElementSubclass>(list.begin());
    EXPECT_EQ(kMagicNumberToUseForSimpleDerivedElementTwo, derivedElement1->getValue());
    separator.Call();

    EXPECT_CALL(*derivedElement1, Destruct());
    list.clear();
}

TEST(ListContainerTest, DestructorCalledOnceWhenErase)
{
    ListContainer<DerivedElement> list(kCurrentLargestDerivedElementSize);
    size_t size = 1;
    MockDerivedElement* derivedElement1 = list.allocateAndConstruct<MockDerivedElement>();

    EXPECT_EQ(size, list.size());
    EXPECT_EQ(derivedElement1, list.front());

    // Make sure destructor is called once during clear, and won't be called
    // again.
    testing::MockFunction<void()> separator;
    {
        testing::InSequence s;
        EXPECT_CALL(*derivedElement1, Destruct());
        EXPECT_CALL(separator, Call());
        EXPECT_CALL(*derivedElement1, Destruct()).Times(0);
    }

    list.eraseAndInvalidateAllPointers(list.begin());
    separator.Call();
}

TEST(ListContainerTest, SimpleIndexAccessNonDerivedElement)
{
    ListContainer<NonDerivedElement> list;

    size_t size = 3;
    NonDerivedElement* nonDerivedElement1 = list.allocateAndConstruct<NonDerivedElement>();
    NonDerivedElement* nonDerivedElement2 = list.allocateAndConstruct<NonDerivedElement>();
    NonDerivedElement* nonDerivedElement3 = list.allocateAndConstruct<NonDerivedElement>();

    EXPECT_EQ(size, list.size());
    EXPECT_EQ(nonDerivedElement1, list.front());
    EXPECT_EQ(nonDerivedElement3, list.back());
    EXPECT_EQ(list.front(), list.elementAt(0));
    EXPECT_EQ(nonDerivedElement2, list.elementAt(1));
    EXPECT_EQ(list.back(), list.elementAt(2));
}

TEST(ListContainerTest, SimpleInsertionNonDerivedElement)
{
    ListContainer<NonDerivedElement> list;

    size_t size = 3;
    NonDerivedElement* nonDerivedElement1 = list.allocateAndConstruct<NonDerivedElement>();
    list.allocateAndConstruct<NonDerivedElement>();
    NonDerivedElement* nonDerivedElement3 = list.allocateAndConstruct<NonDerivedElement>();

    EXPECT_EQ(size, list.size());
    EXPECT_EQ(nonDerivedElement1, list.front());
    EXPECT_EQ(nonDerivedElement3, list.back());
}

TEST(ListContainerTest, SimpleInsertionAndClearNonDerivedElement)
{
    ListContainer<NonDerivedElement> list;
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(0u, list.size());

    size_t size = 3;
    NonDerivedElement* nonDerivedElement1 = list.allocateAndConstruct<NonDerivedElement>();
    list.allocateAndConstruct<NonDerivedElement>();
    NonDerivedElement* nonDerivedElement3 = list.allocateAndConstruct<NonDerivedElement>();

    EXPECT_EQ(size, list.size());
    EXPECT_EQ(nonDerivedElement1, list.front());
    EXPECT_EQ(nonDerivedElement3, list.back());
    EXPECT_FALSE(list.empty());

    list.clear();
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(0u, list.size());
}

TEST(ListContainerTest, SimpleInsertionClearAndInsertAgainNonDerivedElement)
{
    ListContainer<NonDerivedElement> list;
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(0u, list.size());

    size_t size = 2;
    NonDerivedElement* nonDerivedElementfront = list.allocateAndConstruct<NonDerivedElement>();
    NonDerivedElement* nonDerivedElementback = list.allocateAndConstruct<NonDerivedElement>();

    EXPECT_EQ(size, list.size());
    EXPECT_EQ(nonDerivedElementfront, list.front());
    EXPECT_EQ(nonDerivedElementback, list.back());
    EXPECT_FALSE(list.empty());

    list.clear();
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(0u, list.size());

    size = 3;
    nonDerivedElementfront = list.allocateAndConstruct<NonDerivedElement>();
    list.allocateAndConstruct<NonDerivedElement>();
    nonDerivedElementback = list.allocateAndConstruct<NonDerivedElement>();

    EXPECT_EQ(size, list.size());
    EXPECT_EQ(nonDerivedElementfront, list.front());
    EXPECT_EQ(nonDerivedElementback, list.back());
    EXPECT_FALSE(list.empty());
}

// This test is used to test when there is more than one allocation needed
// for, ListContainer can still perform like normal vector.
TEST(ListContainerTest, SimpleInsertionTriggerMoreThanOneAllocationNonDerivedElement)
{
    ListContainer<NonDerivedElement> list(sizeof(NonDerivedElement), 2);
    std::vector<NonDerivedElement*> nonDerivedElementlist;
    size_t size = 10;
    for (size_t i = 0; i < size; ++i)
        nonDerivedElementlist.push_back(list.allocateAndConstruct<NonDerivedElement>());
    EXPECT_EQ(size, list.size());

    ListContainer<NonDerivedElement>::Iterator iter = list.begin();
    for (std::vector<NonDerivedElement*>::const_iterator nonderivedElementIter = nonDerivedElementlist.begin(); nonderivedElementIter != nonDerivedElementlist.end(); ++nonderivedElementIter) {
        EXPECT_EQ(*nonderivedElementIter, *iter);
        ++iter;
    }
}

TEST(ListContainerTest, CorrectAllocationSizeForMoreThanOneAllocationNonDerivedElement)
{
    // Constructor sets the allocation size to 2. Every time ListContainer needs
    // to allocate again, it doubles allocation size. In this test, 10 elements is
    // needed, thus ListContainerShould allocate spaces 2, 4 and 8 elements.
    ListContainer<NonDerivedElement> list(sizeof(NonDerivedElement), 2);
    std::vector<NonDerivedElement*> nonDerivedElementlist;
    size_t size = 10;
    for (size_t i = 0; i < size; ++i) {
        // Before asking for a new element, space available without another
        // allocation follows.
        switch (i) {
        case 2:
        case 6:
            EXPECT_EQ(0u, list.availableSizeWithoutAnotherAllocationForTesting());
            break;
        case 1:
        case 5:
            EXPECT_EQ(1u, list.availableSizeWithoutAnotherAllocationForTesting());
            break;
        case 0:
        case 4:
            EXPECT_EQ(2u, list.availableSizeWithoutAnotherAllocationForTesting());
            break;
        case 3:
            EXPECT_EQ(3u, list.availableSizeWithoutAnotherAllocationForTesting());
            break;
        case 9:
            EXPECT_EQ(5u, list.availableSizeWithoutAnotherAllocationForTesting());
            break;
        case 8:
            EXPECT_EQ(6u, list.availableSizeWithoutAnotherAllocationForTesting());
            break;
        case 7:
            EXPECT_EQ(7u, list.availableSizeWithoutAnotherAllocationForTesting());
            break;
        default:
            break;
        }
        nonDerivedElementlist.push_back(list.allocateAndConstruct<NonDerivedElement>());
        // After asking for a new element, space available without another
        // allocation follows.
        switch (i) {
        case 1:
        case 5:
            EXPECT_EQ(0u, list.availableSizeWithoutAnotherAllocationForTesting());
            break;
        case 0:
        case 4:
            EXPECT_EQ(1u, list.availableSizeWithoutAnotherAllocationForTesting());
            break;
        case 3:
            EXPECT_EQ(2u, list.availableSizeWithoutAnotherAllocationForTesting());
            break;
        case 2:
            EXPECT_EQ(3u, list.availableSizeWithoutAnotherAllocationForTesting());
            break;
        case 9:
            EXPECT_EQ(4u, list.availableSizeWithoutAnotherAllocationForTesting());
            break;
        case 8:
            EXPECT_EQ(5u, list.availableSizeWithoutAnotherAllocationForTesting());
            break;
        case 7:
            EXPECT_EQ(6u, list.availableSizeWithoutAnotherAllocationForTesting());
            break;
        case 6:
            EXPECT_EQ(7u, list.availableSizeWithoutAnotherAllocationForTesting());
            break;
        default:
            break;
        }
    }
    EXPECT_EQ(size, list.size());

    ListContainer<NonDerivedElement>::Iterator iter = list.begin();
    for (std::vector<NonDerivedElement*>::const_iterator nonderivedElementIter = nonDerivedElementlist.begin(); nonderivedElementIter != nonDerivedElementlist.end(); ++nonderivedElementIter) {
        EXPECT_EQ(*nonderivedElementIter, *iter);
        ++iter;
    }
}

TEST(ListContainerTest, SimpleIterationNonDerivedElement)
{
    ListContainer<NonDerivedElement> list;
    std::vector<NonDerivedElement*> nonDerivedElementlist;
    size_t size = 10;
    for (size_t i = 0; i < size; ++i)
        nonDerivedElementlist.push_back(list.allocateAndConstruct<NonDerivedElement>());
    EXPECT_EQ(size, list.size());

    size_t numItersInList = 0;
    {
        std::vector<NonDerivedElement*>::const_iterator nonderivedElementIter = nonDerivedElementlist.begin();
        for (ListContainer<NonDerivedElement>::Iterator iter = list.begin(); iter != list.end(); ++iter) {
            EXPECT_EQ(*nonderivedElementIter, *iter);
            ++numItersInList;
            ++nonderivedElementIter;
        }
    }

    size_t numItersInVector = 0;
    {
        ListContainer<NonDerivedElement>::Iterator iter = list.begin();
        for (std::vector<NonDerivedElement*>::const_iterator nonderivedElementIter = nonDerivedElementlist.begin(); nonderivedElementIter != nonDerivedElementlist.end(); ++nonderivedElementIter) {
            EXPECT_EQ(*nonderivedElementIter, *iter);
            ++numItersInVector;
            ++iter;
        }
    }

    EXPECT_EQ(numItersInVector, numItersInList);
}

TEST(ListContainerTest, SimpleConstIteratorIterationNonDerivedElement)
{
    ListContainer<NonDerivedElement> list;
    std::vector<const NonDerivedElement*> nonDerivedElementlist;
    size_t size = 10;
    for (size_t i = 0; i < size; ++i)
        nonDerivedElementlist.push_back(list.allocateAndConstruct<NonDerivedElement>());
    EXPECT_EQ(size, list.size());

    {
        std::vector<const NonDerivedElement*>::const_iterator nonderivedElementIter = nonDerivedElementlist.begin();
        for (ListContainer<NonDerivedElement>::ConstIterator iter = list.begin(); iter != list.end(); ++iter) {
            EXPECT_TRUE(isConstNonDerivedElementPointer(*iter));
            EXPECT_EQ(*nonderivedElementIter, *iter);
            ++nonderivedElementIter;
        }
    }

    {
        std::vector<const NonDerivedElement*>::const_iterator nonderivedElementIter = nonDerivedElementlist.begin();
        for (ListContainer<NonDerivedElement>::Iterator iter = list.begin(); iter != list.end(); ++iter) {
            EXPECT_FALSE(isConstNonDerivedElementPointer(*iter));
            EXPECT_EQ(*nonderivedElementIter, *iter);
            ++nonderivedElementIter;
        }
    }

    {
        ListContainer<NonDerivedElement>::ConstIterator iter = list.begin();
        for (std::vector<const NonDerivedElement*>::const_iterator nonderivedElementIter = nonDerivedElementlist.begin(); nonderivedElementIter != nonDerivedElementlist.end(); ++nonderivedElementIter) {
            EXPECT_EQ(*nonderivedElementIter, *iter);
            ++iter;
        }
    }
}

TEST(ListContainerTest, SimpleReverseInsertionNonDerivedElement)
{
    ListContainer<NonDerivedElement> list;
    std::vector<NonDerivedElement*> nonDerivedElementlist;
    size_t size = 10;
    for (size_t i = 0; i < size; ++i)
        nonDerivedElementlist.push_back(list.allocateAndConstruct<NonDerivedElement>());
    EXPECT_EQ(size, list.size());

    {
        std::vector<NonDerivedElement*>::const_reverse_iterator nonderivedElementIter =
        nonDerivedElementlist.rbegin();
        for (ListContainer<NonDerivedElement>::ReverseIterator iter = list.rbegin(); iter != list.rend(); ++iter) {
            EXPECT_EQ(*nonderivedElementIter, *iter);
            ++nonderivedElementIter;
        }
    }

    {
        ListContainer<NonDerivedElement>::ReverseIterator iter = list.rbegin();
        for (std::vector<NonDerivedElement*>::reverse_iterator nonderivedElementIter = nonDerivedElementlist.rbegin(); nonderivedElementIter != nonDerivedElementlist.rend(); ++nonderivedElementIter) {
            EXPECT_EQ(*nonderivedElementIter, *iter);
            ++iter;
        }
    }
}

TEST(ListContainerTest, SimpleDeletion)
{
    ListContainer<DerivedElement> list(kCurrentLargestDerivedElementSize);
    std::vector<SimpleDerivedElement*> simpleDerivedElementList;
    int size = 10;
    for (int i = 0; i < size; ++i) {
        simpleDerivedElementList.push_back(list.allocateAndConstruct<SimpleDerivedElement>());
        simpleDerivedElementList.back()->setValue(i);
    }
    EXPECT_EQ(static_cast<size_t>(size), list.size());

    list.eraseAndInvalidateAllPointers(list.begin());
    --size;
    EXPECT_EQ(static_cast<size_t>(size), list.size());
    int i = 1;
    for (ListContainer<DerivedElement>::Iterator iter = list.begin(); iter != list.end(); ++iter) {
        EXPECT_EQ(i, static_cast<SimpleDerivedElement*>(*iter)->getValue());
        ++i;
    }
}

TEST(ListContainerTest, DeletionAllInAllocation)
{
    const size_t kReserve = 10;
    ListContainer<DerivedElement> list(kCurrentLargestDerivedElementSize, kReserve);
    std::vector<SimpleDerivedElement*> simpleDerivedElementList;
    // Add enough elements to cause another allocation.
    for (size_t i = 0; i < kReserve + 1; ++i) {
        simpleDerivedElementList.push_back(list.allocateAndConstruct<SimpleDerivedElement>());
        simpleDerivedElementList.back()->setValue(static_cast<int>(i));
    }
    EXPECT_EQ(kReserve + 1, list.size());

    // Remove everything in the first allocation.
    for (size_t i = 0; i < kReserve; ++i)
        list.eraseAndInvalidateAllPointers(list.begin());
    EXPECT_EQ(1u, list.size());

    // The last element is left.
    SimpleDerivedElement* de = static_cast<SimpleDerivedElement*>(*list.begin());
    EXPECT_EQ(static_cast<int>(kReserve), de->getValue());

    // Remove the element from the 2nd allocation.
    list.eraseAndInvalidateAllPointers(list.begin());
    EXPECT_EQ(0u, list.size());
}

TEST(ListContainerTest, DeletionAllInAllocationReversed)
{
    const size_t kReserve = 10;
    ListContainer<DerivedElement> list(kCurrentLargestDerivedElementSize, kReserve);
    std::vector<SimpleDerivedElement*> simpleDerivedElementList;
    // Add enough elements to cause another allocation.
    for (size_t i = 0; i < kReserve + 1; ++i) {
        simpleDerivedElementList.push_back(list.allocateAndConstruct<SimpleDerivedElement>());
        simpleDerivedElementList.back()->setValue(static_cast<int>(i));
    }
    EXPECT_EQ(kReserve + 1, list.size());

    // Remove everything in the 2nd allocation.
    auto it = list.begin();
    for (size_t i = 0; i < kReserve; ++i)
        ++it;
    list.eraseAndInvalidateAllPointers(it);

    // The 2nd-last element is next, and the rest of the elements exist.
    size_t i = kReserve - 1;
    for (auto it = list.rbegin(); it != list.rend(); ++it) {
        SimpleDerivedElement* de = static_cast<SimpleDerivedElement*>(*it);
        EXPECT_EQ(static_cast<int>(i), de->getValue());
        --i;
    }

    // Can forward iterate too.
    i = 0;
    for (auto it = list.begin(); it != list.end(); ++it) {
        SimpleDerivedElement* de = static_cast<SimpleDerivedElement*>(*it);
        EXPECT_EQ(static_cast<int>(i), de->getValue());
        ++i;
    }

    // Remove the last thing from the 1st allocation.
    it = list.begin();
    for (size_t i = 0; i < kReserve - 1; ++i)
        ++it;
    list.eraseAndInvalidateAllPointers(it);

    // The 2nd-last element is next, and the rest of the elements exist.
    i = kReserve - 2;
    for (auto it = list.rbegin(); it != list.rend(); ++it) {
        SimpleDerivedElement* de = static_cast<SimpleDerivedElement*>(*it);
        EXPECT_EQ(static_cast<int>(i), de->getValue());
        --i;
    }

    // Can forward iterate too.
    i = 0;
    for (auto it = list.begin(); it != list.end(); ++it) {
        SimpleDerivedElement* de = static_cast<SimpleDerivedElement*>(*it);
        EXPECT_EQ(static_cast<int>(i), de->getValue());
        ++i;
    }
}

TEST(ListContainerTest, SimpleIterationAndManipulation)
{
    ListContainer<DerivedElement> list(kCurrentLargestDerivedElementSize);
    std::vector<SimpleDerivedElement*> simpleDerivedElementList;
    size_t size = 10;
    for (size_t i = 0; i < size; ++i) {
        SimpleDerivedElement* simpleDerivedElement = list.allocateAndConstruct<SimpleDerivedElement>();
        simpleDerivedElementList.push_back(simpleDerivedElement);
    }
    EXPECT_EQ(size, list.size());

    ListContainer<DerivedElement>::Iterator iter = list.begin();
    for (int i = 0; i < 10; ++i) {
        static_cast<SimpleDerivedElement*>(*iter)->setValue(i);
        ++iter;
    }

    int i = 0;
    for (std::vector<SimpleDerivedElement*>::const_iterator simpleNonderivedElementIter =
        simpleDerivedElementList.begin();
        simpleNonderivedElementIter < simpleDerivedElementList.end(); ++simpleNonderivedElementIter) {
        EXPECT_EQ(i, (*simpleNonderivedElementIter)->getValue());
        ++i;
    }
}

TEST(ListContainerTest, SimpleManipulationWithIndexSimpleDerivedElement)
{
    ListContainer<DerivedElement> list(kCurrentLargestDerivedElementSize);
    std::vector<SimpleDerivedElement*> derivedElementlist;
    int size = 10;
    for (int i = 0; i < size; ++i)
        derivedElementlist.push_back(list.allocateAndConstruct<SimpleDerivedElement>());
    EXPECT_EQ(static_cast<size_t>(size), list.size());

    for (int i = 0; i < size; ++i)
        static_cast<SimpleDerivedElement*>(list.elementAt(i))->setValue(i);

    int i = 0;
    for (std::vector<SimpleDerivedElement*>::const_iterator derivedElementIter = derivedElementlist.begin(); derivedElementIter != derivedElementlist.end(); ++derivedElementIter, ++i)
        EXPECT_EQ(i, (*derivedElementIter)->getValue());
}

TEST(ListContainerTest, SimpleManipulationWithIndexMoreThanOneAllocationSimpleDerivedElement)
{
    ListContainer<DerivedElement> list(largestDerivedElementSize(), 2);
    std::vector<SimpleDerivedElement*> derivedElementlist;
    int size = 10;
    for (int i = 0; i < size; ++i)
        derivedElementlist.push_back(list.allocateAndConstruct<SimpleDerivedElement>());
    EXPECT_EQ(static_cast<size_t>(size), list.size());

    for (int i = 0; i < size; ++i)
        static_cast<SimpleDerivedElement*>(list.elementAt(i))->setValue(i);

    int i = 0;
    for (std::vector<SimpleDerivedElement*>::const_iterator derivedElementIter = derivedElementlist.begin(); derivedElementIter != derivedElementlist.end(); ++derivedElementIter, ++i)
        EXPECT_EQ(i, (*derivedElementIter)->getValue());
}

TEST(ListContainerTest, SimpleIterationAndReverseIterationWithIndexNonDerivedElement)
{
    ListContainer<NonDerivedElement> list;
    std::vector<NonDerivedElement*> nonDerivedElementlist;
    size_t size = 10;
    for (size_t i = 0; i < size; ++i)
        nonDerivedElementlist.push_back(list.allocateAndConstruct<NonDerivedElement>());
    EXPECT_EQ(size, list.size());

    size_t i = 0;
    for (ListContainer<NonDerivedElement>::Iterator iter = list.begin(); iter != list.end(); ++iter) {
        EXPECT_EQ(i, iter.index());
        ++i;
    }

    i = 0;
    for (ListContainer<NonDerivedElement>::ReverseIterator iter = list.rbegin(); iter != list.rend(); ++iter) {
        EXPECT_EQ(i, iter.index());
        ++i;
    }
}

// Increments an int when constructed (or the counter pointer is supplied) and
// decrements when destructed.
class InstanceCounter {
public:
    InstanceCounter() : m_counter(nullptr) {}
    explicit InstanceCounter(int* counter) { setCounter(counter); }
    ~InstanceCounter()
    {
        if (m_counter)
        --*m_counter;
    }
    void setCounter(int* counter)
    {
        m_counter = counter;
        ++*m_counter;
    }

private:
    int* m_counter;
};

TEST(ListContainerTest, RemoveLastDestruction)
{
    // We keep an explicit instance count to make sure that the destructors are
    // indeed getting called.
    int counter = 0;
    ListContainer<InstanceCounter> list(sizeof(InstanceCounter), 1);
    EXPECT_EQ(0, counter);
    EXPECT_EQ(0u, list.size());

    // We should be okay to add one and then go back to zero.
    list.allocateAndConstruct<InstanceCounter>()->setCounter(&counter);
    EXPECT_EQ(1, counter);
    EXPECT_EQ(1u, list.size());
    list.removeLast();
    EXPECT_EQ(0, counter);
    EXPECT_EQ(0u, list.size());

    // We should also be okay to remove the last multiple times, as long as there
    // are enough elements in the first place.
    list.allocateAndConstruct<InstanceCounter>()->setCounter(&counter);
    list.allocateAndConstruct<InstanceCounter>()->setCounter(&counter);
    list.allocateAndConstruct<InstanceCounter>()->setCounter(&counter);
    list.allocateAndConstruct<InstanceCounter>()->setCounter(&counter);
    list.allocateAndConstruct<InstanceCounter>()->setCounter(&counter);
    list.allocateAndConstruct<InstanceCounter>()->setCounter(&counter);
    list.removeLast();
    list.removeLast();
    EXPECT_EQ(4, counter); // Leaves one in the last list.
    EXPECT_EQ(4u, list.size());
    list.removeLast();
    EXPECT_EQ(3, counter); // Removes an inner list from before.
    EXPECT_EQ(3u, list.size());
}

// TODO(jbroman): std::equal would work if ListContainer iterators satisfied the
// usual STL iterator constraints. We should fix that.
template <typename It1, typename It2>
bool Equal(It1 it1, const It1& end1, It2 it2)
{
    for (; it1 != end1; ++it1, ++it2) {
        if (!(*it1 == *it2))
            return false;
    }
    return true;
}

TEST(ListContainerTest, RemoveLastIteration)
{
    struct SmallStruct {
        char dummy[16];
    };
    ListContainer<SmallStruct> list(sizeof(SmallStruct), 1);
    std::vector<SmallStruct*> pointers;

    // Utilities which keep these two lists in sync and check that their iteration
    // order matches.
    auto push = [&list, &pointers]()
    {
        pointers.push_back(list.allocateAndConstruct<SmallStruct>());
    };

    auto pop = [&list, &pointers]()
    {
        pointers.pop_back();
        list.removeLast();
    };

    auto check_equal = [&list, &pointers]()
    {
        // They should be of the same size, and compare equal with all four kinds of
        // iteration.
        // Apparently Mac doesn't have vector::cbegin and vector::crbegin?
        const auto& constPointers = pointers;
        ASSERT_EQ(list.size(), pointers.size());
        ASSERT_TRUE(Equal(list.begin(), list.end(), pointers.begin()));
        ASSERT_TRUE(Equal(list.cbegin(), list.cend(), constPointers.begin()));
        ASSERT_TRUE(Equal(list.rbegin(), list.rend(), pointers.rbegin()));
        ASSERT_TRUE(Equal(list.crbegin(), list.crend(), constPointers.rbegin()));
    };

    check_equal(); // Initially empty.
    push();
    check_equal(); // One full inner list.
    push();
    check_equal(); // One full, one partially full.
    push();
    push();
    check_equal(); // Two full, one partially full.
    pop();
    check_equal(); // Two full, one empty.
    pop();
    check_equal(); // One full, one partially full, one empty.
    pop();
    check_equal(); // One full, one empty.
    push();
    pop();
    pop();
    ASSERT_TRUE(list.empty());
    check_equal(); // Empty.
}

TEST(ListContainerTest, AppendByMovingSameList)
{
    ListContainer<SimpleDerivedElement> list(kCurrentLargestDerivedElementSize);
    list.allocateAndConstruct<SimpleDerivedElementConstructMagicNumberOne>();

    list.appendByMoving(list.front());
    EXPECT_EQ(kMagicNumberToUseForSimpleDerivedElementOne, list.back()->getValue());
    EXPECT_EQ(2u, list.size());

    list.front()->setValue(kMagicNumberToUseForSimpleDerivedElementTwo);
    EXPECT_EQ(kMagicNumberToUseForSimpleDerivedElementTwo, list.front()->getValue());
    list.appendByMoving(list.front());
    EXPECT_EQ(kMagicNumberToUseForSimpleDerivedElementTwo, list.back()->getValue());
    EXPECT_EQ(3u, list.size());
}

TEST(ListContainerTest, AppendByMovingDoesNotDestruct)
{
    ListContainer<DerivedElement> list1(kCurrentLargestDerivedElementSize);
    ListContainer<DerivedElement> list2(kCurrentLargestDerivedElementSize);
    MockDerivedElement* mde1 = list1.allocateAndConstruct<MockDerivedElement>();

    // Make sure destructor isn't called during AppendByMoving.
    list2.appendByMoving(mde1);
    EXPECT_CALL(*mde1, Destruct()).Times(0);
    testing::Mock::VerifyAndClearExpectations(mde1);
    mde1 = static_cast<MockDerivedElement*>(list2.back());
    EXPECT_CALL(*mde1, Destruct());
}

TEST(ListContainerTest, AppendByMovingReturnsMovedPointer)
{
    ListContainer<SimpleDerivedElement> list1(kCurrentLargestDerivedElementSize);
    ListContainer<SimpleDerivedElement> list2(kCurrentLargestDerivedElementSize);
    SimpleDerivedElement* simpleElement = list1.allocateAndConstruct<SimpleDerivedElement>();

    SimpleDerivedElement* movedElement1 = list2.appendByMoving(simpleElement);
    EXPECT_EQ(list2.back(), movedElement1);

    SimpleDerivedElement* movedElement2 = list1.appendByMoving(movedElement1);
    EXPECT_EQ(list1.back(), movedElement2);
    EXPECT_NE(movedElement1, movedElement2);
}

TEST(ListContainerTest, AppendByMovingReplacesSourceWithNewDerivedElement)
{
    ListContainer<SimpleDerivedElementConstructMagicNumberOne> list1(kCurrentLargestDerivedElementSize);
    ListContainer<SimpleDerivedElementConstructMagicNumberTwo> list2(kCurrentLargestDerivedElementSize);

    list1.allocateAndConstruct<SimpleDerivedElementConstructMagicNumberOne>();
    EXPECT_EQ(kMagicNumberToUseForSimpleDerivedElementOne,
    list1.front()->getValue());

    list2.appendByMoving(list1.front());
    EXPECT_EQ(kMagicNumberToUseForSimpleDerivedElementOne,
    list1.front()->getValue());
    EXPECT_EQ(kMagicNumberToUseForSimpleDerivedElementOne,
    list2.front()->getValue());

    // Change the value of list2.front() to ensure the value is actually moved.
    list2.back()->setValue(kMagicNumberToUseForSimpleDerivedElementThree);

    list1.appendByMoving(list2.back());
    EXPECT_EQ(kMagicNumberToUseForSimpleDerivedElementOne,
    list1.front()->getValue());
    EXPECT_EQ(kMagicNumberToUseForSimpleDerivedElementThree,
    list1.back()->getValue());
    EXPECT_EQ(kMagicNumberToUseForSimpleDerivedElementTwo,
    list2.back()->getValue());

    // AppendByMoving replaces the source element with a new derived element so
    // we do not expect sizes to shrink after AppendByMoving is called.
    EXPECT_EQ(2u, list1.size()); // One direct allocation and one AppendByMoving.
    EXPECT_EQ(1u, list2.size()); // One AppendByMoving.
}

const size_t kLongCountForLongSimpleDerivedElement = 5;

class LongSimpleDerivedElement : public SimpleDerivedElement {
public:
    ~LongSimpleDerivedElement() override {}
    void setAllValues(unsigned long value)
    {
        for (size_t i = 0; i < kLongCountForLongSimpleDerivedElement; i++)
        values[i] = value;
    }
    bool areAllValuesEqualTo(size_t value) const
    {
        for (size_t i = 1; i < kLongCountForLongSimpleDerivedElement; i++) {
            if (values[i] != values[0])
                return false;
        }
        return true;
    }

private:
    unsigned long values[kLongCountForLongSimpleDerivedElement];
};

const unsigned long kMagicNumberToUseForLongSimpleDerivedElement = 2718ul;

class LongSimpleDerivedElementConstructMagicNumber : public LongSimpleDerivedElement {
public:
    LongSimpleDerivedElementConstructMagicNumber()
    {
        setAllValues(kMagicNumberToUseForLongSimpleDerivedElement);
    }
};

TEST(ListContainerTest, AppendByMovingLongAndSimpleDerivedElements)
{
    static_assert(sizeof(LongSimpleDerivedElement) > sizeof(SimpleDerivedElement),
        "LongSimpleDerivedElement should be larger than "
        "SimpleDerivedElement's size.");
    static_assert(sizeof(LongSimpleDerivedElement) <= kLargestDerivedElementSize,
        "LongSimpleDerivedElement should be smaller than the maximum "
        "DerivedElement size.");

    ListContainer<SimpleDerivedElement> list(kCurrentLargestDerivedElementSize);
    list.allocateAndConstruct<LongSimpleDerivedElementConstructMagicNumber>();
    list.allocateAndConstruct<SimpleDerivedElementConstructMagicNumberOne>();

    EXPECT_TRUE(static_cast<LongSimpleDerivedElement*>(list.front())->areAllValuesEqualTo(kMagicNumberToUseForLongSimpleDerivedElement));
    EXPECT_EQ(kMagicNumberToUseForSimpleDerivedElementOne, list.back()->getValue());

    // Test that moving a simple derived element actually moves enough data so
    // that the LongSimpleDerivedElement at this location is entirely moved.
    SimpleDerivedElement* simpleElement = list.back();
    list.appendByMoving(list.front());
    EXPECT_TRUE(static_cast<LongSimpleDerivedElement*>(list.back())->areAllValuesEqualTo(kMagicNumberToUseForLongSimpleDerivedElement));
    EXPECT_EQ(kMagicNumberToUseForSimpleDerivedElementOne, simpleElement->getValue());

    LongSimpleDerivedElement* longElement = static_cast<LongSimpleDerivedElement*>(list.back());
    list.appendByMoving(simpleElement);
    EXPECT_TRUE(longElement->areAllValuesEqualTo(kMagicNumberToUseForLongSimpleDerivedElement));
    EXPECT_EQ(kMagicNumberToUseForSimpleDerivedElementOne, list.back()->getValue());
}

TEST(ListContainerTest, Swap)
{
    ListContainer<SimpleDerivedElement> list1(kCurrentLargestDerivedElementSize);
    list1.allocateAndConstruct<SimpleDerivedElementConstructMagicNumberOne>();
    ListContainer<SimpleDerivedElement> list2(kCurrentLargestDerivedElementSize);
    list2.allocateAndConstruct<SimpleDerivedElementConstructMagicNumberTwo>();
    list2.allocateAndConstruct<SimpleDerivedElementConstructMagicNumberThree>();

    SimpleDerivedElement* preSwapList1Front = list1.front();

    EXPECT_EQ(kMagicNumberToUseForSimpleDerivedElementOne, list1.front()->getValue());
    EXPECT_EQ(1u, list1.size());
    EXPECT_EQ(kMagicNumberToUseForSimpleDerivedElementTwo, list2.front()->getValue());
    EXPECT_EQ(kMagicNumberToUseForSimpleDerivedElementThree, list2.back()->getValue());
    EXPECT_EQ(2u, list2.size());

    list2.swap(list1);

    EXPECT_EQ(kMagicNumberToUseForSimpleDerivedElementTwo, list1.front()->getValue());
    EXPECT_EQ(kMagicNumberToUseForSimpleDerivedElementThree, list1.back()->getValue());
    EXPECT_EQ(2u, list1.size());
    EXPECT_EQ(kMagicNumberToUseForSimpleDerivedElementOne, list2.front()->getValue());
    EXPECT_EQ(1u, list2.size());

    // Ensure pointers are still valid after swapping.
    EXPECT_EQ(preSwapList1Front, list2.front());
}

TEST(ListContainerTest, GetCapacityInBytes)
{
    const int iterations = 500;
    const size_t initialCapacity = 10;
    const size_t upperBoundOnMinCapacity = initialCapacity;

    // At time of writing, removing elements from the end can cause up to 7x the
    // memory required to be consumed, in the worst case, since we can have up to
    // two trailing inner lists that are empty (for 2*size + 4*size in unused
    // memory, due to the exponential growth strategy).
    const size_t maxWasteFactor = 8;

    ListContainer<DerivedElement> list(largestDerivedElementSize(), initialCapacity);

    // The capacity should grow with the list.
    for (int i = 0; i < iterations; i++) {
        size_t capacity = list.getCapacityInBytes();
        ASSERT_GE(capacity, list.size() * largestDerivedElementSize());
        ASSERT_LE(capacity,
            std::max(list.size(),
                upperBoundOnMinCapacity) * maxWasteFactor * largestDerivedElementSize());
        list.allocateAndConstruct<DerivedElement1>();
    }

    // The capacity should shrink with the list.
    for (int i = 0; i < iterations; i++) {
        size_t capacity = list.getCapacityInBytes();
        ASSERT_GE(capacity, list.size() * largestDerivedElementSize());
        ASSERT_LE(capacity,
            std::max(list.size(),
                upperBoundOnMinCapacity) * maxWasteFactor * largestDerivedElementSize());
        list.removeLast();
    }
}

} // namespace
} // namespace blink
