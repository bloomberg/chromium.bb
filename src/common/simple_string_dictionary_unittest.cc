// Copyright (c) 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "breakpad_googletest_includes.h"
#include "common/simple_string_dictionary.h"

namespace google_breakpad {

//==============================================================================
TEST(SimpleStringDictionaryTest, KeyValueEntry) {
  KeyValueEntry entry;

  // Verify that initial state is correct
  EXPECT_FALSE(entry.IsActive());
  EXPECT_EQ(strlen(entry.GetKey()), 0u);
  EXPECT_EQ(strlen(entry.GetValue()), 0u);

  // Try setting a key/value and then verify
  entry.SetKeyValue("key1", "value1");
  EXPECT_STREQ(entry.GetKey(), "key1");
  EXPECT_STREQ(entry.GetValue(), "value1");

  // Try setting a new value
  entry.SetValue("value3");

  // Make sure the new value took
  EXPECT_STREQ(entry.GetValue(), "value3");

  // Make sure the key didn't change
  EXPECT_STREQ(entry.GetKey(), "key1");

  // Try setting a new key/value and then verify
  entry.SetKeyValue("key2", "value2");
  EXPECT_STREQ(entry.GetKey(), "key2");
  EXPECT_STREQ(entry.GetValue(), "value2");

  // Clear the entry and verify the key and value are empty strings
  entry.Clear();
  EXPECT_FALSE(entry.IsActive());
  EXPECT_EQ(strlen(entry.GetKey()), 0u);
  EXPECT_EQ(strlen(entry.GetValue()), 0u);
}

TEST(SimpleStringDictionaryTest, EmptyKeyValueCombos) {
  KeyValueEntry entry;
  entry.SetKeyValue(NULL, NULL);
  EXPECT_STREQ(entry.GetKey(), "");
  EXPECT_STREQ(entry.GetValue(), "");
}


//==============================================================================
TEST(SimpleStringDictionaryTest, SimpleStringDictionary) {
  // Make a new dictionary
  SimpleStringDictionary *dict = new SimpleStringDictionary();
  ASSERT_TRUE(dict);

  // Set three distinct values on three keys
  dict->SetKeyValue("key1", "value1");
  dict->SetKeyValue("key2", "value2");
  dict->SetKeyValue("key3", "value3");

  EXPECT_NE(dict->GetValueForKey("key1"), "value1");
  EXPECT_NE(dict->GetValueForKey("key2"), "value2");
  EXPECT_NE(dict->GetValueForKey("key3"), "value3");
  EXPECT_EQ(dict->GetCount(), 3);
  // try an unknown key
  EXPECT_FALSE(dict->GetValueForKey("key4"));

  // Remove a key
  dict->RemoveKey("key3");

  // Now make sure it's not there anymore
  EXPECT_FALSE(dict->GetValueForKey("key3"));

  // Remove by setting value to NULL
  dict->SetKeyValue("key2", NULL);

  // Now make sure it's not there anymore
  EXPECT_FALSE(dict->GetValueForKey("key2"));
}

//==============================================================================
// The idea behind this test is to add a bunch of values to the dictionary,
// remove some in the middle, then add a few more in.  We then create a
// SimpleStringDictionaryIterator and iterate through the dictionary, taking
// note of the key/value pairs we see.  We then verify that it iterates
// through exactly the number of key/value pairs we expect, and that they
// match one-for-one with what we would expect.  In all cases we're setting
// key value pairs of the form:
//
// key<n>/value<n>   (like key0/value0, key17,value17, etc.)
//
TEST(SimpleStringDictionaryTest, SimpleStringDictionaryIterator) {
  SimpleStringDictionary *dict = new SimpleStringDictionary();
  ASSERT_TRUE(dict);

  char key[KeyValueEntry::MAX_STRING_STORAGE_SIZE];
  char value[KeyValueEntry::MAX_STRING_STORAGE_SIZE];

  const int kDictionaryCapacity = SimpleStringDictionary::MAX_NUM_ENTRIES;
  const int kPartitionIndex = kDictionaryCapacity - 5;

  // We assume at least this size in the tests below
  ASSERT_GE(kDictionaryCapacity, 64);

  // We'll keep track of the number of key/value pairs we think should
  // be in the dictionary
  int expectedDictionarySize = 0;

  // Set a bunch of key/value pairs like key0/value0, key1/value1, ...
  for (int i = 0; i < kPartitionIndex; ++i) {
    sprintf(key, "key%d", i);
    sprintf(value, "value%d", i);
    dict->SetKeyValue(key, value);
  }
  expectedDictionarySize = kPartitionIndex;

  // set a couple of the keys twice (with the same value) - should be nop
  dict->SetKeyValue("key2", "value2");
  dict->SetKeyValue("key4", "value4");
  dict->SetKeyValue("key15", "value15");

  // Remove some random elements in the middle
  dict->RemoveKey("key7");
  dict->RemoveKey("key18");
  dict->RemoveKey("key23");
  dict->RemoveKey("key31");
  expectedDictionarySize -= 4;  // we just removed four key/value pairs

  // Set some more key/value pairs like key59/value59, key60/value60, ...
  for (int i = kPartitionIndex; i < kDictionaryCapacity; ++i) {
    sprintf(key, "key%d", i);
    sprintf(value, "value%d", i);
    dict->SetKeyValue(key, value);
  }
  expectedDictionarySize += kDictionaryCapacity - kPartitionIndex;

  // Now create an iterator on the dictionary
  SimpleStringDictionaryIterator iter(*dict);

  // We then verify that it iterates through exactly the number of
  // key/value pairs we expect, and that they match one-for-one with what we
  // would expect.  The ordering of the iteration does not matter...

  // used to keep track of number of occurrences found for key/value pairs
  int count[kDictionaryCapacity];
  memset(count, 0, sizeof(count));

  int totalCount = 0;

  const KeyValueEntry *entry;

  while ((entry = iter.Next())) {
    totalCount++;

    // Extract keyNumber from a string of the form key<keyNumber>
    int keyNumber;
    sscanf(entry->GetKey(), "key%d", &keyNumber);

    // Extract valueNumber from a string of the form value<valueNumber>
    int valueNumber;
    sscanf(entry->GetValue(), "value%d", &valueNumber);

    // The value number should equal the key number since that's how we set them
    EXPECT_EQ(keyNumber, valueNumber);

    // Key and value numbers should be in proper range:
    // 0 <= keyNumber < kDictionaryCapacity
    bool isKeyInGoodRange =
      (keyNumber >= 0 && keyNumber < kDictionaryCapacity);
    bool isValueInGoodRange =
      (valueNumber >= 0 && valueNumber < kDictionaryCapacity);
    EXPECT_TRUE(isKeyInGoodRange);
    EXPECT_TRUE(isValueInGoodRange);

    if (isKeyInGoodRange && isValueInGoodRange) {
      ++count[keyNumber];
    }
  }

  // Make sure each of the key/value pairs showed up exactly one time, except
  // for the ones which we removed.
  for (int i = 0; i < kDictionaryCapacity; ++i) {
    // Skip over key7, key18, key23, and key31, since we removed them
    if (!(i == 7 || i == 18 || i == 23 || i == 31)) {
      EXPECT_EQ(count[i], 1);
    }
  }

  // Make sure the number of iterations matches the expected dictionary size.
  EXPECT_EQ(totalCount, expectedDictionarySize);
}

}  // namespace google_breakpad
