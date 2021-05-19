//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// JsonSerializer_unittests-cpp: Unit tests for the JSON based serializer
//

#if defined(ANGLE_HAVE_RAPIDJSON)
#    include "JsonSerializer.h"

#    include <gtest/gtest.h>

class JsonSerializerTest : public ::testing::Test
{
  protected:
    void SetUp() override;
    void check(const std::string &expect);

    angle::JsonSerializer js;
};

// Test writing one integer value
TEST_F(JsonSerializerTest, NamedIntValue1)
{
    js.addScalar("test1", 1);

    const std::string expect =
        R"({
    "context": {
        "test1": 1
    }
})";
    check(expect);
}

// Test writing one long value
TEST_F(JsonSerializerTest, NamedLongValue)
{
    long v = -12;
    js.addScalar("test1", v);

    const std::string expect =
        R"({
    "context": {
        "test1": -12
    }
})";
    check(expect);
}

// Test writing one unsigned long value
TEST_F(JsonSerializerTest, NamedULongValue)
{
    unsigned long v = 12;
    js.addScalar("test1", v);

    const std::string expect =
        R"({
    "context": {
        "test1": 12
    }
})";
    check(expect);
}

// Test writing another integer value
TEST_F(JsonSerializerTest, NamedIntValue2)
{
    js.addScalar("test2", 2);

    const std::string expect =
        R"({
    "context": {
        "test2": 2
    }
})";

    check(expect);
}

// Test writing one string value
TEST_F(JsonSerializerTest, NamedStringValue)
{
    js.addCString("test2", "value");

    const std::string expect =
        R"({
    "context": {
        "test2": "value"
    }
})";

    check(expect);
}

// Test writing one byte array
// Since he serialiter is only used for testing we don't store
// the whole byte array, but only it's SHA1 checksum
TEST_F(JsonSerializerTest, ByteArrayValue)
{
    const uint8_t value[5] = {10, 0, 0xcc, 0xff, 0xaa};
    js.addBlob("test2", value, 5);

    const std::string expect =
        R"({
    "context": {
        "test2": "SHA1:4315724B1AB1EB2C0128E8E9DAD6D76254BA711D"
    }
})";

    check(expect);
}

// Test writing one vector of integer values
TEST_F(JsonSerializerTest, IntVectorValue)
{
    std::vector<int> v = {0, 1, -1};

    js.addVector("test2", v);

    const std::string expect =
        R"({
    "context": {
        "test2": [
            0,
            1,
            -1
        ]
    }
})";

    check(expect);
}

// Test writing boolean values
TEST_F(JsonSerializerTest, NamedBoolValues)
{
    js.addScalar("test_false", false);
    js.addScalar("test_true", true);

    const std::string expect =
        R"({
    "context": {
        "test_false": false,
        "test_true": true
    }
})";

    check(expect);
}

// test writing two values in a sub-group
TEST_F(JsonSerializerTest, GroupedIntValue)
{
    js.startGroup("group");
    js.addScalar("test1", 1);
    js.addScalar("test2", 2);
    js.endGroup();

    const std::string expect =
        R"({
    "context": {
        "group": {
            "test1": 1,
            "test2": 2
        }
    }
})";

    check(expect);
}

void JsonSerializerTest::SetUp()
{
    js.startDocument("context");
}

void JsonSerializerTest::check(const std::string &expect)
{
    js.endDocument();
    EXPECT_EQ(js.data(), expect);
    EXPECT_EQ(js.length(), expect.length());
    std::vector<uint8_t> expect_as_ubyte(expect.begin(), expect.end());
    EXPECT_EQ(js.getData(), expect_as_ubyte);
}

#endif
