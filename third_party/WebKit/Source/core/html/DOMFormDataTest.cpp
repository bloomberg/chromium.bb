// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/html/DOMFormData.h"

#include "core/html/FormDataList.h"
#include <gtest/gtest.h>

namespace blink {

TEST(DOMFormDataTest, opacityGet)
{
    DOMFormData* fd = DOMFormData::create(UTF8Encoding());
    fd->append("name1", "value1");

    FileOrUSVString result;
    fd->get("name1", result);
    EXPECT_TRUE(result.isUSVString());
    EXPECT_EQ("value1", result.getAsUSVString());

    FormDataList::Entry entry = fd->getEntry("name1");
    EXPECT_EQ("value1", entry.string());

    fd->makeOpaque();

    // Web-exposed interface should be opaque.
    FileOrUSVString opaqueResult;
    fd->get("name1", opaqueResult);
    EXPECT_TRUE(opaqueResult.isNull());

    // Internal interface should be uneffected.
    FormDataList::Entry opaqueEntry = fd->getEntry("name1");
    EXPECT_EQ("value1", opaqueEntry.string());
}

TEST(DOMFormDataTest, opacityGetAll)
{
    DOMFormData* fd = DOMFormData::create(UTF8Encoding());
    fd->append("name1", "value1");

    HeapVector<FormDataEntryValue> results = fd->getAll("name1");
    EXPECT_EQ(1u, results.size());
    EXPECT_TRUE(results[0].isUSVString());
    EXPECT_EQ("value1", results[0].getAsUSVString());

    EXPECT_EQ(1u, fd->size());

    fd->makeOpaque();

    // Web-exposed interface should be opaque.
    results = fd->getAll("name1");
    EXPECT_EQ(0u, results.size());

    // Internal interface should be uneffected.
    EXPECT_EQ(1u, fd->size());
}

TEST(DOMFormDataTest, opacityHas)
{
    DOMFormData* fd = DOMFormData::create(UTF8Encoding());
    fd->append("name1", "value1");

    EXPECT_TRUE(fd->has("name1"));
    EXPECT_TRUE(fd->hasEntry("name1"));

    fd->makeOpaque();

    // Web-exposed interface should be opaque.
    EXPECT_FALSE(fd->has("name1"));

    // Internal interface should be uneffected.
    EXPECT_TRUE(fd->hasEntry("name1"));
}

} // namespace blink
