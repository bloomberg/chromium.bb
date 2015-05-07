// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/change_record.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "sync/protocol/extension_specifics.pb.h"
#include "sync/protocol/proto_value_conversions.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using base::ExpectDictDictionaryValue;
using base::ExpectDictStringValue;
using testing::Invoke;
using testing::StrictMock;

class ChangeRecordTest : public testing::Test {};

void ExpectChangeRecordActionValue(ChangeRecord::Action expected_value,
                                   const base::DictionaryValue& value,
                                   const std::string& key) {
  std::string str_value;
  EXPECT_TRUE(value.GetString(key, &str_value));
  switch (expected_value) {
    case ChangeRecord::ACTION_ADD:
      EXPECT_EQ("Add", str_value);
      break;
    case ChangeRecord::ACTION_UPDATE:
      EXPECT_EQ("Update", str_value);
      break;
    case ChangeRecord::ACTION_DELETE:
      EXPECT_EQ("Delete", str_value);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void CheckChangeRecordValue(
    const ChangeRecord& record,
    const base::DictionaryValue& value) {
  ExpectChangeRecordActionValue(record.action, value, "action");
  ExpectDictStringValue(base::Int64ToString(record.id), value, "id");
  if (record.action == ChangeRecord::ACTION_DELETE) {
    scoped_ptr<base::DictionaryValue> expected_extra_value;
    if (record.extra.get()) {
      expected_extra_value = record.extra->ToValue();
    }
    const base::Value* extra_value = NULL;
    EXPECT_EQ(record.extra.get() != NULL,
              value.Get("extra", &extra_value));
    EXPECT_TRUE(base::Value::Equals(extra_value, expected_extra_value.get()));

    scoped_ptr<base::DictionaryValue> expected_specifics_value(
        EntitySpecificsToValue(record.specifics));
    ExpectDictDictionaryValue(*expected_specifics_value,
                              value, "specifics");
  }
}

class TestExtraChangeRecordData : public ExtraPasswordChangeRecordData {
 public:
  TestExtraChangeRecordData()
      : to_value_calls_(0), expected_to_value_calls_(0) {}

  ~TestExtraChangeRecordData() override {
    EXPECT_EQ(expected_to_value_calls_, to_value_calls_);
  }

  scoped_ptr<base::DictionaryValue> ToValue() const override {
    const_cast<TestExtraChangeRecordData*>(this)->to_value_calls_++;
    return dict_->CreateDeepCopy();
  }

  void set_dictionary_value(scoped_ptr<base::DictionaryValue> dict) {
    dict_ = dict.Pass();
  }

  void set_expected_to_value_calls(size_t expectation) {
    expected_to_value_calls_ = expectation;
  }

 private:
  scoped_ptr<base::DictionaryValue> dict_;
  size_t to_value_calls_;
  size_t expected_to_value_calls_;
};

TEST_F(ChangeRecordTest, ChangeRecordToValue) {
  sync_pb::EntitySpecifics old_specifics;
  old_specifics.mutable_extension()->set_id("old");
  sync_pb::EntitySpecifics new_specifics;
  old_specifics.mutable_extension()->set_id("new");

  const int64 kTestId = 5;

  // Add
  {
    ChangeRecord record;
    record.action = ChangeRecord::ACTION_ADD;
    record.id = kTestId;
    record.specifics = old_specifics;
    record.extra.reset(new TestExtraChangeRecordData());
    scoped_ptr<base::DictionaryValue> value(record.ToValue());
    CheckChangeRecordValue(record, *value);
  }

  // Update
  {
    ChangeRecord record;
    record.action = ChangeRecord::ACTION_UPDATE;
    record.id = kTestId;
    record.specifics = old_specifics;
    record.extra.reset(new TestExtraChangeRecordData());
    scoped_ptr<base::DictionaryValue> value(record.ToValue());
    CheckChangeRecordValue(record, *value);
  }

  // Delete (no extra)
  {
    ChangeRecord record;
    record.action = ChangeRecord::ACTION_DELETE;
    record.id = kTestId;
    record.specifics = old_specifics;
    scoped_ptr<base::DictionaryValue> value(record.ToValue());
    CheckChangeRecordValue(record, *value);
  }

  // Delete (with extra)
  {
    ChangeRecord record;
    record.action = ChangeRecord::ACTION_DELETE;
    record.id = kTestId;
    record.specifics = old_specifics;

    scoped_ptr<base::DictionaryValue> extra_value(new base::DictionaryValue());
    extra_value->SetString("foo", "bar");
    scoped_ptr<TestExtraChangeRecordData> extra(
        new TestExtraChangeRecordData());
    extra->set_dictionary_value(extra_value.Pass());
    extra->set_expected_to_value_calls(2U);

    record.extra.reset(extra.release());
    scoped_ptr<base::DictionaryValue> value(record.ToValue());
    CheckChangeRecordValue(record, *value);
  }
}

}  // namespace
}  // namespace syncer
