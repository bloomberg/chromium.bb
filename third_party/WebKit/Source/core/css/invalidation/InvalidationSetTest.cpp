// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/invalidation/InvalidationSet.h"

#include <gtest/gtest.h>

namespace blink {

// Once we setWholeSubtreeInvalid, we should not keep the HashSets.
TEST(InvalidationSetTest, SubtreeInvalid_AddBefore)
{
    RefPtr<InvalidationSet> set = InvalidationSet::create();
    set->addClass("a");
    set->setWholeSubtreeInvalid();

    ASSERT_TRUE(set->isEmpty());
}

// Don't (re)create HashSets if we've already setWholeSubtreeInvalid.
TEST(InvalidationSetTest, SubtreeInvalid_AddAfter)
{
    RefPtr<InvalidationSet> set = InvalidationSet::create();
    set->setWholeSubtreeInvalid();
    set->addTagName("a");

    ASSERT_TRUE(set->isEmpty());
}

// No need to keep the HashSets when combining with a wholeSubtreeInvalid set.
TEST(InvalidationSetTest, SubtreeInvalid_Combine_1)
{
    RefPtr<InvalidationSet> set1 = InvalidationSet::create();
    RefPtr<InvalidationSet> set2 = InvalidationSet::create();

    set1->addId("a");
    set2->setWholeSubtreeInvalid();

    set1->combine(*set2);

    ASSERT_TRUE(set1->wholeSubtreeInvalid());
    ASSERT_TRUE(set1->isEmpty());
}

// No need to add HashSets from combining set when we already have wholeSubtreeInvalid.
TEST(InvalidationSetTest, SubtreeInvalid_Combine_2)
{
    RefPtr<InvalidationSet> set1 = InvalidationSet::create();
    RefPtr<InvalidationSet> set2 = InvalidationSet::create();

    set1->setWholeSubtreeInvalid();
    set2->addAttribute("a");

    set1->combine(*set2);

    ASSERT_TRUE(set1->wholeSubtreeInvalid());
    ASSERT_TRUE(set1->isEmpty());
}

TEST(InvalidationSetTest, SubtreeInvalid_AddCustomPseudoBefore)
{
    RefPtr<InvalidationSet> set = InvalidationSet::create();
    set->setCustomPseudoInvalid();
    ASSERT_FALSE(set->isEmpty());

    set->setWholeSubtreeInvalid();
    ASSERT_TRUE(set->isEmpty());
}

#ifndef NDEBUG
TEST(InvalidationSetTest, ShowDebug)
{
    RefPtr<InvalidationSet> set = InvalidationSet::create();
    set->show();
}
#endif // NDEBUG

} // namespace blink
