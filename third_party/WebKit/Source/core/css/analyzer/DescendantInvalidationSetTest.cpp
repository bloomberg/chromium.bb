// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/analyzer/DescendantInvalidationSet.h"

#include <gtest/gtest.h>

using namespace WebCore;

namespace {

// Once we setWholeSubtreeInvalid, we should not keep the HashSets.
TEST(DescendantInvalidationSetTest, SubtreeInvalid_AddBefore)
{
    RefPtr<DescendantInvalidationSet> set = DescendantInvalidationSet::create();
    set->addClass("a");
    set->setWholeSubtreeInvalid();

    Vector<AtomicString> classes;
    set->getClasses(classes);

    ASSERT_TRUE(classes.isEmpty());
}

// Don't (re)create HashSets if we've already setWholeSubtreeInvalid.
TEST(DescendantInvalidationSetTest, SubtreeInvalid_AddAfter)
{
    RefPtr<DescendantInvalidationSet> set = DescendantInvalidationSet::create();
    set->setWholeSubtreeInvalid();
    set->addClass("a");

    Vector<AtomicString> classes;
    set->getClasses(classes);

    ASSERT_TRUE(classes.isEmpty());
}

// No need to keep the HashSets when combining with a wholeSubtreeInvalid set.
TEST(DescendantInvalidationSetTest, SubtreeInvalid_Combine_1)
{
    RefPtr<DescendantInvalidationSet> set1 = DescendantInvalidationSet::create();
    RefPtr<DescendantInvalidationSet> set2 = DescendantInvalidationSet::create();

    set1->addClass("a");
    set2->setWholeSubtreeInvalid();

    set1->combine(*set2);

    Vector<AtomicString> classes;
    set1->getClasses(classes);

    ASSERT_TRUE(set1->wholeSubtreeInvalid());
    ASSERT_TRUE(classes.isEmpty());
}

// No need to add HashSets from combining set when we already have wholeSubtreeInvalid.
TEST(DescendantInvalidationSetTest, SubtreeInvalid_Combine_2)
{
    RefPtr<DescendantInvalidationSet> set1 = DescendantInvalidationSet::create();
    RefPtr<DescendantInvalidationSet> set2 = DescendantInvalidationSet::create();

    set1->setWholeSubtreeInvalid();
    set2->addClass("a");

    set1->combine(*set2);

    Vector<AtomicString> classes;
    set1->getClasses(classes);

    ASSERT_TRUE(set1->wholeSubtreeInvalid());
    ASSERT_TRUE(classes.isEmpty());
}

} // namespace
