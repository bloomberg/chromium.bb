// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementDescriptor.h"

#include "core/dom/custom/CustomElementDescriptorHash.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/HashSet.h"
#include "wtf/text/AtomicString.h"

namespace blink {

TEST(CustomElementDescriptorTest, equal)
{
    CustomElementDescriptor myTypeExtension("my-button", "button");
    CustomElementDescriptor again("my-button", "button");
    EXPECT_TRUE(myTypeExtension == again)
        << "two descriptors with the same name and local name should be equal";
}

TEST(CustomElementDescriptorTest, notEqual)
{
    CustomElementDescriptor myTypeExtension("my-button", "button");
    CustomElementDescriptor collidingNewType("my-button", "my-button");
    EXPECT_FALSE(myTypeExtension == collidingNewType)
        << "type extension should not be equal to a non-type extension";
}

TEST(CustomElementDescriptorTest, hashable)
{
    HashSet<CustomElementDescriptor> descriptors;
    descriptors.add(CustomElementDescriptor("foo-bar", "foo-bar"));
    EXPECT_TRUE(
        descriptors.contains(CustomElementDescriptor("foo-bar", "foo-bar")))
        << "the identical descriptor should be found in the hash set";
    EXPECT_FALSE(descriptors.contains(CustomElementDescriptor(
        "bad-poetry",
        "blockquote")))
        << "an unrelated descriptor should not be found in the hash set";
}

} // namespace blink
