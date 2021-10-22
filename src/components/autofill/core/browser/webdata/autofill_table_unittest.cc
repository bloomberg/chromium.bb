// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_table.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/cxx17_backports.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_metadata.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using sync_pb::EntityMetadata;
using sync_pb::ModelTypeState;
using syncer::EntityMetadataMap;
using syncer::MetadataBatch;
using testing::ElementsAre;
using testing::UnorderedElementsAre;

namespace autofill {

using structured_address::VerificationStatus;

// So we can compare AutofillKeys with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const AutofillKey& key) {
  return os << base::UTF16ToASCII(key.name()) << ", "
            << base::UTF16ToASCII(key.value());
}

// So we can compare AutofillChanges with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const AutofillChange& change) {
  switch (change.type()) {
    case AutofillChange::ADD: {
      os << "ADD";
      break;
    }
    case AutofillChange::UPDATE: {
      os << "UPDATE";
      break;
    }
    case AutofillChange::REMOVE: {
      os << "REMOVE";
      break;
    }
    case AutofillChange::EXPIRE: {
      os << "EXPIRED";
      break;
    }
  }
  return os << " " << change.key();
}

namespace {

typedef std::set<AutofillEntry,
                 bool (*)(const AutofillEntry&, const AutofillEntry&)>
    AutofillEntrySet;
typedef AutofillEntrySet::iterator AutofillEntrySetIterator;

bool CompareAutofillEntries(const AutofillEntry& a, const AutofillEntry& b) {
  return std::tie(a.key().name(), a.key().value(), a.date_created(),
                  a.date_last_used()) <
         std::tie(b.key().name(), b.key().value(), b.date_created(),
                  b.date_last_used());
}

AutofillEntry MakeAutofillEntry(const std::u16string& name,
                                const std::u16string& value,
                                time_t date_created,
                                time_t date_last_used) {
  if (date_last_used < 0)
    date_last_used = date_created;
  return AutofillEntry(AutofillKey(name, value), Time::FromTimeT(date_created),
                       Time::FromTimeT(date_last_used));
}

// Checks |actual| and |expected| contain the same elements.
void CompareAutofillEntrySets(const AutofillEntrySet& actual,
                              const AutofillEntrySet& expected) {
  ASSERT_EQ(expected.size(), actual.size());
  size_t count = 0;
  for (auto it = actual.begin(); it != actual.end(); ++it) {
    count += expected.count(*it);
  }
  EXPECT_EQ(actual.size(), count);
}

int GetAutofillEntryCount(const std::u16string& name,
                          const std::u16string& value,
                          WebDatabase* db) {
  sql::Statement s(db->GetSQLConnection()->GetUniqueStatement(
      "SELECT count FROM autofill WHERE name = ? AND value = ?"));
  s.BindString16(0, name);
  s.BindString16(1, value);
  if (!s.Step())
    return 0;
  return s.ColumnInt(0);
}

}  // namespace

class AutofillTableTest : public testing::Test {
 public:
  AutofillTableTest() = default;
  AutofillTableTest(const AutofillTableTest&) = delete;
  AutofillTableTest& operator=(const AutofillTableTest&) = delete;
  ~AutofillTableTest() override = default;

 protected:
  void SetUp() override {
    OSCryptMocker::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.GetPath().AppendASCII("TestWebDatabase");

    table_ = std::make_unique<AutofillTable>();
    db_ = std::make_unique<WebDatabase>();
    db_->AddTable(table_.get());
    ASSERT_EQ(sql::INIT_OK, db_->Init(file_));
  }

  void TearDown() override { OSCryptMocker::TearDown(); }

  base::FilePath file_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<AutofillTable> table_;
  std::unique_ptr<WebDatabase> db_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AutofillTableTest, Autofill) {
  Time t1 = AutofillClock::Now();

  // Simulate the submission of a handful of entries in a field called "Name",
  // some more often than others.
  AutofillChangeList changes;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  base::Time now = AutofillClock::Now();
  base::TimeDelta two_seconds = base::Seconds(2);
  EXPECT_TRUE(table_->AddFormFieldValue(field, &changes));
  std::vector<AutofillEntry> v;
  for (int i = 0; i < 5; ++i) {
    field.value = u"Clark Kent";
    EXPECT_TRUE(
        table_->AddFormFieldValueTime(field, &changes, now + i * two_seconds));
  }
  for (int i = 0; i < 3; ++i) {
    field.value = u"Clark Sutter";
    EXPECT_TRUE(
        table_->AddFormFieldValueTime(field, &changes, now + i * two_seconds));
  }
  for (int i = 0; i < 2; ++i) {
    field.name = u"Favorite Color";
    field.value = u"Green";
    EXPECT_TRUE(
        table_->AddFormFieldValueTime(field, &changes, now + i * two_seconds));
  }

  // We have added the name Clark Kent 5 times, so count should be 5.
  EXPECT_EQ(5, GetAutofillEntryCount(u"Name", u"Clark Kent", db_.get()));

  // Storing in the data base should be case sensitive, so there should be no
  // database entry for clark kent lowercase.
  EXPECT_EQ(0, GetAutofillEntryCount(u"Name", u"clark kent", db_.get()));

  EXPECT_EQ(2, GetAutofillEntryCount(u"Favorite Color", u"Green", db_.get()));

  // This is meant to get a list of suggestions for Name.  The empty prefix
  // in the second argument means it should return all suggestions for a name
  // no matter what they start with.  The order that the names occur in the list
  // should be decreasing order by count.
  EXPECT_TRUE(
      table_->GetFormValuesForElementName(u"Name", std::u16string(), &v, 6));
  EXPECT_EQ(3U, v.size());
  if (v.size() == 3) {
    EXPECT_EQ(u"Clark Kent", v[0].key().value());
    EXPECT_EQ(u"Clark Sutter", v[1].key().value());
    EXPECT_EQ(u"Superman", v[2].key().value());
  }

  // If we query again limiting the list size to 1, we should only get the most
  // frequent entry.
  EXPECT_TRUE(
      table_->GetFormValuesForElementName(u"Name", std::u16string(), &v, 1));
  EXPECT_EQ(1U, v.size());
  if (v.size() == 1) {
    EXPECT_EQ(u"Clark Kent", v[0].key().value());
  }

  // Querying for suggestions given a prefix is case-insensitive, so the prefix
  // "cLa" shoud get suggestions for both Clarks.
  EXPECT_TRUE(table_->GetFormValuesForElementName(u"Name", u"cLa", &v, 6));
  EXPECT_EQ(2U, v.size());
  if (v.size() == 2) {
    EXPECT_EQ(u"Clark Kent", v[0].key().value());
    EXPECT_EQ(u"Clark Sutter", v[1].key().value());
  }

  // Removing all elements since the beginning of this function should remove
  // everything from the database.
  changes.clear();
  EXPECT_TRUE(table_->RemoveFormElementsAddedBetween(t1, Time(), &changes));

  const AutofillChange kExpectedChanges[] = {
      AutofillChange(AutofillChange::REMOVE, AutofillKey(u"Name", u"Superman")),
      AutofillChange(AutofillChange::REMOVE,
                     AutofillKey(u"Name", u"Clark Kent")),
      AutofillChange(AutofillChange::REMOVE,
                     AutofillKey(u"Name", u"Clark Sutter")),
      AutofillChange(AutofillChange::REMOVE,
                     AutofillKey(u"Favorite Color", u"Green")),
  };
  EXPECT_EQ(base::size(kExpectedChanges), changes.size());
  for (size_t i = 0; i < base::size(kExpectedChanges); ++i) {
    EXPECT_EQ(kExpectedChanges[i], changes[i]);
  }

  EXPECT_EQ(0, GetAutofillEntryCount(u"Name", u"Clark Kent", db_.get()));

  EXPECT_TRUE(
      table_->GetFormValuesForElementName(u"Name", std::u16string(), &v, 6));
  EXPECT_EQ(0U, v.size());

  // Now add some values with empty strings.
  const std::u16string kValue = u"  toto   ";
  field.name = u"blank";
  field.value = std::u16string();
  EXPECT_TRUE(table_->AddFormFieldValue(field, &changes));
  field.name = u"blank";
  field.value = u" ";
  EXPECT_TRUE(table_->AddFormFieldValue(field, &changes));
  field.name = u"blank";
  field.value = u"      ";
  EXPECT_TRUE(table_->AddFormFieldValue(field, &changes));
  field.name = u"blank";
  field.value = kValue;
  EXPECT_TRUE(table_->AddFormFieldValue(field, &changes));

  // They should be stored normally as the DB layer does not check for empty
  // values.
  v.clear();
  EXPECT_TRUE(
      table_->GetFormValuesForElementName(u"blank", std::u16string(), &v, 10));
  EXPECT_EQ(4U, v.size());
}

TEST_F(AutofillTableTest, Autofill_GetEntry_Populated) {
  AutofillChangeList changes;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  base::Time now = base::Time::FromDoubleT(1546889367);

  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes, now));

  std::vector<AutofillEntry> prefix_v;
  EXPECT_TRUE(
      table_->GetFormValuesForElementName(field.name, u"Super", &prefix_v, 10));

  std::vector<AutofillEntry> no_prefix_v;
  EXPECT_TRUE(
      table_->GetFormValuesForElementName(field.name, u"", &no_prefix_v, 10));

  AutofillEntry expected_entry(AutofillKey(field.name, field.value), now, now);

  EXPECT_THAT(prefix_v, ElementsAre(expected_entry));
  EXPECT_THAT(no_prefix_v, ElementsAre(expected_entry));

  // Update date_last_used.
  base::Time new_time = now + base::Seconds(1000);
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes, new_time));
  EXPECT_TRUE(
      table_->GetFormValuesForElementName(field.name, u"Super", &prefix_v, 10));
  EXPECT_TRUE(
      table_->GetFormValuesForElementName(field.name, u"", &no_prefix_v, 10));

  expected_entry =
      AutofillEntry(AutofillKey(field.name, field.value), now, new_time);

  EXPECT_THAT(prefix_v, ElementsAre(expected_entry));
  EXPECT_THAT(no_prefix_v, ElementsAre(expected_entry));
}

TEST_F(AutofillTableTest, Autofill_GetCountOfValuesContainedBetween) {
  AutofillChangeList changes;
  // This test makes time comparisons that are precise to a microsecond, but the
  // database uses the time_t format which is only precise to a second.
  // Make sure we use timestamps rounded to a second.
  Time begin = Time::FromTimeT(AutofillClock::Now().ToTimeT());
  Time now = begin;
  base::TimeDelta second = base::Seconds(1);

  struct Entry {
    const char16_t* name;
    const char16_t* value;
  } entries[] = {{u"Alter ego", u"Superman"}, {u"Name", u"Superman"},
                 {u"Name", u"Clark Kent"},    {u"Name", u"Superman"},
                 {u"Name", u"Clark Sutter"},  {u"Nomen", u"Clark Kent"}};

  for (Entry entry : entries) {
    FormFieldData field;
    field.name = entry.name;
    field.value = entry.value;
    ASSERT_TRUE(table_->AddFormFieldValueTime(field, &changes, now));
    now += second;
  }

  // While the entry "Alter ego" : "Superman" is entirely contained within
  // the first second, the value "Superman" itself appears in another entry,
  // so it is not contained.
  EXPECT_EQ(0, table_->GetCountOfValuesContainedBetween(begin, begin + second));

  // No values are entirely contained within the first three seconds either
  // (note that the second time constraint is exclusive).
  EXPECT_EQ(
      0, table_->GetCountOfValuesContainedBetween(begin, begin + 3 * second));

  // Only "Superman" is entirely contained within the first four seconds.
  EXPECT_EQ(
      1, table_->GetCountOfValuesContainedBetween(begin, begin + 4 * second));

  // "Clark Kent" and "Clark Sutter" are contained between the first
  // and seventh second.
  EXPECT_EQ(2, table_->GetCountOfValuesContainedBetween(begin + second,
                                                        begin + 7 * second));

  // Beginning from the third second, "Clark Kent" is not contained.
  EXPECT_EQ(1, table_->GetCountOfValuesContainedBetween(begin + 3 * second,
                                                        begin + 7 * second));

  // We have three distinct values total.
  EXPECT_EQ(
      3, table_->GetCountOfValuesContainedBetween(begin, begin + 7 * second));

  // And we should get the same result for unlimited time interval.
  EXPECT_EQ(3, table_->GetCountOfValuesContainedBetween(Time(), Time::Max()));

  // The null time interval is also interpreted as unlimited.
  EXPECT_EQ(3, table_->GetCountOfValuesContainedBetween(Time(), Time()));

  // An interval that does not fully contain any entries returns zero.
  EXPECT_EQ(0, table_->GetCountOfValuesContainedBetween(begin + second,
                                                        begin + 2 * second));

  // So does an interval which has no intersection with any entry.
  EXPECT_EQ(0, table_->GetCountOfValuesContainedBetween(Time(), begin));
}

TEST_F(AutofillTableTest, Autofill_RemoveBetweenChanges) {
  base::TimeDelta one_day(base::Days(1));
  Time t1 = AutofillClock::Now();
  Time t2 = t1 + one_day;

  AutofillChangeList changes;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes, t1));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes, t2));

  changes.clear();
  EXPECT_TRUE(table_->RemoveFormElementsAddedBetween(t1, t2, &changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(
      AutofillChange(AutofillChange::UPDATE, AutofillKey(u"Name", u"Superman")),
      changes[0]);
  changes.clear();

  EXPECT_TRUE(
      table_->RemoveFormElementsAddedBetween(t2, t2 + one_day, &changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(
      AutofillChange(AutofillChange::REMOVE, AutofillKey(u"Name", u"Superman")),
      changes[0]);
}

TEST_F(AutofillTableTest, Autofill_AddChanges) {
  base::TimeDelta one_day(base::Days(1));
  Time t1 = AutofillClock::Now();
  Time t2 = t1 + one_day;

  AutofillChangeList changes;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes, t1));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(
      AutofillChange(AutofillChange::ADD, AutofillKey(u"Name", u"Superman")),
      changes[0]);

  changes.clear();
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes, t2));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(
      AutofillChange(AutofillChange::UPDATE, AutofillKey(u"Name", u"Superman")),
      changes[0]);
}

TEST_F(AutofillTableTest, Autofill_UpdateOneWithOneTimestamp) {
  AutofillEntry entry(MakeAutofillEntry(u"foo", u"bar", 1, -1));
  std::vector<AutofillEntry> entries;
  entries.push_back(entry);
  ASSERT_TRUE(table_->UpdateAutofillEntries(entries));

  EXPECT_EQ(1, GetAutofillEntryCount(u"foo", u"bar", db_.get()));

  std::vector<AutofillEntry> all_entries;
  ASSERT_TRUE(table_->GetAllAutofillEntries(&all_entries));
  ASSERT_EQ(1U, all_entries.size());
  EXPECT_EQ(entry, all_entries[0]);
}

TEST_F(AutofillTableTest, Autofill_UpdateOneWithTwoTimestamps) {
  AutofillEntry entry(MakeAutofillEntry(u"foo", u"bar", 1, 2));
  std::vector<AutofillEntry> entries;
  entries.push_back(entry);
  ASSERT_TRUE(table_->UpdateAutofillEntries(entries));

  EXPECT_EQ(2, GetAutofillEntryCount(u"foo", u"bar", db_.get()));

  std::vector<AutofillEntry> all_entries;
  ASSERT_TRUE(table_->GetAllAutofillEntries(&all_entries));
  ASSERT_EQ(1U, all_entries.size());
  EXPECT_EQ(entry, all_entries[0]);
}

TEST_F(AutofillTableTest, Autofill_GetAutofillTimestamps) {
  AutofillEntry entry(MakeAutofillEntry(u"foo", u"bar", 1, 2));
  std::vector<AutofillEntry> entries;
  entries.push_back(entry);
  ASSERT_TRUE(table_->UpdateAutofillEntries(entries));

  Time date_created, date_last_used;
  ASSERT_TRUE(table_->GetAutofillTimestamps(u"foo", u"bar", &date_created,
                                            &date_last_used));
  EXPECT_EQ(Time::FromTimeT(1), date_created);
  EXPECT_EQ(Time::FromTimeT(2), date_last_used);
}

TEST_F(AutofillTableTest, Autofill_UpdateTwo) {
  AutofillEntry entry0(MakeAutofillEntry(u"foo", u"bar0", 1, -1));
  AutofillEntry entry1(MakeAutofillEntry(u"foo", u"bar1", 2, 3));
  std::vector<AutofillEntry> entries;
  entries.push_back(entry0);
  entries.push_back(entry1);
  ASSERT_TRUE(table_->UpdateAutofillEntries(entries));

  EXPECT_EQ(1, GetAutofillEntryCount(u"foo", u"bar0", db_.get()));
  EXPECT_EQ(2, GetAutofillEntryCount(u"foo", u"bar1", db_.get()));
}

TEST_F(AutofillTableTest, Autofill_UpdateNullTerminated) {
  const char16_t kName[] = u"foo";
  const char16_t kValue[] = u"bar";
  // A value which contains terminating character.
  std::u16string value(kValue, base::size(kValue));

  AutofillEntry entry0(MakeAutofillEntry(kName, kValue, 1, -1));
  AutofillEntry entry1(MakeAutofillEntry(kName, value, 2, 3));
  std::vector<AutofillEntry> entries;
  entries.push_back(entry0);
  entries.push_back(entry1);
  ASSERT_TRUE(table_->UpdateAutofillEntries(entries));

  EXPECT_EQ(1, GetAutofillEntryCount(kName, kValue, db_.get()));
  EXPECT_EQ(2, GetAutofillEntryCount(kName, value, db_.get()));

  std::vector<AutofillEntry> all_entries;
  ASSERT_TRUE(table_->GetAllAutofillEntries(&all_entries));
  ASSERT_EQ(2U, all_entries.size());
  EXPECT_EQ(entry0, all_entries[0]);
  EXPECT_EQ(entry1, all_entries[1]);
}

TEST_F(AutofillTableTest, Autofill_UpdateReplace) {
  AutofillChangeList changes;
  // Add a form field.  This will be replaced.
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  EXPECT_TRUE(table_->AddFormFieldValue(field, &changes));

  AutofillEntry entry(MakeAutofillEntry(u"Name", u"Superman", 1, 2));
  std::vector<AutofillEntry> entries;
  entries.push_back(entry);
  ASSERT_TRUE(table_->UpdateAutofillEntries(entries));

  std::vector<AutofillEntry> all_entries;
  ASSERT_TRUE(table_->GetAllAutofillEntries(&all_entries));
  ASSERT_EQ(1U, all_entries.size());
  EXPECT_EQ(entry, all_entries[0]);
}

TEST_F(AutofillTableTest, Autofill_UpdateDontReplace) {
  Time t = AutofillClock::Now();
  AutofillEntry existing(
      MakeAutofillEntry(u"Name", u"Superman", t.ToTimeT(), -1));

  AutofillChangeList changes;
  // Add a form field.  This will NOT be replaced.
  FormFieldData field;
  field.name = existing.key().name();
  field.value = existing.key().value();
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes, t));
  AutofillEntry entry(MakeAutofillEntry(u"Name", u"Clark Kent", 1, 2));
  std::vector<AutofillEntry> entries;
  entries.push_back(entry);
  ASSERT_TRUE(table_->UpdateAutofillEntries(entries));

  std::vector<AutofillEntry> all_entries;
  ASSERT_TRUE(table_->GetAllAutofillEntries(&all_entries));
  ASSERT_EQ(2U, all_entries.size());
  AutofillEntrySet expected_entries(all_entries.begin(), all_entries.end(),
                                    CompareAutofillEntries);
  EXPECT_EQ(1U, expected_entries.count(existing));
  EXPECT_EQ(1U, expected_entries.count(entry));
}

TEST_F(AutofillTableTest, Autofill_AddFormFieldValues) {
  Time t = AutofillClock::Now();

  // Add multiple values for "firstname" and "lastname" names.  Test that only
  // first value of each gets added. Related to security issue:
  // http://crbug.com/51727.
  std::vector<FormFieldData> elements;
  FormFieldData field;
  field.name = u"firstname";
  field.value = u"Joe";
  elements.push_back(field);

  field.name = u"firstname";
  field.value = u"Jane";
  elements.push_back(field);

  field.name = u"lastname";
  field.value = u"Smith";
  elements.push_back(field);

  field.name = u"lastname";
  field.value = u"Jones";
  elements.push_back(field);

  std::vector<AutofillChange> changes;
  table_->AddFormFieldValuesTime(elements, &changes, t);

  ASSERT_EQ(2U, changes.size());
  EXPECT_EQ(changes[0], AutofillChange(AutofillChange::ADD,
                                       AutofillKey(u"firstname", u"Joe")));
  EXPECT_EQ(changes[1], AutofillChange(AutofillChange::ADD,
                                       AutofillKey(u"lastname", u"Smith")));

  std::vector<AutofillEntry> all_entries;
  ASSERT_TRUE(table_->GetAllAutofillEntries(&all_entries));
  ASSERT_EQ(2U, all_entries.size());
}

TEST_F(AutofillTableTest,
       Autofill_RemoveFormElementsAddedBetween_UsedOnlyBefore) {
  // Add an entry used only before the targetted range.
  AutofillChangeList changes;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(10)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(20)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(30)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(40)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(50)));

  EXPECT_EQ(5, GetAutofillEntryCount(field.name, field.value, db_.get()));

  changes.clear();
  EXPECT_TRUE(table_->RemoveFormElementsAddedBetween(
      base::Time::FromTimeT(51), base::Time::FromTimeT(60), &changes));
  EXPECT_TRUE(changes.empty());
  EXPECT_EQ(5, GetAutofillEntryCount(field.name, field.value, db_.get()));
}

TEST_F(AutofillTableTest,
       Autofill_RemoveFormElementsAddedBetween_UsedOnlyAfter) {
  // Add an entry used only after the targetted range.
  AutofillChangeList changes;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(50)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(60)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(70)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(80)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(90)));

  EXPECT_EQ(5, GetAutofillEntryCount(field.name, field.value, db_.get()));

  changes.clear();
  EXPECT_TRUE(table_->RemoveFormElementsAddedBetween(
      base::Time::FromTimeT(40), base::Time::FromTimeT(50), &changes));
  EXPECT_TRUE(changes.empty());
  EXPECT_EQ(5, GetAutofillEntryCount(field.name, field.value, db_.get()));
}

TEST_F(AutofillTableTest,
       Autofill_RemoveFormElementsAddedBetween_UsedOnlyDuring) {
  // Add an entry used entirely during the targetted range.
  AutofillChangeList changes;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(10)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(20)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(30)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(40)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(50)));

  EXPECT_EQ(5, GetAutofillEntryCount(field.name, field.value, db_.get()));

  changes.clear();
  EXPECT_TRUE(table_->RemoveFormElementsAddedBetween(
      base::Time::FromTimeT(10), base::Time::FromTimeT(51), &changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutofillChange(AutofillChange::REMOVE,
                           AutofillKey(field.name, field.value)),
            changes[0]);
  EXPECT_EQ(0, GetAutofillEntryCount(field.name, field.value, db_.get()));
}

TEST_F(AutofillTableTest,
       Autofill_RemoveFormElementsAddedBetween_UsedBeforeAndDuring) {
  // Add an entry used both before and during the targetted range.
  AutofillChangeList changes;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(10)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(20)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(30)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(40)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(50)));

  EXPECT_EQ(5, GetAutofillEntryCount(field.name, field.value, db_.get()));

  changes.clear();
  EXPECT_TRUE(table_->RemoveFormElementsAddedBetween(
      base::Time::FromTimeT(40), base::Time::FromTimeT(60), &changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutofillChange(AutofillChange::UPDATE,
                           AutofillKey(field.name, field.value)),
            changes[0]);
  EXPECT_EQ(4, GetAutofillEntryCount(field.name, field.value, db_.get()));
  base::Time date_created, date_last_used;
  EXPECT_TRUE(table_->GetAutofillTimestamps(field.name, field.value,
                                            &date_created, &date_last_used));
  EXPECT_EQ(base::Time::FromTimeT(10), date_created);
  EXPECT_EQ(base::Time::FromTimeT(39), date_last_used);
}

TEST_F(AutofillTableTest,
       Autofill_RemoveFormElementsAddedBetween_UsedDuringAndAfter) {
  // Add an entry used both during and after the targetted range.
  AutofillChangeList changes;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(50)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(60)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(70)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(80)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(90)));

  EXPECT_EQ(5, GetAutofillEntryCount(field.name, field.value, db_.get()));

  changes.clear();
  EXPECT_TRUE(table_->RemoveFormElementsAddedBetween(
      base::Time::FromTimeT(40), base::Time::FromTimeT(80), &changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutofillChange(AutofillChange::UPDATE,
                           AutofillKey(field.name, field.value)),
            changes[0]);
  EXPECT_EQ(2, GetAutofillEntryCount(field.name, field.value, db_.get()));
  base::Time date_created, date_last_used;
  EXPECT_TRUE(table_->GetAutofillTimestamps(field.name, field.value,
                                            &date_created, &date_last_used));
  EXPECT_EQ(base::Time::FromTimeT(80), date_created);
  EXPECT_EQ(base::Time::FromTimeT(90), date_last_used);
}

TEST_F(AutofillTableTest,
       Autofill_RemoveFormElementsAddedBetween_OlderThan30Days) {
  const base::Time kNow = AutofillClock::Now();
  const base::Time k29DaysOld = kNow - base::Days(29);
  const base::Time k30DaysOld = kNow - base::Days(30);
  const base::Time k31DaysOld = kNow - base::Days(31);

  // Add some form field entries.
  AutofillChangeList changes;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes, kNow));
  field.value = u"Clark Kent";
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes, k29DaysOld));
  field.value = u"Clark Sutter";
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes, k31DaysOld));
  EXPECT_EQ(3U, changes.size());

  // Removing all elements added before 30days from the database.
  changes.clear();
  EXPECT_TRUE(table_->RemoveFormElementsAddedBetween(base::Time(), k30DaysOld,
                                                     &changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutofillChange(AutofillChange::REMOVE,
                           AutofillKey(u"Name", u"Clark Sutter")),
            changes[0]);
  EXPECT_EQ(0, GetAutofillEntryCount(u"Name", u"Clark Sutter", db_.get()));
  EXPECT_EQ(1, GetAutofillEntryCount(u"Name", u"Superman", db_.get()));
  EXPECT_EQ(1, GetAutofillEntryCount(u"Name", u"Clark Kent", db_.get()));
  changes.clear();
}

// Tests that we set the change type to EXPIRE for expired elements and we
// delete an old entry.
TEST_F(AutofillTableTest, RemoveExpiredFormElements_Expires_DeleteEntry) {
  auto kNow = AutofillClock::Now();
  auto k2YearsOld =
      kNow - base::Days(2 * kAutocompleteRetentionPolicyPeriodInDays);

  AutofillChangeList changes;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes, k2YearsOld));
  changes.clear();

  EXPECT_TRUE(table_->RemoveExpiredFormElements(&changes));

  EXPECT_EQ(AutofillChange(AutofillChange::EXPIRE,
                           AutofillKey(field.name, field.value)),
            changes[0]);
}

// Tests that we don't
// delete non-expired entries' data from the SQLite table.
TEST_F(AutofillTableTest, RemoveExpiredFormElements_NotOldEnough) {
  auto kNow = AutofillClock::Now();
  auto k2DaysOld = kNow - base::Days(2);

  AutofillChangeList changes;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes, k2DaysOld));
  changes.clear();

  EXPECT_TRUE(table_->RemoveExpiredFormElements(&changes));
  EXPECT_TRUE(changes.empty());
}

TEST_F(AutofillTableTest,
       AutofillProfile_StructuredNames_BackAndForthMigration) {
  // Enable the structured names.
  scoped_feature_list_.InitWithFeatures(
      {features::kAutofillEnableSupportForMoreStructureInNames},
      {features::kAutofillEnableSupportForMoreStructureInAddresses});

  AutofillProfile structured_name_profile;
  structured_name_profile.set_origin(std::string());

  // TODO(crbug.com/1113617): Honorifics are temporally disabled.
  // structured_name_profile.SetRawInfoWithVerificationStatus(
  //     NAME_HONORIFIC_PREFIX, u"Dr.",
  //     VerificationStatus::kObserved);

  structured_name_profile.SetRawInfoWithVerificationStatus(
      NAME_FIRST, u"John", VerificationStatus::kObserved);

  structured_name_profile.SetRawInfoWithVerificationStatus(
      NAME_MIDDLE, u"Q.", VerificationStatus::kObserved);

  structured_name_profile.SetRawInfoWithVerificationStatus(
      NAME_LAST_FIRST, u"Agent", VerificationStatus::kParsed);

  structured_name_profile.SetRawInfoWithVerificationStatus(
      NAME_LAST_CONJUNCTION, u"007", VerificationStatus::kParsed);

  structured_name_profile.SetRawInfoWithVerificationStatus(
      NAME_LAST_SECOND, u"Smith", VerificationStatus::kParsed);

  structured_name_profile.SetRawInfoWithVerificationStatus(
      NAME_LAST, u"Agent 007 Smith", VerificationStatus::kParsed);

  structured_name_profile.SetRawInfoWithVerificationStatus(
      NAME_FULL, u"John Q. Agent 007 Smith", VerificationStatus::kObserved);

  structured_name_profile.SetRawInfo(EMAIL_ADDRESS, u"js@smith.xyz");

  structured_name_profile.SetRawInfo(COMPANY_NAME, u"Google");

  structured_name_profile.SetRawInfo(ADDRESS_HOME_LINE1, u"1234 Apple Way");

  structured_name_profile.SetRawInfo(ADDRESS_HOME_LINE2, u"unit 5");

  structured_name_profile.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                     u"Beverly Hills");

  structured_name_profile.SetRawInfo(ADDRESS_HOME_CITY, u"Los Angeles");

  structured_name_profile.SetRawInfo(ADDRESS_HOME_STATE, u"CA");

  structured_name_profile.SetRawInfo(ADDRESS_HOME_ZIP, u"90025");

  structured_name_profile.SetRawInfo(ADDRESS_HOME_SORTING_CODE, u"MAGIC ###");

  structured_name_profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");

  structured_name_profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"18181234567");

  structured_name_profile.set_language_code("en");

  structured_name_profile.SetClientValidityFromBitfieldValue(6);

  structured_name_profile.set_is_client_validity_states_updated(true);

  // Add the profile to the table.
  EXPECT_TRUE(table_->AddAutofillProfile(structured_name_profile));

  // Get the structured-name profile from the table.
  std::unique_ptr<AutofillProfile> db_profile =
      table_->GetAutofillProfile(structured_name_profile.guid());
  ASSERT_TRUE(db_profile);

  // Verify that it is correct.
  EXPECT_EQ(structured_name_profile, *db_profile);

  // Now the feature for new structured names is disabled and the profile
  // retrieved again.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(
      features::kAutofillEnableSupportForMoreStructureInNames);

  // Get the legacy profile from the table.
  std::unique_ptr<AutofillProfile> db_legacy_profile =
      table_->GetAutofillProfile(structured_name_profile.guid());
  ASSERT_TRUE(db_profile);

  // And verify that state of the retrieved profile since it should only contain
  // the legacy structure.
  // TODO(crbug.com/1113617): Honorifics are temporally disabled.
  EXPECT_EQ(db_legacy_profile->GetRawInfo(NAME_FULL),
            u"John Q. Agent 007 Smith");
  EXPECT_EQ(db_legacy_profile->GetRawInfo(NAME_FIRST), u"John");
  EXPECT_EQ(db_legacy_profile->GetRawInfo(NAME_MIDDLE), u"Q.");
  EXPECT_EQ(db_legacy_profile->GetRawInfo(NAME_LAST), u"Agent 007 Smith");
  EXPECT_TRUE(db_legacy_profile->GetRawInfo(NAME_HONORIFIC_PREFIX).empty());
  EXPECT_TRUE(db_legacy_profile->GetRawInfo(NAME_LAST_FIRST).empty());
  EXPECT_TRUE(db_legacy_profile->GetRawInfo(NAME_LAST_CONJUNCTION).empty());
  EXPECT_TRUE(db_legacy_profile->GetRawInfo(NAME_LAST_SECOND).empty());

  // Now the profile is updated (although it is technically the same).
  EXPECT_TRUE(table_->UpdateAutofillProfile(*db_legacy_profile));

  // Manually query the data base to verify that all tokens have been reset.
  sql::Statement s(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT "
      "guid, "
      "honorific_prefix, honorific_prefix_status, "
      "first_name, first_name_status, "
      "middle_name, middle_name_status, "
      "first_last_name, first_last_name_status, "
      "conjunction_last_name, conjunction_last_name_status, "
      "second_last_name, second_last_name_status, "
      "last_name, last_name_status, "
      "full_name, full_name_status "
      "FROM autofill_profile_names "
      "WHERE guid=? "
      "LIMIT 1"));
  s.BindString(0, structured_name_profile.guid());
  ASSERT_TRUE(s.is_valid());
  ASSERT_TRUE(s.Step());

  // Verify that the columns containing the additional structure were reset.
  // NAME_HONORIFIC_PREFIX
  EXPECT_TRUE(s.ColumnString16(1).empty());
  EXPECT_EQ(s.ColumnInt(2), 0);
  // NAME_LAST_FIRST
  EXPECT_TRUE(s.ColumnString16(7).empty());
  EXPECT_EQ(s.ColumnInt(8), 0);
  // NAME_LAST_CONJUNCTION
  EXPECT_TRUE(s.ColumnString16(9).empty());
  EXPECT_EQ(s.ColumnInt(10), 0);
  // NAME_LAST_SECOND
  EXPECT_TRUE(s.ColumnString16(11).empty());
  EXPECT_EQ(s.ColumnInt(12), 0);

  // Now the feature for new structured names is enabled again and the profile
  // is retrieved once more.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillEnableSupportForMoreStructureInNames);

  std::unique_ptr<AutofillProfile> db_migrated_profile =
      table_->GetAutofillProfile(structured_name_profile.guid());
  ASSERT_TRUE(db_migrated_profile);

  // Verify that the legacy tokens are written correctly to the profile and
  // the profile is migrated correctly.
  // TODO(crbug.com/1113617): Honorifics are temporally disabled.
  EXPECT_EQ(db_migrated_profile->GetRawInfo(NAME_FULL),
            u"John Q. Agent 007 Smith");
  EXPECT_TRUE(db_migrated_profile->GetRawInfo(NAME_HONORIFIC_PREFIX).empty());
  EXPECT_EQ(db_migrated_profile->GetRawInfo(NAME_FIRST), u"John");
  EXPECT_EQ(db_migrated_profile->GetRawInfo(NAME_MIDDLE), u"Q.");
  EXPECT_EQ(db_migrated_profile->GetRawInfo(NAME_LAST), u"Agent 007 Smith");
  EXPECT_TRUE(db_migrated_profile->GetRawInfo(NAME_LAST_FIRST).empty());
  EXPECT_TRUE(db_migrated_profile->GetRawInfo(NAME_LAST_CONJUNCTION).empty());
  EXPECT_EQ(db_migrated_profile->GetRawInfo(NAME_LAST_SECOND),
            u"Agent 007 Smith");

  EXPECT_EQ(db_migrated_profile->GetVerificationStatus(NAME_FULL),
            VerificationStatus::kObserved);
  // TODO(crbug.com/1113617): Honorifics are temporally disabled.
  EXPECT_EQ(db_migrated_profile->GetVerificationStatus(NAME_HONORIFIC_PREFIX),
            VerificationStatus::kNoStatus);
  EXPECT_EQ(db_migrated_profile->GetVerificationStatus(NAME_FIRST),
            VerificationStatus::kParsed);
  EXPECT_EQ(db_migrated_profile->GetVerificationStatus(NAME_MIDDLE),
            VerificationStatus::kParsed);
  EXPECT_EQ(db_migrated_profile->GetVerificationStatus(NAME_LAST),
            VerificationStatus::kParsed);
  EXPECT_EQ(db_migrated_profile->GetVerificationStatus(NAME_LAST_FIRST),
            VerificationStatus::kParsed);
  EXPECT_EQ(db_migrated_profile->GetVerificationStatus(NAME_LAST_CONJUNCTION),
            VerificationStatus::kParsed);
  EXPECT_EQ(db_migrated_profile->GetVerificationStatus(NAME_LAST_SECOND),
            VerificationStatus::kParsed);
}

TEST_F(AutofillTableTest, AutofillProfile_StructuredAddresses) {
  // Enable the structured addresses features.
  scoped_feature_list_.InitWithFeatures(
      {features::kAutofillEnableSupportForMoreStructureInAddresses}, {});

  AutofillProfile profile;
  profile.set_origin(std::string());

  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_ADDRESS,
      u"Street Name House Number Premise APT 10 Floor 2",
      VerificationStatus::kUserVerified);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_NAME, u"Street Name", VerificationStatus::kFormatted);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                           u"Dependent Locality",
                                           VerificationStatus::kObserved);

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_CITY, u"City",
                                           VerificationStatus::kObserved);

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STATE, u"State",
                                           VerificationStatus::kObserved);

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_SORTING_CODE,
                                           u"Sorting Code",
                                           VerificationStatus::kObserved);

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_ZIP, u"ZIP",
                                           VerificationStatus::kObserved);

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_COUNTRY, u"DE",
                                           VerificationStatus::kObserved);

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_DEPENDENT_STREET_NAME,
                                           u"", VerificationStatus::kObserved);

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER,
                                           u"House Number",
                                           VerificationStatus::kUserVerified);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_SUBPREMISE,
                                           u"APT 10 Floor 2",
                                           VerificationStatus::kUserVerified);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_APT_NUM, u"10",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_FLOOR, u"2",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_PREMISE_NAME, u"Premise", VerificationStatus::kUserVerified);
  ASSERT_EQ(profile.GetRawInfo(ADDRESS_HOME_STREET_NAME), u"Street Name");

  // Add the profile to the table.
  EXPECT_TRUE(table_->AddAutofillProfile(profile));

  // Read the profile from the table and verify the correct values.
  std::unique_ptr<AutofillProfile> db_profile =
      table_->GetAutofillProfile(profile.guid());
  ASSERT_TRUE(db_profile);

  EXPECT_EQ(db_profile->GetVerificationStatus(ADDRESS_HOME_STREET_NAME),
            VerificationStatus::kFormatted);

  EXPECT_EQ(
      db_profile->GetVerificationStatus(ADDRESS_HOME_DEPENDENT_STREET_NAME),
      VerificationStatus::kObserved);

  EXPECT_EQ(db_profile->GetVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER),
            VerificationStatus::kUserVerified);

  EXPECT_EQ(db_profile->GetVerificationStatus(ADDRESS_HOME_SUBPREMISE),
            VerificationStatus::kUserVerified);

  EXPECT_EQ(db_profile->GetVerificationStatus(ADDRESS_HOME_APT_NUM),
            VerificationStatus::kParsed);

  EXPECT_EQ(db_profile->GetVerificationStatus(ADDRESS_HOME_FLOOR),
            VerificationStatus::kParsed);

  EXPECT_EQ(db_profile->GetVerificationStatus(ADDRESS_HOME_PREMISE_NAME),
            VerificationStatus::kUserVerified);

  EXPECT_EQ(db_profile->GetVerificationStatus(ADDRESS_HOME_DEPENDENT_LOCALITY),
            VerificationStatus::kObserved);

  EXPECT_EQ(db_profile->GetVerificationStatus(ADDRESS_HOME_CITY),
            VerificationStatus::kObserved);

  EXPECT_EQ(db_profile->GetVerificationStatus(ADDRESS_HOME_STATE),
            VerificationStatus::kObserved);

  EXPECT_EQ(db_profile->GetVerificationStatus(ADDRESS_HOME_SORTING_CODE),
            VerificationStatus::kObserved);

  EXPECT_EQ(db_profile->GetVerificationStatus(ADDRESS_HOME_ZIP),
            VerificationStatus::kObserved);

  EXPECT_EQ(db_profile->GetVerificationStatus(ADDRESS_HOME_COUNTRY),
            VerificationStatus::kObserved);

  EXPECT_EQ(db_profile->GetRawInfo(ADDRESS_HOME_STREET_NAME), u"Street Name");
  EXPECT_EQ(db_profile->GetRawInfo(ADDRESS_HOME_DEPENDENT_STREET_NAME), u"");
  EXPECT_EQ(db_profile->GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER), u"House Number");
  EXPECT_EQ(db_profile->GetRawInfo(ADDRESS_HOME_SUBPREMISE), u"APT 10 Floor 2");
  EXPECT_EQ(db_profile->GetRawInfo(ADDRESS_HOME_APT_NUM), u"10");
  EXPECT_EQ(db_profile->GetRawInfo(ADDRESS_HOME_FLOOR), u"2");
  EXPECT_EQ(db_profile->GetRawInfo(ADDRESS_HOME_PREMISE_NAME), u"Premise");
  EXPECT_EQ(db_profile->GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY),
            u"Dependent Locality");
  EXPECT_EQ(db_profile->GetRawInfo(ADDRESS_HOME_CITY), u"City");
  EXPECT_EQ(db_profile->GetRawInfo(ADDRESS_HOME_STATE), u"State");
  EXPECT_EQ(db_profile->GetRawInfo(ADDRESS_HOME_SORTING_CODE), u"Sorting Code");
  EXPECT_EQ(db_profile->GetRawInfo(ADDRESS_HOME_ZIP), u"ZIP");
  EXPECT_EQ(db_profile->GetRawInfo(ADDRESS_HOME_COUNTRY), u"DE");

  EXPECT_EQ(profile, *db_profile);
}

TEST_F(AutofillTableTest,
       AutofillProfile_StructuredAddresses_Eventual_Deletion) {
  // Enable the structured addresses.
  scoped_feature_list_.InitWithFeatures(
      {features::kAutofillEnableSupportForMoreStructureInAddresses}, {});

  AutofillProfile profile;
  profile.set_origin(std::string());

  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_ADDRESS,
      u"Street Name House Number Premise Subpremise",
      VerificationStatus::kUserVerified);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_NAME, u"Street Name", VerificationStatus::kFormatted);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_DEPENDENT_STREET_NAME,
                                           u"", VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER,
                                           u"House Number",
                                           VerificationStatus::kUserVerified);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_SUBPREMISE,
                                           u"Subpremise",
                                           VerificationStatus::kUserVerified);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_PREMISE_NAME, u"Premise", VerificationStatus::kUserVerified);
  ASSERT_EQ(profile.GetRawInfo(ADDRESS_HOME_STREET_NAME), u"Street Name");
  // Add the profile to the table.
  EXPECT_TRUE(table_->AddAutofillProfile(profile));

  AutofillClock::Now();
  std::unique_ptr<AutofillProfile> db_profile =
      table_->GetAutofillProfile(profile.guid());
  ASSERT_TRUE(db_profile);

  // Verify that it is correct.
  EXPECT_EQ(profile, *db_profile);

  EXPECT_EQ(db_profile->GetRawInfo(ADDRESS_HOME_STREET_NAME), u"Street Name");
  EXPECT_EQ(db_profile->GetRawInfo(ADDRESS_HOME_DEPENDENT_STREET_NAME), u"");
  EXPECT_EQ(db_profile->GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER), u"House Number");
  EXPECT_EQ(db_profile->GetRawInfo(ADDRESS_HOME_SUBPREMISE), u"Subpremise");
  EXPECT_EQ(db_profile->GetRawInfo(ADDRESS_HOME_PREMISE_NAME), u"Premise");

  // Deactivate the features.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {}, {features::kAutofillEnableSupportForMoreStructureInAddresses});

  // Retrieve the address and verify that the structured tokens are not written.
  std::unique_ptr<AutofillProfile> legacy_db_profile =
      table_->GetAutofillProfile(profile.guid());
  ASSERT_TRUE(legacy_db_profile);

  EXPECT_EQ(legacy_db_profile->GetRawInfo(ADDRESS_HOME_STREET_NAME),
            std::u16string());
  EXPECT_EQ(legacy_db_profile->GetRawInfo(ADDRESS_HOME_DEPENDENT_STREET_NAME),
            std::u16string());
  EXPECT_EQ(legacy_db_profile->GetRawInfo(ADDRESS_HOME_HOUSE_NUMBER),
            std::u16string());
  EXPECT_EQ(legacy_db_profile->GetRawInfo(ADDRESS_HOME_SUBPREMISE),
            std::u16string());
  EXPECT_EQ(legacy_db_profile->GetRawInfo(ADDRESS_HOME_PREMISE_NAME),
            std::u16string());

  // Change the street address and update the profile.
  legacy_db_profile->SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_ADDRESS, u"Other Street Address",
      VerificationStatus::kUserVerified);
  table_->UpdateAutofillProfile(*legacy_db_profile);
  std::unique_ptr<AutofillProfile> changed_db_profile =
      table_->GetAutofillProfile(profile.guid());
  EXPECT_EQ(changed_db_profile->GetRawInfo(ADDRESS_HOME_STREET_ADDRESS),
            u"Other Street Address");

  // Note, this step already removes the structured address entry from the
  // table. To simulate the behavior of legacy clients, we manually insert it
  // again.
  sql::Statement s(db_->GetSQLConnection()->GetUniqueStatement(
      "INSERT INTO autofill_profile_addresses "
      "(guid, street_address) VALUES (?,?)"));
  s.BindString(0, profile.guid());
  s.BindString16(1, u"Street Address");
  ASSERT_TRUE(s.is_valid());
  ASSERT_TRUE(s.Run());

  // And verify that it is written.
  sql::Statement s1(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT count(*) "
      "FROM autofill_profile_addresses "
      "WHERE guid=?"));
  s1.BindString(0, profile.guid());
  ASSERT_TRUE(s1.is_valid());
  ASSERT_TRUE(s1.Step());
  EXPECT_EQ(1, s1.ColumnInt(0));

  // Enable the feature again and load the profile.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillEnableSupportForMoreStructureInAddresses);

  // Retrieve the address and manually query the data base to verify that the
  // structured address was deleted.
  std::unique_ptr<AutofillProfile> migrated_db_profile =
      table_->GetAutofillProfile(profile.guid());
  ASSERT_TRUE(migrated_db_profile);
  sql::Statement s2(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT count(*) "
      "FROM autofill_profile_addresses "
      "WHERE guid=?"));
  s2.BindString(0, profile.guid());
  ASSERT_TRUE(s2.is_valid());
  ASSERT_TRUE(s2.Step());

  EXPECT_EQ(0, s2.ColumnInt(0));
}

// This test is an adaption of |AutofillTableTest.AutofillProfile| to structured
// names.
TEST_F(AutofillTableTest, AutofillProfile_StructuredNames) {
  // Enable the structured names.
  scoped_feature_list_.InitWithFeatures(
      {features::kAutofillEnableSupportForMoreStructureInNames},
      {features::kAutofillEnableSupportForMoreStructureInAddresses});

  AutofillProfile home_profile;
  home_profile.set_origin(std::string());

  // TODO(crbug.com/1113617): Honorifics are temporally disabled.
  // home_profile.SetRawInfoWithVerificationStatus(
  // NAME_HONORIFIC_PREFIX, u"Dr.",
  // VerificationStatus::kObserved);

  home_profile.SetRawInfoWithVerificationStatus(NAME_HONORIFIC_PREFIX, u"Dr.",
                                                VerificationStatus::kObserved);

  home_profile.SetRawInfoWithVerificationStatus(NAME_FIRST, u"John",
                                                VerificationStatus::kObserved);

  home_profile.SetRawInfoWithVerificationStatus(NAME_MIDDLE, u"Q.",
                                                VerificationStatus::kObserved);

  home_profile.SetRawInfoWithVerificationStatus(NAME_LAST_FIRST, u"Agent",
                                                VerificationStatus::kParsed);

  home_profile.SetRawInfoWithVerificationStatus(NAME_LAST_CONJUNCTION, u"007",
                                                VerificationStatus::kParsed);

  home_profile.SetRawInfoWithVerificationStatus(NAME_LAST_SECOND, u"Smith",
                                                VerificationStatus::kParsed);

  home_profile.SetRawInfoWithVerificationStatus(NAME_LAST, u"Agent 007 Smith",
                                                VerificationStatus::kParsed);

  home_profile.SetRawInfoWithVerificationStatus(
      NAME_FULL, u"John Q. Agent 007 Smith", VerificationStatus::kObserved);

  home_profile.SetRawInfoWithVerificationStatus(NAME_FULL_WITH_HONORIFIC_PREFIX,
                                                u"Dr. John Q. Agent 007 Smith",
                                                VerificationStatus::kObserved);

  home_profile.SetRawInfo(EMAIL_ADDRESS, u"js@smith.xyz");
  home_profile.SetRawInfo(COMPANY_NAME, u"Google");
  home_profile.SetRawInfo(ADDRESS_HOME_LINE1, u"1234 Apple Way");
  home_profile.SetRawInfo(ADDRESS_HOME_LINE2, u"unit 5");
  home_profile.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY, u"Beverly Hills");
  home_profile.SetRawInfo(ADDRESS_HOME_CITY, u"Los Angeles");
  home_profile.SetRawInfo(ADDRESS_HOME_STATE, u"CA");
  home_profile.SetRawInfo(ADDRESS_HOME_ZIP, u"90025");
  home_profile.SetRawInfo(ADDRESS_HOME_SORTING_CODE, u"MAGIC ###");
  home_profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");
  home_profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"18181234567");
  home_profile.set_disallow_settings_visible_updates(true);
  home_profile.set_language_code("en");
  home_profile.SetClientValidityFromBitfieldValue(6);
  home_profile.set_is_client_validity_states_updated(true);
  Time pre_creation_time = AutofillClock::Now();

  // Add the profile to the table.
  EXPECT_TRUE(table_->AddAutofillProfile(home_profile));
  Time post_creation_time = AutofillClock::Now();

  // Get the 'Home' profile from the table.
  std::unique_ptr<AutofillProfile> db_profile =
      table_->GetAutofillProfile(home_profile.guid());
  ASSERT_TRUE(db_profile);

  // Verify that it is correct.
  EXPECT_EQ(home_profile, *db_profile);

  sql::Statement s_home(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified "
      "FROM autofill_profiles WHERE guid=?"));
  s_home.BindString(0, home_profile.guid());
  ASSERT_TRUE(s_home.is_valid());
  ASSERT_TRUE(s_home.Step());
  EXPECT_GE(s_home.ColumnInt64(0), pre_creation_time.ToTimeT());
  EXPECT_LE(s_home.ColumnInt64(0), post_creation_time.ToTimeT());
  EXPECT_FALSE(s_home.Step());

  // Add a 'Billing' profile.
  AutofillProfile billing_profile = home_profile;
  billing_profile.set_guid(base::GenerateGUID());
  billing_profile.set_origin("https://www.example.com/");
  billing_profile.SetRawInfo(ADDRESS_HOME_LINE1, u"5678 Bottom Street");
  billing_profile.SetRawInfo(ADDRESS_HOME_LINE2, u"suite 3");

  pre_creation_time = AutofillClock::Now();
  EXPECT_TRUE(table_->AddAutofillProfile(billing_profile));
  post_creation_time = AutofillClock::Now();

  // Get the 'Billing' profile.
  db_profile = table_->GetAutofillProfile(billing_profile.guid());
  ASSERT_TRUE(db_profile);
  EXPECT_EQ(billing_profile, *db_profile);
  sql::Statement s_billing(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM autofill_profiles WHERE guid=?"));
  s_billing.BindString(0, billing_profile.guid());
  ASSERT_TRUE(s_billing.is_valid());
  ASSERT_TRUE(s_billing.Step());
  EXPECT_GE(s_billing.ColumnInt64(0), pre_creation_time.ToTimeT());
  EXPECT_LE(s_billing.ColumnInt64(0), post_creation_time.ToTimeT());
  EXPECT_FALSE(s_billing.Step());

  // Update the 'Billing' profile, name only.
  billing_profile.SetRawInfoWithVerificationStatus(
      NAME_FIRST, u"Jane", VerificationStatus::kObserved);
  Time pre_modification_time = AutofillClock::Now();
  EXPECT_TRUE(table_->UpdateAutofillProfile(billing_profile));
  Time post_modification_time = AutofillClock::Now();
  db_profile = table_->GetAutofillProfile(billing_profile.guid());
  ASSERT_TRUE(db_profile);
  EXPECT_EQ(billing_profile, *db_profile);
  sql::Statement s_billing_updated(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM autofill_profiles WHERE guid=?"));
  s_billing_updated.BindString(0, billing_profile.guid());
  ASSERT_TRUE(s_billing_updated.is_valid());
  ASSERT_TRUE(s_billing_updated.Step());
  EXPECT_GE(s_billing_updated.ColumnInt64(0), pre_modification_time.ToTimeT());
  EXPECT_LE(s_billing_updated.ColumnInt64(0), post_modification_time.ToTimeT());
  EXPECT_FALSE(s_billing_updated.Step());

  // Update the 'Billing' profile with non-default data. The specific values are
  // not important.
  billing_profile.set_origin(kSettingsOrigin);
  billing_profile.SetRawInfoWithVerificationStatus(
      NAME_FIRST, u"Pablo", VerificationStatus::kObserved);
  billing_profile.SetRawInfoWithVerificationStatus(
      NAME_MIDDLE, u"Diege", VerificationStatus::kObserved);
  billing_profile.SetRawInfoWithVerificationStatus(NAME_LAST_FIRST, u"Ruiz",
                                                   VerificationStatus::kParsed);
  billing_profile.SetRawInfoWithVerificationStatus(NAME_LAST_CONJUNCTION, u"y",
                                                   VerificationStatus::kParsed);
  billing_profile.SetRawInfoWithVerificationStatus(NAME_LAST, u"Ruiz y Picasoo",
                                                   VerificationStatus::kParsed);
  billing_profile.SetRawInfoWithVerificationStatus(NAME_LAST_SECOND, u"Picasoo",
                                                   VerificationStatus::kParsed);
  billing_profile.SetRawInfo(EMAIL_ADDRESS, u"jane@singer.com");
  billing_profile.SetRawInfo(COMPANY_NAME, u"Indy");
  billing_profile.SetRawInfo(ADDRESS_HOME_LINE1, u"Open Road");
  billing_profile.SetRawInfo(ADDRESS_HOME_LINE2, u"Route 66");
  billing_profile.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY, u"District 9");
  billing_profile.SetRawInfo(ADDRESS_HOME_CITY, u"NFA");
  billing_profile.SetRawInfo(ADDRESS_HOME_STATE, u"NY");
  billing_profile.SetRawInfo(ADDRESS_HOME_ZIP, u"10011");
  billing_profile.SetRawInfo(ADDRESS_HOME_SORTING_CODE, u"123456");
  billing_profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");
  billing_profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"18181230000");
  billing_profile.SetClientValidityFromBitfieldValue(54);
  billing_profile.set_is_client_validity_states_updated(true);

  Time pre_modification_time_2 = AutofillClock::Now();
  EXPECT_TRUE(table_->UpdateAutofillProfile(billing_profile));
  Time post_modification_time_2 = AutofillClock::Now();
  db_profile = table_->GetAutofillProfile(billing_profile.guid());
  ASSERT_TRUE(db_profile);
  EXPECT_EQ(billing_profile, *db_profile);
  sql::Statement s_billing_updated_2(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT date_modified FROM autofill_profiles WHERE guid=?"));
  s_billing_updated_2.BindString(0, billing_profile.guid());
  ASSERT_TRUE(s_billing_updated_2.is_valid());
  ASSERT_TRUE(s_billing_updated_2.Step());
  EXPECT_GE(s_billing_updated_2.ColumnInt64(0),
            pre_modification_time_2.ToTimeT());
  EXPECT_LE(s_billing_updated_2.ColumnInt64(0),
            post_modification_time_2.ToTimeT());
  EXPECT_FALSE(s_billing_updated_2.Step());

  // Remove the 'Billing' profile.
  EXPECT_TRUE(table_->RemoveAutofillProfile(billing_profile.guid()));
  db_profile = table_->GetAutofillProfile(billing_profile.guid());
  EXPECT_FALSE(db_profile);
}

// TODO(crbug.com/1103421): Clean legacy implementation once structured names
// are fully launched.
TEST_F(AutofillTableTest, AutofillProfile) {
  // Disable the structured names since this test is only applicable if
  // structured names are not used.
  scoped_feature_list_.InitWithFeatures(
      {}, {features::kAutofillEnableSupportForMoreStructureInAddresses,
           features::kAutofillEnableSupportForMoreStructureInNames});

  // Add a 'Home' profile with non-default data. The specific values are not
  // important.
  AutofillProfile home_profile;
  home_profile.set_origin(std::string());
  home_profile.SetRawInfo(NAME_FIRST, u"John");
  home_profile.SetRawInfo(NAME_MIDDLE, u"Q.");
  home_profile.SetRawInfo(NAME_LAST, u"Smith");
  home_profile.SetRawInfo(EMAIL_ADDRESS, u"js@smith.xyz");
  home_profile.SetRawInfo(COMPANY_NAME, u"Google");
  home_profile.SetRawInfo(ADDRESS_HOME_LINE1, u"1234 Apple Way");
  home_profile.SetRawInfo(ADDRESS_HOME_LINE2, u"unit 5");
  home_profile.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY, u"Beverly Hills");
  home_profile.SetRawInfo(ADDRESS_HOME_CITY, u"Los Angeles");
  home_profile.SetRawInfo(ADDRESS_HOME_STATE, u"CA");
  home_profile.SetRawInfo(ADDRESS_HOME_ZIP, u"90025");
  home_profile.SetRawInfo(ADDRESS_HOME_SORTING_CODE, u"MAGIC ###");
  home_profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");
  home_profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"18181234567");
  home_profile.set_language_code("en");
  home_profile.SetClientValidityFromBitfieldValue(6);
  home_profile.set_is_client_validity_states_updated(true);

  Time pre_creation_time = AutofillClock::Now();
  EXPECT_TRUE(table_->AddAutofillProfile(home_profile));
  Time post_creation_time = AutofillClock::Now();

  // Get the 'Home' profile.
  std::unique_ptr<AutofillProfile> db_profile =
      table_->GetAutofillProfile(home_profile.guid());
  ASSERT_TRUE(db_profile);
  EXPECT_EQ(home_profile, *db_profile);
  sql::Statement s_home(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified "
      "FROM autofill_profiles WHERE guid=?"));
  s_home.BindString(0, home_profile.guid());
  ASSERT_TRUE(s_home.is_valid());
  ASSERT_TRUE(s_home.Step());
  EXPECT_GE(s_home.ColumnInt64(0), pre_creation_time.ToTimeT());
  EXPECT_LE(s_home.ColumnInt64(0), post_creation_time.ToTimeT());
  EXPECT_FALSE(s_home.Step());

  // Add a 'Billing' profile.
  AutofillProfile billing_profile = home_profile;
  billing_profile.set_guid(base::GenerateGUID());
  billing_profile.set_origin("https://www.example.com/");
  billing_profile.SetRawInfo(ADDRESS_HOME_LINE1, u"5678 Bottom Street");
  billing_profile.SetRawInfo(ADDRESS_HOME_LINE2, u"suite 3");

  pre_creation_time = AutofillClock::Now();
  EXPECT_TRUE(table_->AddAutofillProfile(billing_profile));
  post_creation_time = AutofillClock::Now();

  // Get the 'Billing' profile.
  db_profile = table_->GetAutofillProfile(billing_profile.guid());
  ASSERT_TRUE(db_profile);
  EXPECT_EQ(billing_profile, *db_profile);
  sql::Statement s_billing(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM autofill_profiles WHERE guid=?"));
  s_billing.BindString(0, billing_profile.guid());
  ASSERT_TRUE(s_billing.is_valid());
  ASSERT_TRUE(s_billing.Step());
  EXPECT_GE(s_billing.ColumnInt64(0), pre_creation_time.ToTimeT());
  EXPECT_LE(s_billing.ColumnInt64(0), post_creation_time.ToTimeT());
  EXPECT_FALSE(s_billing.Step());

  // Update the 'Billing' profile, name only.
  billing_profile.SetRawInfo(NAME_FIRST, u"Jane");
  Time pre_modification_time = AutofillClock::Now();
  EXPECT_TRUE(table_->UpdateAutofillProfile(billing_profile));
  Time post_modification_time = AutofillClock::Now();
  db_profile = table_->GetAutofillProfile(billing_profile.guid());
  ASSERT_TRUE(db_profile);
  EXPECT_EQ(billing_profile, *db_profile);
  sql::Statement s_billing_updated(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM autofill_profiles WHERE guid=?"));
  s_billing_updated.BindString(0, billing_profile.guid());
  ASSERT_TRUE(s_billing_updated.is_valid());
  ASSERT_TRUE(s_billing_updated.Step());
  EXPECT_GE(s_billing_updated.ColumnInt64(0), pre_modification_time.ToTimeT());
  EXPECT_LE(s_billing_updated.ColumnInt64(0), post_modification_time.ToTimeT());
  EXPECT_FALSE(s_billing_updated.Step());

  // Update the 'Billing' profile with non-default data. The specific values are
  // not important.
  billing_profile.set_origin(kSettingsOrigin);
  billing_profile.SetRawInfo(NAME_FIRST, u"Janice");
  billing_profile.SetRawInfo(NAME_MIDDLE, u"C.");
  billing_profile.SetRawInfo(NAME_FIRST, u"Joplin");
  billing_profile.SetRawInfo(EMAIL_ADDRESS, u"jane@singer.com");
  billing_profile.SetRawInfo(COMPANY_NAME, u"Indy");
  billing_profile.SetRawInfo(ADDRESS_HOME_LINE1, u"Open Road");
  billing_profile.SetRawInfo(ADDRESS_HOME_LINE2, u"Route 66");
  billing_profile.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY, u"District 9");
  billing_profile.SetRawInfo(ADDRESS_HOME_CITY, u"NFA");
  billing_profile.SetRawInfo(ADDRESS_HOME_STATE, u"NY");
  billing_profile.SetRawInfo(ADDRESS_HOME_ZIP, u"10011");
  billing_profile.SetRawInfo(ADDRESS_HOME_SORTING_CODE, u"123456");
  billing_profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");
  billing_profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"18181230000");
  billing_profile.SetClientValidityFromBitfieldValue(54);
  billing_profile.set_is_client_validity_states_updated(true);

  Time pre_modification_time_2 = AutofillClock::Now();
  EXPECT_TRUE(table_->UpdateAutofillProfile(billing_profile));
  Time post_modification_time_2 = AutofillClock::Now();
  db_profile = table_->GetAutofillProfile(billing_profile.guid());
  ASSERT_TRUE(db_profile);
  EXPECT_EQ(billing_profile, *db_profile);
  sql::Statement s_billing_updated_2(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT date_modified FROM autofill_profiles WHERE guid=?"));
  s_billing_updated_2.BindString(0, billing_profile.guid());
  ASSERT_TRUE(s_billing_updated_2.is_valid());
  ASSERT_TRUE(s_billing_updated_2.Step());
  EXPECT_GE(s_billing_updated_2.ColumnInt64(0),
            pre_modification_time_2.ToTimeT());
  EXPECT_LE(s_billing_updated_2.ColumnInt64(0),
            post_modification_time_2.ToTimeT());
  EXPECT_FALSE(s_billing_updated_2.Step());

  // Remove the 'Billing' profile.
  EXPECT_TRUE(table_->RemoveAutofillProfile(billing_profile.guid()));
  db_profile = table_->GetAutofillProfile(billing_profile.guid());
  EXPECT_FALSE(db_profile);
}

TEST_F(AutofillTableTest, CreditCard) {
  // Add a 'Work' credit card.
  CreditCard work_creditcard;
  work_creditcard.set_origin("https://www.example.com/");
  work_creditcard.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Jack Torrance");
  work_creditcard.SetRawInfo(CREDIT_CARD_NUMBER, u"1234567890123456");
  work_creditcard.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"04");
  work_creditcard.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2013");
  work_creditcard.SetNickname(u"Corporate card");

  Time pre_creation_time = AutofillClock::Now();
  EXPECT_TRUE(table_->AddCreditCard(work_creditcard));
  Time post_creation_time = AutofillClock::Now();

  // Get the 'Work' credit card.
  std::unique_ptr<CreditCard> db_creditcard =
      table_->GetCreditCard(work_creditcard.guid());
  ASSERT_TRUE(db_creditcard);
  EXPECT_EQ(work_creditcard, *db_creditcard);
  sql::Statement s_work(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT guid, name_on_card, expiration_month, expiration_year, "
      "card_number_encrypted, date_modified, nickname "
      "FROM credit_cards WHERE guid=?"));
  s_work.BindString(0, work_creditcard.guid());
  ASSERT_TRUE(s_work.is_valid());
  ASSERT_TRUE(s_work.Step());
  EXPECT_GE(s_work.ColumnInt64(5), pre_creation_time.ToTimeT());
  EXPECT_LE(s_work.ColumnInt64(5), post_creation_time.ToTimeT());
  EXPECT_FALSE(s_work.Step());

  // Add a 'Target' credit card.
  CreditCard target_creditcard;
  target_creditcard.set_origin(std::string());
  target_creditcard.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Jack Torrance");
  target_creditcard.SetRawInfo(CREDIT_CARD_NUMBER, u"1111222233334444");
  target_creditcard.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"06");
  target_creditcard.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2012");
  target_creditcard.SetNickname(u"Grocery card");

  pre_creation_time = AutofillClock::Now();
  EXPECT_TRUE(table_->AddCreditCard(target_creditcard));
  post_creation_time = AutofillClock::Now();
  db_creditcard = table_->GetCreditCard(target_creditcard.guid());
  ASSERT_TRUE(db_creditcard);
  EXPECT_EQ(target_creditcard, *db_creditcard);
  sql::Statement s_target(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT guid, name_on_card, expiration_month, expiration_year, "
      "card_number_encrypted, date_modified, nickname "
      "FROM credit_cards WHERE guid=?"));
  s_target.BindString(0, target_creditcard.guid());
  ASSERT_TRUE(s_target.is_valid());
  ASSERT_TRUE(s_target.Step());
  EXPECT_GE(s_target.ColumnInt64(5), pre_creation_time.ToTimeT());
  EXPECT_LE(s_target.ColumnInt64(5), post_creation_time.ToTimeT());
  EXPECT_FALSE(s_target.Step());

  // Update the 'Target' credit card.
  target_creditcard.set_origin("Interactive Autofill dialog");
  target_creditcard.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Charles Grady");
  target_creditcard.SetNickname(u"Supermarket");
  Time pre_modification_time = AutofillClock::Now();
  EXPECT_TRUE(table_->UpdateCreditCard(target_creditcard));
  Time post_modification_time = AutofillClock::Now();
  db_creditcard = table_->GetCreditCard(target_creditcard.guid());
  ASSERT_TRUE(db_creditcard);
  EXPECT_EQ(target_creditcard, *db_creditcard);
  sql::Statement s_target_updated(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT guid, name_on_card, expiration_month, expiration_year, "
      "card_number_encrypted, date_modified, nickname "
      "FROM credit_cards WHERE guid=?"));
  s_target_updated.BindString(0, target_creditcard.guid());
  ASSERT_TRUE(s_target_updated.is_valid());
  ASSERT_TRUE(s_target_updated.Step());
  EXPECT_GE(s_target_updated.ColumnInt64(5), pre_modification_time.ToTimeT());
  EXPECT_LE(s_target_updated.ColumnInt64(5), post_modification_time.ToTimeT());
  EXPECT_FALSE(s_target_updated.Step());

  // Remove the 'Target' credit card.
  EXPECT_TRUE(table_->RemoveCreditCard(target_creditcard.guid()));
  db_creditcard = table_->GetCreditCard(target_creditcard.guid());
  EXPECT_FALSE(db_creditcard);
}

TEST_F(AutofillTableTest, AddFullServerCreditCard) {
  CreditCard credit_card;
  credit_card.set_record_type(CreditCard::FULL_SERVER_CARD);
  credit_card.set_server_id("server_id");
  credit_card.set_origin("https://www.example.com/");
  credit_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Jack Torrance");
  credit_card.SetRawInfo(CREDIT_CARD_NUMBER, u"1234567890123456");
  credit_card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"04");
  credit_card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2013");

  EXPECT_TRUE(table_->AddFullServerCreditCard(credit_card));

  std::vector<std::unique_ptr<CreditCard>> outputs;
  ASSERT_TRUE(table_->GetServerCreditCards(&outputs));
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ(0, credit_card.Compare(*outputs[0]));
}

TEST_F(AutofillTableTest, UpdateAutofillProfile) {
  // Add a profile to the db.
  AutofillProfile profile;
  profile.SetRawInfo(NAME_FIRST, u"John");
  profile.SetRawInfo(NAME_MIDDLE, u"Q.");
  profile.SetRawInfo(NAME_LAST, u"Smith");
  profile.SetRawInfo(EMAIL_ADDRESS, u"js@example.com");
  profile.SetRawInfo(COMPANY_NAME, u"Google");
  profile.SetRawInfo(ADDRESS_HOME_LINE1, u"1234 Apple Way");
  profile.SetRawInfo(ADDRESS_HOME_LINE2, u"unit 5");
  profile.SetRawInfo(ADDRESS_HOME_CITY, u"Los Angeles");
  profile.SetRawInfo(ADDRESS_HOME_STATE, u"CA");
  profile.SetRawInfo(ADDRESS_HOME_ZIP, u"90025");
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"18181234567");
  profile.set_language_code("en");
  profile.FinalizeAfterImport();
  table_->AddAutofillProfile(profile);

  // Set a mocked value for the profile's creation time.
  const time_t kMockCreationDate = AutofillClock::Now().ToTimeT() - 13;
  sql::Statement s_mock_creation_date(
      db_->GetSQLConnection()->GetUniqueStatement(
          "UPDATE autofill_profiles SET date_modified = ?"));
  ASSERT_TRUE(s_mock_creation_date.is_valid());
  s_mock_creation_date.BindInt64(0, kMockCreationDate);
  ASSERT_TRUE(s_mock_creation_date.Run());

  // Get the profile.
  std::unique_ptr<AutofillProfile> db_profile =
      table_->GetAutofillProfile(profile.guid());
  ASSERT_TRUE(db_profile);
  EXPECT_EQ(profile, *db_profile);
  sql::Statement s_original(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM autofill_profiles"));
  ASSERT_TRUE(s_original.is_valid());
  ASSERT_TRUE(s_original.Step());
  EXPECT_EQ(kMockCreationDate, s_original.ColumnInt64(0));
  EXPECT_FALSE(s_original.Step());

  // Now, update the profile and save the update to the database.
  // The modification date should change to reflect the update.
  profile.SetRawInfo(EMAIL_ADDRESS, u"js@smith.xyz");
  table_->UpdateAutofillProfile(profile);

  // Get the profile.
  db_profile = table_->GetAutofillProfile(profile.guid());
  ASSERT_TRUE(db_profile);
  EXPECT_EQ(profile, *db_profile);
  sql::Statement s_updated(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM autofill_profiles"));
  ASSERT_TRUE(s_updated.is_valid());
  ASSERT_TRUE(s_updated.Step());
  EXPECT_LT(kMockCreationDate, s_updated.ColumnInt64(0));
  EXPECT_FALSE(s_updated.Step());

  // Set a mocked value for the profile's modification time.
  const time_t mock_modification_date = AutofillClock::Now().ToTimeT() - 7;
  sql::Statement s_mock_modification_date(
      db_->GetSQLConnection()->GetUniqueStatement(
          "UPDATE autofill_profiles SET date_modified = ?"));
  ASSERT_TRUE(s_mock_modification_date.is_valid());
  s_mock_modification_date.BindInt64(0, mock_modification_date);
  ASSERT_TRUE(s_mock_modification_date.Run());

  // Finally, call into |UpdateAutofillProfile()| without changing the
  // profile.  The modification date should not change.
  table_->UpdateAutofillProfile(profile);

  // Get the profile.
  db_profile = table_->GetAutofillProfile(profile.guid());
  ASSERT_TRUE(db_profile);
  EXPECT_EQ(profile, *db_profile);
  sql::Statement s_unchanged(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM autofill_profiles"));
  ASSERT_TRUE(s_unchanged.is_valid());
  ASSERT_TRUE(s_unchanged.Step());
  EXPECT_EQ(mock_modification_date, s_unchanged.ColumnInt64(0));
  EXPECT_FALSE(s_unchanged.Step());
}

TEST_F(AutofillTableTest, UpdateCreditCard) {
  // Add a credit card to the db.
  CreditCard credit_card;
  credit_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Jack Torrance");
  credit_card.SetRawInfo(CREDIT_CARD_NUMBER, u"1234567890123456");
  credit_card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"04");
  credit_card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2013");
  table_->AddCreditCard(credit_card);

  // Set a mocked value for the credit card's creation time.
  const time_t kMockCreationDate = AutofillClock::Now().ToTimeT() - 13;
  sql::Statement s_mock_creation_date(
      db_->GetSQLConnection()->GetUniqueStatement(
          "UPDATE credit_cards SET date_modified = ?"));
  ASSERT_TRUE(s_mock_creation_date.is_valid());
  s_mock_creation_date.BindInt64(0, kMockCreationDate);
  ASSERT_TRUE(s_mock_creation_date.Run());

  // Get the credit card.
  std::unique_ptr<CreditCard> db_credit_card =
      table_->GetCreditCard(credit_card.guid());
  ASSERT_TRUE(db_credit_card);
  EXPECT_EQ(credit_card, *db_credit_card);
  sql::Statement s_original(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_original.is_valid());
  ASSERT_TRUE(s_original.Step());
  EXPECT_EQ(kMockCreationDate, s_original.ColumnInt64(0));
  EXPECT_FALSE(s_original.Step());

  // Now, update the credit card and save the update to the database.
  // The modification date should change to reflect the update.
  credit_card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"01");
  table_->UpdateCreditCard(credit_card);

  // Get the credit card.
  db_credit_card = table_->GetCreditCard(credit_card.guid());
  ASSERT_TRUE(db_credit_card);
  EXPECT_EQ(credit_card, *db_credit_card);
  sql::Statement s_updated(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_updated.is_valid());
  ASSERT_TRUE(s_updated.Step());
  EXPECT_LT(kMockCreationDate, s_updated.ColumnInt64(0));
  EXPECT_FALSE(s_updated.Step());

  // Set a mocked value for the credit card's modification time.
  const time_t mock_modification_date = AutofillClock::Now().ToTimeT() - 7;
  sql::Statement s_mock_modification_date(
      db_->GetSQLConnection()->GetUniqueStatement(
          "UPDATE credit_cards SET date_modified = ?"));
  ASSERT_TRUE(s_mock_modification_date.is_valid());
  s_mock_modification_date.BindInt64(0, mock_modification_date);
  ASSERT_TRUE(s_mock_modification_date.Run());

  // Finally, call into |UpdateCreditCard()| without changing the credit card.
  // The modification date should not change.
  table_->UpdateCreditCard(credit_card);

  // Get the credit card.
  db_credit_card = table_->GetCreditCard(credit_card.guid());
  ASSERT_TRUE(db_credit_card);
  EXPECT_EQ(credit_card, *db_credit_card);
  sql::Statement s_unchanged(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_unchanged.is_valid());
  ASSERT_TRUE(s_unchanged.Step());
  EXPECT_EQ(mock_modification_date, s_unchanged.ColumnInt64(0));
  EXPECT_FALSE(s_unchanged.Step());
}

TEST_F(AutofillTableTest, UpdateProfileOriginOnly) {
  // Add a profile to the db.
  AutofillProfile profile;
  profile.SetRawInfo(NAME_FIRST, u"John");
  profile.SetRawInfo(NAME_MIDDLE, u"Q.");
  profile.SetRawInfo(NAME_LAST, u"Smith");
  profile.SetRawInfo(EMAIL_ADDRESS, u"js@example.com");
  profile.SetRawInfo(COMPANY_NAME, u"Google");
  profile.SetRawInfo(ADDRESS_HOME_LINE1, u"1234 Apple Way");
  profile.SetRawInfo(ADDRESS_HOME_LINE2, u"unit 5");
  profile.SetRawInfo(ADDRESS_HOME_CITY, u"Los Angeles");
  profile.SetRawInfo(ADDRESS_HOME_STATE, u"CA");
  profile.SetRawInfo(ADDRESS_HOME_ZIP, u"90025");
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"18181234567");
  profile.FinalizeAfterImport();
  table_->AddAutofillProfile(profile);

  // Set a mocked value for the profile's creation time.
  const time_t kMockCreationDate = AutofillClock::Now().ToTimeT() - 13;
  sql::Statement s_mock_creation_date(
      db_->GetSQLConnection()->GetUniqueStatement(
          "UPDATE autofill_profiles SET date_modified = ?"));
  ASSERT_TRUE(s_mock_creation_date.is_valid());
  s_mock_creation_date.BindInt64(0, kMockCreationDate);
  ASSERT_TRUE(s_mock_creation_date.Run());

  // Get the profile.
  std::unique_ptr<AutofillProfile> db_profile =
      table_->GetAutofillProfile(profile.guid());
  ASSERT_TRUE(db_profile);
  EXPECT_EQ(profile, *db_profile);
  sql::Statement s_original(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM autofill_profiles"));
  ASSERT_TRUE(s_original.is_valid());
  ASSERT_TRUE(s_original.Step());
  EXPECT_EQ(kMockCreationDate, s_original.ColumnInt64(0));
  EXPECT_FALSE(s_original.Step());

  // Now, update just the profile's origin and save the update to the database.
  // The modification date should change to reflect the update.
  profile.set_origin("https://www.example.com/");
  table_->UpdateAutofillProfile(profile);

  // Get the profile.
  db_profile = table_->GetAutofillProfile(profile.guid());
  ASSERT_TRUE(db_profile);
  EXPECT_EQ(profile, *db_profile);
  sql::Statement s_updated(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM autofill_profiles"));
  ASSERT_TRUE(s_updated.is_valid());
  ASSERT_TRUE(s_updated.Step());
  EXPECT_LT(kMockCreationDate, s_updated.ColumnInt64(0));
  EXPECT_FALSE(s_updated.Step());
}

TEST_F(AutofillTableTest, UpdateCreditCardOriginOnly) {
  // Add a credit card to the db.
  CreditCard credit_card;
  credit_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Jack Torrance");
  credit_card.SetRawInfo(CREDIT_CARD_NUMBER, u"1234567890123456");
  credit_card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"04");
  credit_card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2013");
  table_->AddCreditCard(credit_card);

  // Set a mocked value for the credit card's creation time.
  const time_t kMockCreationDate = AutofillClock::Now().ToTimeT() - 13;
  sql::Statement s_mock_creation_date(
      db_->GetSQLConnection()->GetUniqueStatement(
          "UPDATE credit_cards SET date_modified = ?"));
  ASSERT_TRUE(s_mock_creation_date.is_valid());
  s_mock_creation_date.BindInt64(0, kMockCreationDate);
  ASSERT_TRUE(s_mock_creation_date.Run());

  // Get the credit card.
  std::unique_ptr<CreditCard> db_credit_card =
      table_->GetCreditCard(credit_card.guid());
  ASSERT_TRUE(db_credit_card);
  EXPECT_EQ(credit_card, *db_credit_card);
  sql::Statement s_original(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_original.is_valid());
  ASSERT_TRUE(s_original.Step());
  EXPECT_EQ(kMockCreationDate, s_original.ColumnInt64(0));
  EXPECT_FALSE(s_original.Step());

  // Now, update just the credit card's origin and save the update to the
  // database.  The modification date should change to reflect the update.
  credit_card.set_origin("https://www.example.com/");
  table_->UpdateCreditCard(credit_card);

  // Get the credit card.
  db_credit_card = table_->GetCreditCard(credit_card.guid());
  ASSERT_TRUE(db_credit_card);
  EXPECT_EQ(credit_card, *db_credit_card);
  sql::Statement s_updated(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_updated.is_valid());
  ASSERT_TRUE(s_updated.Step());
  EXPECT_LT(kMockCreationDate, s_updated.ColumnInt64(0));
  EXPECT_FALSE(s_updated.Step());
}

TEST_F(AutofillTableTest, RemoveAutofillDataModifiedBetween) {
  // Populate the autofill_profiles and credit_cards tables.
  ASSERT_TRUE(db_->GetSQLConnection()->Execute(
      "INSERT INTO autofill_profiles (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000000', 11);"
      "INSERT INTO autofill_profile_names (guid, full_name) "
      "VALUES('00000000-0000-0000-0000-000000000000', 'John Jones');"
      "INSERT INTO autofill_profile_emails (guid, email) "
      "VALUES('00000000-0000-0000-0000-000000000000', 'john@jones.com');"
      "INSERT INTO autofill_profile_phones (guid, number) "
      "VALUES('00000000-0000-0000-0000-000000000000', '111-111-1111');"
      "INSERT INTO autofill_profiles (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000001', 21);"
      "INSERT INTO autofill_profile_names (guid, full_name) "
      "VALUES('00000000-0000-0000-0000-000000000001', 'John Jones2');"
      "INSERT INTO autofill_profile_emails (guid, email) "
      "VALUES('00000000-0000-0000-0000-000000000001', 'john@jones2.com');"
      "INSERT INTO autofill_profile_phones (guid, number) "
      "VALUES('00000000-0000-0000-0000-000000000001', '222-222-2222');"
      "INSERT INTO autofill_profiles (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000002', 31);"
      "INSERT INTO autofill_profile_names (guid, full_name) "
      "VALUES('00000000-0000-0000-0000-000000000002', 'John Jones3');"
      "INSERT INTO autofill_profile_emails (guid, email) "
      "VALUES('00000000-0000-0000-0000-000000000002', 'john@jones3.com');"
      "INSERT INTO autofill_profile_phones (guid, number) "
      "VALUES('00000000-0000-0000-0000-000000000002', '333-333-3333');"
      "INSERT INTO autofill_profiles (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000003', 41);"
      "INSERT INTO autofill_profile_names (guid, full_name) "
      "VALUES('00000000-0000-0000-0000-000000000003', 'John Jones4');"
      "INSERT INTO autofill_profile_emails (guid, email) "
      "VALUES('00000000-0000-0000-0000-000000000003', 'john@jones4.com');"
      "INSERT INTO autofill_profile_phones (guid, number) "
      "VALUES('00000000-0000-0000-0000-000000000003', '444-444-4444');"
      "INSERT INTO autofill_profiles (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000004', 51);"
      "INSERT INTO autofill_profile_names (guid, full_name) "
      "VALUES('00000000-0000-0000-0000-000000000004', 'John Jones5');"
      "INSERT INTO autofill_profile_emails (guid, email) "
      "VALUES('00000000-0000-0000-0000-000000000004', 'john@jones5.com');"
      "INSERT INTO autofill_profile_phones (guid, number) "
      "VALUES('00000000-0000-0000-0000-000000000004', '555-555-5555');"
      "INSERT INTO autofill_profiles (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000005', 61);"
      "INSERT INTO autofill_profile_names (guid, full_name) "
      "VALUES('00000000-0000-0000-0000-000000000005', 'John Jones6');"
      "INSERT INTO autofill_profile_emails (guid, email) "
      "VALUES('00000000-0000-0000-0000-000000000005', 'john@jones6.com');"
      "INSERT INTO autofill_profile_phones (guid, number) "
      "VALUES('00000000-0000-0000-0000-000000000005', '666-666-6666');"
      "INSERT INTO credit_cards (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000006', 17);"
      "INSERT INTO credit_cards (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000007', 27);"
      "INSERT INTO credit_cards (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000008', 37);"
      "INSERT INTO credit_cards (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000009', 47);"
      "INSERT INTO credit_cards (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000010', 57);"
      "INSERT INTO credit_cards (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000011', 67);"));

  // Remove all entries modified in the bounded time range [17,41).
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  std::vector<std::unique_ptr<CreditCard>> credit_cards;
  table_->RemoveAutofillDataModifiedBetween(
      Time::FromTimeT(17), Time::FromTimeT(41), &profiles, &credit_cards);

  // Two profiles should have been removed.
  ASSERT_EQ(2UL, profiles.size());
  EXPECT_EQ("00000000-0000-0000-0000-000000000001", profiles[0]->guid());
  EXPECT_EQ("00000000-0000-0000-0000-000000000002", profiles[1]->guid());

  // Make sure that only the expected profiles are still present.
  sql::Statement s_autofill_profiles_bounded(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT date_modified FROM autofill_profiles"));
  ASSERT_TRUE(s_autofill_profiles_bounded.is_valid());
  ASSERT_TRUE(s_autofill_profiles_bounded.Step());
  EXPECT_EQ(11, s_autofill_profiles_bounded.ColumnInt64(0));
  ASSERT_TRUE(s_autofill_profiles_bounded.Step());
  EXPECT_EQ(41, s_autofill_profiles_bounded.ColumnInt64(0));
  ASSERT_TRUE(s_autofill_profiles_bounded.Step());
  EXPECT_EQ(51, s_autofill_profiles_bounded.ColumnInt64(0));
  ASSERT_TRUE(s_autofill_profiles_bounded.Step());
  EXPECT_EQ(61, s_autofill_profiles_bounded.ColumnInt64(0));
  EXPECT_FALSE(s_autofill_profiles_bounded.Step());

  // Make sure that only the expected profile names are still present.
  sql::Statement s_autofill_profile_names_bounded(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT full_name FROM autofill_profile_names"));
  ASSERT_TRUE(s_autofill_profile_names_bounded.is_valid());
  ASSERT_TRUE(s_autofill_profile_names_bounded.Step());
  EXPECT_EQ("John Jones", s_autofill_profile_names_bounded.ColumnString(0));
  ASSERT_TRUE(s_autofill_profile_names_bounded.Step());
  EXPECT_EQ("John Jones4", s_autofill_profile_names_bounded.ColumnString(0));
  ASSERT_TRUE(s_autofill_profile_names_bounded.Step());
  EXPECT_EQ("John Jones5", s_autofill_profile_names_bounded.ColumnString(0));
  ASSERT_TRUE(s_autofill_profile_names_bounded.Step());
  EXPECT_EQ("John Jones6", s_autofill_profile_names_bounded.ColumnString(0));
  EXPECT_FALSE(s_autofill_profile_names_bounded.Step());

  // Make sure that only the expected profile emails are still present.
  sql::Statement s_autofill_profile_emails_bounded(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT email FROM autofill_profile_emails"));
  ASSERT_TRUE(s_autofill_profile_emails_bounded.is_valid());
  ASSERT_TRUE(s_autofill_profile_emails_bounded.Step());
  EXPECT_EQ("john@jones.com",
            s_autofill_profile_emails_bounded.ColumnString(0));
  ASSERT_TRUE(s_autofill_profile_emails_bounded.Step());
  EXPECT_EQ("john@jones4.com",
            s_autofill_profile_emails_bounded.ColumnString(0));
  ASSERT_TRUE(s_autofill_profile_emails_bounded.Step());
  EXPECT_EQ("john@jones5.com",
            s_autofill_profile_emails_bounded.ColumnString(0));
  ASSERT_TRUE(s_autofill_profile_emails_bounded.Step());
  EXPECT_EQ("john@jones6.com",
            s_autofill_profile_emails_bounded.ColumnString(0));
  EXPECT_FALSE(s_autofill_profile_emails_bounded.Step());

  // Make sure the expected profile phone numbers are still present.
  sql::Statement s_autofill_profile_phones_bounded(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT number FROM autofill_profile_phones"));
  ASSERT_TRUE(s_autofill_profile_phones_bounded.is_valid());
  ASSERT_TRUE(s_autofill_profile_phones_bounded.Step());
  EXPECT_EQ("111-111-1111", s_autofill_profile_phones_bounded.ColumnString(0));
  ASSERT_TRUE(s_autofill_profile_phones_bounded.Step());
  EXPECT_EQ("444-444-4444", s_autofill_profile_phones_bounded.ColumnString(0));
  ASSERT_TRUE(s_autofill_profile_phones_bounded.Step());
  EXPECT_EQ("555-555-5555", s_autofill_profile_phones_bounded.ColumnString(0));
  ASSERT_TRUE(s_autofill_profile_phones_bounded.Step());
  EXPECT_EQ("666-666-6666", s_autofill_profile_phones_bounded.ColumnString(0));
  EXPECT_FALSE(s_autofill_profile_phones_bounded.Step());

  // Three cards should have been removed.
  ASSERT_EQ(3UL, credit_cards.size());
  EXPECT_EQ("00000000-0000-0000-0000-000000000006", credit_cards[0]->guid());
  EXPECT_EQ("00000000-0000-0000-0000-000000000007", credit_cards[1]->guid());
  EXPECT_EQ("00000000-0000-0000-0000-000000000008", credit_cards[2]->guid());

  // Make sure the expected profiles are still present.
  sql::Statement s_credit_cards_bounded(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_credit_cards_bounded.is_valid());
  ASSERT_TRUE(s_credit_cards_bounded.Step());
  EXPECT_EQ(47, s_credit_cards_bounded.ColumnInt64(0));
  ASSERT_TRUE(s_credit_cards_bounded.Step());
  EXPECT_EQ(57, s_credit_cards_bounded.ColumnInt64(0));
  ASSERT_TRUE(s_credit_cards_bounded.Step());
  EXPECT_EQ(67, s_credit_cards_bounded.ColumnInt64(0));
  EXPECT_FALSE(s_credit_cards_bounded.Step());

  // Remove all entries modified on or after time 51 (unbounded range).
  table_->RemoveAutofillDataModifiedBetween(Time::FromTimeT(51), Time(),
                                            &profiles, &credit_cards);
  ASSERT_EQ(2UL, profiles.size());
  EXPECT_EQ("00000000-0000-0000-0000-000000000004", profiles[0]->guid());
  EXPECT_EQ("00000000-0000-0000-0000-000000000005", profiles[1]->guid());

  // Make sure that only the expected profile names are still present.
  sql::Statement s_autofill_profiles_unbounded(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT date_modified FROM autofill_profiles"));
  ASSERT_TRUE(s_autofill_profiles_unbounded.is_valid());
  ASSERT_TRUE(s_autofill_profiles_unbounded.Step());
  EXPECT_EQ(11, s_autofill_profiles_unbounded.ColumnInt64(0));
  ASSERT_TRUE(s_autofill_profiles_unbounded.Step());
  EXPECT_EQ(41, s_autofill_profiles_unbounded.ColumnInt64(0));
  EXPECT_FALSE(s_autofill_profiles_unbounded.Step());

  // Make sure that only the expected profile names are still present.
  sql::Statement s_autofill_profile_names_unbounded(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT full_name FROM autofill_profile_names"));
  ASSERT_TRUE(s_autofill_profile_names_unbounded.is_valid());
  ASSERT_TRUE(s_autofill_profile_names_unbounded.Step());
  EXPECT_EQ("John Jones", s_autofill_profile_names_unbounded.ColumnString(0));
  ASSERT_TRUE(s_autofill_profile_names_unbounded.Step());
  EXPECT_EQ("John Jones4", s_autofill_profile_names_unbounded.ColumnString(0));
  EXPECT_FALSE(s_autofill_profile_names_unbounded.Step());

  // Make sure that only the expected profile emails are still present.
  sql::Statement s_autofill_profile_emails_unbounded(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT email FROM autofill_profile_emails"));
  ASSERT_TRUE(s_autofill_profile_emails_unbounded.is_valid());
  ASSERT_TRUE(s_autofill_profile_emails_unbounded.Step());
  EXPECT_EQ("john@jones.com",
            s_autofill_profile_emails_unbounded.ColumnString(0));
  ASSERT_TRUE(s_autofill_profile_emails_unbounded.Step());
  EXPECT_EQ("john@jones4.com",
            s_autofill_profile_emails_unbounded.ColumnString(0));
  EXPECT_FALSE(s_autofill_profile_emails_unbounded.Step());

  // Make sure the expected profile phone numbers are still present.
  sql::Statement s_autofill_profile_phones_unbounded(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT number FROM autofill_profile_phones"));
  ASSERT_TRUE(s_autofill_profile_phones_unbounded.is_valid());
  ASSERT_TRUE(s_autofill_profile_phones_unbounded.Step());
  EXPECT_EQ("111-111-1111",
            s_autofill_profile_phones_unbounded.ColumnString(0));
  ASSERT_TRUE(s_autofill_profile_phones_unbounded.Step());
  EXPECT_EQ("444-444-4444",
            s_autofill_profile_phones_unbounded.ColumnString(0));
  EXPECT_FALSE(s_autofill_profile_phones_unbounded.Step());

  // Two cards should have been removed.
  ASSERT_EQ(2UL, credit_cards.size());
  EXPECT_EQ("00000000-0000-0000-0000-000000000010", credit_cards[0]->guid());
  EXPECT_EQ("00000000-0000-0000-0000-000000000011", credit_cards[1]->guid());

  // Make sure the remaining card is the expected one.
  sql::Statement s_credit_cards_unbounded(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_credit_cards_unbounded.is_valid());
  ASSERT_TRUE(s_credit_cards_unbounded.Step());
  EXPECT_EQ(47, s_credit_cards_unbounded.ColumnInt64(0));
  EXPECT_FALSE(s_credit_cards_unbounded.Step());

  // Remove all remaining entries.
  table_->RemoveAutofillDataModifiedBetween(Time(), Time(), &profiles,
                                            &credit_cards);

  // Two profiles should have been removed.
  ASSERT_EQ(2UL, profiles.size());
  EXPECT_EQ("00000000-0000-0000-0000-000000000000", profiles[0]->guid());
  EXPECT_EQ("00000000-0000-0000-0000-000000000003", profiles[1]->guid());

  // Make sure there are no profiles remaining.
  sql::Statement s_autofill_profiles_empty(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT date_modified FROM autofill_profiles"));
  ASSERT_TRUE(s_autofill_profiles_empty.is_valid());
  EXPECT_FALSE(s_autofill_profiles_empty.Step());

  // Make sure there are no profile names remaining.
  sql::Statement s_autofill_profile_names_empty(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT full_name FROM autofill_profile_names"));
  ASSERT_TRUE(s_autofill_profile_names_empty.is_valid());
  EXPECT_FALSE(s_autofill_profile_names_empty.Step());

  // Make sure there are no profile emails remaining.
  sql::Statement s_autofill_profile_emails_empty(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT email FROM autofill_profile_emails"));
  ASSERT_TRUE(s_autofill_profile_emails_empty.is_valid());
  EXPECT_FALSE(s_autofill_profile_emails_empty.Step());

  // Make sure there are no profile phones remaining.
  sql::Statement s_autofill_profile_phones_empty(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT number FROM autofill_profile_phones"));
  ASSERT_TRUE(s_autofill_profile_phones_empty.is_valid());
  EXPECT_FALSE(s_autofill_profile_phones_empty.Step());

  // One credit card should have been deleted.
  ASSERT_EQ(1UL, credit_cards.size());
  EXPECT_EQ("00000000-0000-0000-0000-000000000009", credit_cards[0]->guid());

  // There should be no cards left.
  sql::Statement s_credit_cards_empty(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_credit_cards_empty.is_valid());
  EXPECT_FALSE(s_credit_cards_empty.Step());
}

TEST_F(AutofillTableTest, RemoveOriginURLsModifiedBetween) {
  // Populate the autofill_profiles and credit_cards tables.
  ASSERT_TRUE(db_->GetSQLConnection()->Execute(
      "INSERT INTO autofill_profiles (guid, origin, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000000', '', 11);"
      "INSERT INTO autofill_profiles (guid, origin, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000001', "
      "       'https://www.example.com/', 21);"
      "INSERT INTO autofill_profiles (guid, origin, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000002', 'Chrome settings', 31);"
      "INSERT INTO credit_cards (guid, origin, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000003', '', 17);"
      "INSERT INTO credit_cards (guid, origin, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000004', "
      "       'https://www.example.com/', 27);"
      "INSERT INTO credit_cards (guid, origin, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000005', 'Chrome settings', "
      "       37);"));

  // Remove all origin URLs set in the bounded time range [21,27).
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  table_->RemoveOriginURLsModifiedBetween(Time::FromTimeT(21),
                                          Time::FromTimeT(27), &profiles);
  ASSERT_EQ(1UL, profiles.size());
  EXPECT_EQ("00000000-0000-0000-0000-000000000001", profiles[0]->guid());
  sql::Statement s_autofill_profiles_bounded(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT date_modified, origin FROM autofill_profiles"));
  ASSERT_TRUE(s_autofill_profiles_bounded.is_valid());
  ASSERT_TRUE(s_autofill_profiles_bounded.Step());
  EXPECT_EQ(11, s_autofill_profiles_bounded.ColumnInt64(0));
  EXPECT_EQ(std::string(), s_autofill_profiles_bounded.ColumnString(1));
  ASSERT_TRUE(s_autofill_profiles_bounded.Step());
  EXPECT_EQ(21, s_autofill_profiles_bounded.ColumnInt64(0));
  EXPECT_EQ(std::string(), s_autofill_profiles_bounded.ColumnString(1));
  ASSERT_TRUE(s_autofill_profiles_bounded.Step());
  EXPECT_EQ(31, s_autofill_profiles_bounded.ColumnInt64(0));
  EXPECT_EQ(kSettingsOrigin, s_autofill_profiles_bounded.ColumnString(1));
  sql::Statement s_credit_cards_bounded(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT date_modified, origin FROM credit_cards"));
  ASSERT_TRUE(s_credit_cards_bounded.is_valid());
  ASSERT_TRUE(s_credit_cards_bounded.Step());
  EXPECT_EQ(17, s_credit_cards_bounded.ColumnInt64(0));
  EXPECT_EQ(std::string(), s_credit_cards_bounded.ColumnString(1));
  ASSERT_TRUE(s_credit_cards_bounded.Step());
  EXPECT_EQ(27, s_credit_cards_bounded.ColumnInt64(0));
  EXPECT_EQ("https://www.example.com/", s_credit_cards_bounded.ColumnString(1));
  ASSERT_TRUE(s_credit_cards_bounded.Step());
  EXPECT_EQ(37, s_credit_cards_bounded.ColumnInt64(0));
  EXPECT_EQ(kSettingsOrigin, s_credit_cards_bounded.ColumnString(1));

  // Remove all origin URLS.
  profiles.clear();
  table_->RemoveOriginURLsModifiedBetween(Time(), Time(), &profiles);
  EXPECT_EQ(0UL, profiles.size());
  sql::Statement s_autofill_profiles_all(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT date_modified, origin FROM autofill_profiles"));
  ASSERT_TRUE(s_autofill_profiles_all.is_valid());
  ASSERT_TRUE(s_autofill_profiles_all.Step());
  EXPECT_EQ(11, s_autofill_profiles_all.ColumnInt64(0));
  EXPECT_EQ(std::string(), s_autofill_profiles_all.ColumnString(1));
  ASSERT_TRUE(s_autofill_profiles_all.Step());
  EXPECT_EQ(21, s_autofill_profiles_all.ColumnInt64(0));
  EXPECT_EQ(std::string(), s_autofill_profiles_all.ColumnString(1));
  ASSERT_TRUE(s_autofill_profiles_all.Step());
  EXPECT_EQ(31, s_autofill_profiles_all.ColumnInt64(0));
  EXPECT_EQ(kSettingsOrigin, s_autofill_profiles_all.ColumnString(1));
  sql::Statement s_credit_cards_all(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified, origin FROM credit_cards"));
  ASSERT_TRUE(s_credit_cards_all.is_valid());
  ASSERT_TRUE(s_credit_cards_all.Step());
  EXPECT_EQ(17, s_credit_cards_all.ColumnInt64(0));
  EXPECT_EQ(std::string(), s_credit_cards_all.ColumnString(1));
  ASSERT_TRUE(s_credit_cards_all.Step());
  EXPECT_EQ(27, s_credit_cards_all.ColumnInt64(0));
  EXPECT_EQ(std::string(), s_credit_cards_all.ColumnString(1));
  ASSERT_TRUE(s_credit_cards_all.Step());
  EXPECT_EQ(37, s_credit_cards_all.ColumnInt64(0));
  EXPECT_EQ(kSettingsOrigin, s_credit_cards_all.ColumnString(1));
}

TEST_F(AutofillTableTest, Autofill_GetAllAutofillEntries_NoResults) {
  std::vector<AutofillEntry> entries;
  ASSERT_TRUE(table_->GetAllAutofillEntries(&entries));

  EXPECT_EQ(0U, entries.size());
}

TEST_F(AutofillTableTest, Autofill_GetAllAutofillEntries_OneResult) {
  AutofillChangeList changes;
  std::map<std::string, std::vector<Time>> name_value_times_map;

  time_t start = 0;
  std::vector<Time> timestamps1;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  EXPECT_TRUE(
      table_->AddFormFieldValueTime(field, &changes, Time::FromTimeT(start)));
  timestamps1.push_back(Time::FromTimeT(start));
  std::string key1("NameSuperman");
  name_value_times_map.insert(
      std::pair<std::string, std::vector<Time>>(key1, timestamps1));

  AutofillEntrySet expected_entries(CompareAutofillEntries);
  AutofillKey ak1(u"Name", u"Superman");
  AutofillEntry ae1(ak1, timestamps1.front(), timestamps1.back());

  expected_entries.insert(ae1);

  std::vector<AutofillEntry> entries;
  ASSERT_TRUE(table_->GetAllAutofillEntries(&entries));
  AutofillEntrySet entry_set(entries.begin(), entries.end(),
                             CompareAutofillEntries);

  CompareAutofillEntrySets(entry_set, expected_entries);
}

TEST_F(AutofillTableTest, Autofill_GetAllAutofillEntries_TwoDistinct) {
  AutofillChangeList changes;
  std::map<std::string, std::vector<Time>> name_value_times_map;
  time_t start = 0;

  std::vector<Time> timestamps1;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  EXPECT_TRUE(
      table_->AddFormFieldValueTime(field, &changes, Time::FromTimeT(start)));
  timestamps1.push_back(Time::FromTimeT(start));
  std::string key1("NameSuperman");
  name_value_times_map.insert(
      std::pair<std::string, std::vector<Time>>(key1, timestamps1));

  ++start;
  std::vector<Time> timestamps2;
  field.name = u"Name";
  field.value = u"Clark Kent";
  EXPECT_TRUE(
      table_->AddFormFieldValueTime(field, &changes, Time::FromTimeT(start)));
  timestamps2.push_back(Time::FromTimeT(start));
  std::string key2("NameClark Kent");
  name_value_times_map.insert(
      std::pair<std::string, std::vector<Time>>(key2, timestamps2));

  AutofillEntrySet expected_entries(CompareAutofillEntries);
  AutofillKey ak1(u"Name", u"Superman");
  AutofillKey ak2(u"Name", u"Clark Kent");
  AutofillEntry ae1(ak1, timestamps1.front(), timestamps1.back());
  AutofillEntry ae2(ak2, timestamps2.front(), timestamps2.back());

  expected_entries.insert(ae1);
  expected_entries.insert(ae2);

  std::vector<AutofillEntry> entries;
  ASSERT_TRUE(table_->GetAllAutofillEntries(&entries));
  AutofillEntrySet entry_set(entries.begin(), entries.end(),
                             CompareAutofillEntries);

  CompareAutofillEntrySets(entry_set, expected_entries);
}

TEST_F(AutofillTableTest, Autofill_GetAllAutofillEntries_TwoSame) {
  AutofillChangeList changes;
  std::map<std::string, std::vector<Time>> name_value_times_map;

  std::vector<Time> timestamps;
  time_t start = 0;
  for (int i = 0; i < 2; ++i, ++start) {
    FormFieldData field;
    field.name = u"Name";
    field.value = u"Superman";
    EXPECT_TRUE(
        table_->AddFormFieldValueTime(field, &changes, Time::FromTimeT(start)));
    timestamps.push_back(Time::FromTimeT(start));
  }

  std::string key("NameSuperman");
  name_value_times_map.insert(
      std::pair<std::string, std::vector<Time>>(key, timestamps));

  AutofillEntrySet expected_entries(CompareAutofillEntries);
  AutofillKey ak1(u"Name", u"Superman");
  AutofillEntry ae1(ak1, timestamps.front(), timestamps.back());

  expected_entries.insert(ae1);

  std::vector<AutofillEntry> entries;
  ASSERT_TRUE(table_->GetAllAutofillEntries(&entries));
  AutofillEntrySet entry_set(entries.begin(), entries.end(),
                             CompareAutofillEntries);

  CompareAutofillEntrySets(entry_set, expected_entries);
}

TEST_F(AutofillTableTest, AutofillProfileValidityBitfield) {
  // Add an autofill profile with a non default validity state. The value itself
  // is insignificant for this test since only the serialization and
  // deserialization are tested.
  const int kValidityBitfieldValue = 1984;
  AutofillProfile profile;
  profile.set_origin(std::string());
  profile.SetRawInfo(NAME_FIRST, u"John");
  profile.SetRawInfo(NAME_LAST, u"Smith");
  profile.SetClientValidityFromBitfieldValue(kValidityBitfieldValue);

  // Add the profile to the table.
  EXPECT_TRUE(table_->AddAutofillProfile(profile));

  // Get the profile from the table and make sure the validity was set.
  std::unique_ptr<AutofillProfile> db_profile =
      table_->GetAutofillProfile(profile.guid());
  ASSERT_TRUE(db_profile);
  EXPECT_EQ(kValidityBitfieldValue,
            db_profile->GetClientValidityBitfieldValue());

  // Modify the validity of the profile.
  const int kOtherValidityBitfieldValue = 1999;
  profile.SetClientValidityFromBitfieldValue(kOtherValidityBitfieldValue);

  // Update the profile in the table.
  EXPECT_TRUE(table_->UpdateAutofillProfile(profile));

  // Get the profile from the table and make sure the validity was updated.
  db_profile = table_->GetAutofillProfile(profile.guid());
  ASSERT_TRUE(db_profile);
  EXPECT_EQ(kOtherValidityBitfieldValue,
            db_profile->GetClientValidityBitfieldValue());
}

TEST_F(AutofillTableTest, AutofillProfileIsClientValidityStatesUpdatedFlag) {
  AutofillProfile profile;
  profile.set_origin(std::string());
  profile.SetRawInfo(NAME_FIRST, u"John");
  profile.SetRawInfo(NAME_LAST, u"Smith");
  profile.set_is_client_validity_states_updated(true);

  // Add the profile to the table.
  EXPECT_TRUE(table_->AddAutofillProfile(profile));
  // Get the profile from the table and make sure the validity was set.
  std::unique_ptr<AutofillProfile> db_profile =
      table_->GetAutofillProfile(profile.guid());
  ASSERT_TRUE(db_profile);
  EXPECT_TRUE(db_profile->is_client_validity_states_updated());

  // Test if turning off the validity updated flag works.
  profile.set_is_client_validity_states_updated(false);
  // Update the profile in the table.
  EXPECT_TRUE(table_->UpdateAutofillProfile(profile));
  // Get the profile from the table and make sure the validity was updated.
  db_profile = table_->GetAutofillProfile(profile.guid());
  ASSERT_TRUE(db_profile);
  EXPECT_FALSE(db_profile->is_client_validity_states_updated());

  // Test if turning on the validity updated flag works.
  profile.set_is_client_validity_states_updated(true);
  // Update the profile in the table.
  EXPECT_TRUE(table_->UpdateAutofillProfile(profile));
  // Get the profile from the table and make sure the validity was updated.
  db_profile = table_->GetAutofillProfile(profile.guid());
  ASSERT_TRUE(db_profile);
  EXPECT_TRUE(db_profile->is_client_validity_states_updated());
}

TEST_F(AutofillTableTest, SetGetServerCards) {
  std::vector<CreditCard> inputs;
  inputs.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "a123"));
  inputs[0].SetRawInfo(CREDIT_CARD_NAME_FULL, u"Paul F. Tompkins");
  inputs[0].SetRawInfo(CREDIT_CARD_EXP_MONTH, u"1");
  inputs[0].SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2020");
  inputs[0].SetRawInfo(CREDIT_CARD_NUMBER, u"4111111111111111");
  inputs[0].set_instrument_id(321);
  inputs[0].set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::UNENROLLED);

  inputs.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "b456"));
  inputs[1].SetRawInfo(CREDIT_CARD_NAME_FULL, u"Rick Roman");
  inputs[1].SetRawInfo(CREDIT_CARD_EXP_MONTH, u"12");
  inputs[1].SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"1997");
  inputs[1].SetRawInfo(CREDIT_CARD_NUMBER, u"1111");
  inputs[1].SetNetworkForMaskedCard(kVisaCard);
  std::u16string nickname = u"Grocery card";
  inputs[1].SetNickname(nickname);
  inputs[1].set_card_issuer(CreditCard::Issuer::GOOGLE);
  inputs[1].set_instrument_id(123);
  inputs[1].set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::ENROLLED);
  inputs[1].set_card_art_url(GURL("https://www.example.com"));

  test::SetServerCreditCards(table_.get(), inputs);

  std::vector<std::unique_ptr<CreditCard>> outputs;
  ASSERT_TRUE(table_->GetServerCreditCards(&outputs));
  ASSERT_EQ(inputs.size(), outputs.size());

  // Ordering isn't guaranteed, so fix the ordering if it's backwards.
  if (outputs[1]->server_id() == inputs[0].server_id())
    std::swap(outputs[0], outputs[1]);

  // GUIDs for server cards are dynamically generated so will be different
  // after reading from the DB. Check they're valid, but otherwise don't count
  // them in the comparison.
  inputs[0].set_guid(std::string());
  inputs[1].set_guid(std::string());
  outputs[0]->set_guid(std::string());
  outputs[1]->set_guid(std::string());

  EXPECT_EQ(inputs[0], *outputs[0]);
  EXPECT_EQ(inputs[1], *outputs[1]);

  EXPECT_TRUE(outputs[0]->nickname().empty());
  EXPECT_EQ(nickname, outputs[1]->nickname());

  EXPECT_EQ(CreditCard::Issuer::ISSUER_UNKNOWN, outputs[0]->card_issuer());
  EXPECT_EQ(CreditCard::Issuer::GOOGLE, outputs[1]->card_issuer());

  EXPECT_EQ(321, outputs[0]->instrument_id());
  EXPECT_EQ(123, outputs[1]->instrument_id());

  EXPECT_EQ(CreditCard::VirtualCardEnrollmentState::UNENROLLED,
            outputs[0]->virtual_card_enrollment_state());
  EXPECT_EQ(CreditCard::VirtualCardEnrollmentState::ENROLLED,
            outputs[1]->virtual_card_enrollment_state());

  EXPECT_EQ(GURL(), outputs[0]->card_art_url());
  EXPECT_EQ(GURL("https://www.example.com"), outputs[1]->card_art_url());
}

TEST_F(AutofillTableTest, SetGetRemoveServerCardMetadata) {
  // Create and set the metadata.
  AutofillMetadata input;
  input.id = "server id";
  input.use_count = 50;
  input.use_date = AutofillClock::Now();
  input.billing_address_id = "billing id";
  EXPECT_TRUE(table_->AddServerCardMetadata(input));

  // Make sure it was added correctly.
  std::map<std::string, AutofillMetadata> outputs;
  ASSERT_TRUE(table_->GetServerCardsMetadata(&outputs));
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ(input, outputs[input.id]);

  // Remove the metadata from the table.
  EXPECT_TRUE(table_->RemoveServerCardMetadata(input.id));

  // Make sure it was removed correctly.
  ASSERT_TRUE(table_->GetServerCardsMetadata(&outputs));
  EXPECT_EQ(0U, outputs.size());
}

TEST_F(AutofillTableTest, SetGetRemoveServerAddressMetadata) {
  // Create and set the metadata.
  AutofillMetadata input;
  input.id = "server id";
  input.use_count = 50;
  input.use_date = AutofillClock::Now();
  input.has_converted = true;
  table_->AddServerAddressMetadata(input);

  // Make sure it was added correctly.
  std::map<std::string, AutofillMetadata> outputs;
  ASSERT_TRUE(table_->GetServerAddressesMetadata(&outputs));
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ(input, outputs[input.id]);

  // Remove the metadata from the table.
  EXPECT_TRUE(table_->RemoveServerAddressMetadata(input.id));

  // Make sure it was removed correctly.
  ASSERT_TRUE(table_->GetServerAddressesMetadata(&outputs));
  EXPECT_EQ(0U, outputs.size());
}

TEST_F(AutofillTableTest, AddUpdateServerAddressMetadata) {
  // Create and set the metadata.
  AutofillMetadata input;
  input.id = "server id";
  input.use_count = 50;
  input.use_date = AutofillClock::Now();
  input.has_converted = true;
  ASSERT_TRUE(table_->AddServerAddressMetadata(input));

  // Make sure it was added correctly.
  std::map<std::string, AutofillMetadata> outputs;
  ASSERT_TRUE(table_->GetServerAddressesMetadata(&outputs));
  ASSERT_EQ(1U, outputs.size());
  ASSERT_EQ(input, outputs[input.id]);

  // Update the metadata in the table.
  input.use_count = 51;
  EXPECT_TRUE(table_->UpdateServerAddressMetadata(input));

  // Make sure it was updated correctly.
  ASSERT_TRUE(table_->GetServerAddressesMetadata(&outputs));
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ(input, outputs[input.id]);

  // Insert a new entry using update - that should also be legal.
  input.id = "another server id";
  EXPECT_TRUE(table_->UpdateServerAddressMetadata(input));
  ASSERT_TRUE(table_->GetServerAddressesMetadata(&outputs));
  ASSERT_EQ(2U, outputs.size());
}

TEST_F(AutofillTableTest, AddUpdateServerCardMetadata) {
  // Create and set the metadata.
  AutofillMetadata input;
  input.id = "server id";
  input.use_count = 50;
  input.use_date = AutofillClock::Now();
  input.billing_address_id = "billing id";
  ASSERT_TRUE(table_->AddServerCardMetadata(input));

  // Make sure it was added correctly.
  std::map<std::string, AutofillMetadata> outputs;
  ASSERT_TRUE(table_->GetServerCardsMetadata(&outputs));
  ASSERT_EQ(1U, outputs.size());
  ASSERT_EQ(input, outputs[input.id]);

  // Update the metadata in the table.
  input.use_count = 51;
  EXPECT_TRUE(table_->UpdateServerCardMetadata(input));

  // Make sure it was updated correctly.
  ASSERT_TRUE(table_->GetServerCardsMetadata(&outputs));
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ(input, outputs[input.id]);

  // Insert a new entry using update - that should also be legal.
  input.id = "another server id";
  EXPECT_TRUE(table_->UpdateServerCardMetadata(input));
  ASSERT_TRUE(table_->GetServerCardsMetadata(&outputs));
  ASSERT_EQ(2U, outputs.size());
}

TEST_F(AutofillTableTest, UpdateServerAddressMetadataDoesNotChangeData) {
  AutofillProfile one(AutofillProfile::SERVER_PROFILE, "a123");
  std::vector<AutofillProfile> inputs;
  inputs.push_back(one);
  table_->SetServerProfiles(inputs);

  std::vector<std::unique_ptr<AutofillProfile>> outputs;
  table_->GetServerProfiles(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(one.server_id(), outputs[0]->server_id());

  // Update metadata in the profile.
  ASSERT_NE(outputs[0]->use_count(), 51u);
  outputs[0]->set_use_count(51);

  AutofillMetadata input_metadata = outputs[0]->GetMetadata();
  EXPECT_TRUE(table_->UpdateServerAddressMetadata(input_metadata));

  // Make sure it was updated correctly.
  std::map<std::string, AutofillMetadata> output_metadata;
  ASSERT_TRUE(table_->GetServerAddressesMetadata(&output_metadata));
  ASSERT_EQ(1U, output_metadata.size());
  EXPECT_EQ(input_metadata, output_metadata[input_metadata.id]);

  // Make sure nothing else got updated.
  std::vector<std::unique_ptr<AutofillProfile>> outputs2;
  table_->GetServerProfiles(&outputs2);
  ASSERT_EQ(1u, outputs2.size());
  EXPECT_TRUE(outputs[0]->EqualsForSyncPurposes(*outputs2[0]));
}

TEST_F(AutofillTableTest, UpdateServerCardMetadataDoesNotChangeData) {
  std::vector<CreditCard> inputs;
  inputs.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "a123"));
  inputs[0].SetRawInfo(CREDIT_CARD_NAME_FULL, u"Paul F. Tompkins");
  inputs[0].SetRawInfo(CREDIT_CARD_EXP_MONTH, u"1");
  inputs[0].SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2020");
  inputs[0].SetRawInfo(CREDIT_CARD_NUMBER, u"4111111111111111");
  test::SetServerCreditCards(table_.get(), inputs);

  std::vector<std::unique_ptr<CreditCard>> outputs;
  ASSERT_TRUE(table_->GetServerCreditCards(&outputs));
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(inputs[0].server_id(), outputs[0]->server_id());

  // Update metadata in the profile.
  ASSERT_NE(outputs[0]->use_count(), 51u);
  outputs[0]->set_use_count(51);

  AutofillMetadata input_metadata = outputs[0]->GetMetadata();
  EXPECT_TRUE(table_->UpdateServerCardMetadata(input_metadata));

  // Make sure it was updated correctly.
  std::map<std::string, AutofillMetadata> output_metadata;
  ASSERT_TRUE(table_->GetServerCardsMetadata(&output_metadata));
  ASSERT_EQ(1U, output_metadata.size());
  EXPECT_EQ(input_metadata, output_metadata[input_metadata.id]);

  // Make sure nothing else got updated.
  std::vector<std::unique_ptr<CreditCard>> outputs2;
  table_->GetServerCreditCards(&outputs2);
  ASSERT_EQ(1u, outputs2.size());
  EXPECT_EQ(0, outputs[0]->Compare(*outputs2[0]));
}

TEST_F(AutofillTableTest, RemoveWrongServerCardMetadata) {
  // Crete and set some metadata.
  AutofillMetadata input;
  input.id = "server id";
  input.use_count = 50;
  input.use_date = AutofillClock::Now();
  input.billing_address_id = "billing id";
  table_->AddServerCardMetadata(input);

  // Make sure it was added correctly.
  std::map<std::string, AutofillMetadata> outputs;
  ASSERT_TRUE(table_->GetServerCardsMetadata(&outputs));
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ(input, outputs[input.id]);

  // Try removing some non-existent metadata.
  EXPECT_FALSE(table_->RemoveServerCardMetadata("a_wrong_id"));

  // Make sure the metadata was not removed.
  ASSERT_TRUE(table_->GetServerCardsMetadata(&outputs));
  ASSERT_EQ(1U, outputs.size());
}

TEST_F(AutofillTableTest, SetServerCardsData) {
  // Set a card data.
  std::vector<CreditCard> inputs;
  inputs.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "card1"));
  inputs[0].SetRawInfo(CREDIT_CARD_NAME_FULL, u"Rick Roman");
  inputs[0].SetRawInfo(CREDIT_CARD_EXP_MONTH, u"12");
  inputs[0].SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"1997");
  inputs[0].SetRawInfo(CREDIT_CARD_NUMBER, u"1111");
  inputs[0].SetNetworkForMaskedCard(kVisaCard);
  inputs[0].SetNickname(u"Grocery card");
  inputs[0].set_instrument_id(1);
  inputs[0].set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::ENROLLED);
  inputs[0].set_card_art_url(GURL("https://www.example.com"));
  table_->SetServerCardsData(inputs);

  // Make sure the card was added correctly.
  std::vector<std::unique_ptr<CreditCard>> outputs;
  ASSERT_TRUE(table_->GetServerCreditCards(&outputs));
  ASSERT_EQ(inputs.size(), outputs.size());

  // GUIDs for server cards are dynamically generated so will be different
  // after reading from the DB. Check they're valid, but otherwise don't count
  // them in the comparison.
  inputs[0].set_guid(std::string());
  outputs[0]->set_guid(std::string());

  EXPECT_EQ(inputs[0], *outputs[0]);

  EXPECT_EQ(CreditCard::VirtualCardEnrollmentState::ENROLLED,
            outputs[0]->virtual_card_enrollment_state());

  EXPECT_EQ(GURL("https://www.example.com"), outputs[0]->card_art_url());

  // Make sure no metadata was added.
  std::map<std::string, AutofillMetadata> metadata_map;
  ASSERT_TRUE(table_->GetServerCardsMetadata(&metadata_map));
  ASSERT_EQ(0U, metadata_map.size());

  // Set a different card.
  inputs[0] = CreditCard(CreditCard::MASKED_SERVER_CARD, "card2");
  table_->SetServerCardsData(inputs);

  // The original one should have been replaced.
  ASSERT_TRUE(table_->GetServerCreditCards(&outputs));
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ("card2", outputs[0]->server_id());

  // Make sure no metadata was added.
  ASSERT_TRUE(table_->GetServerCardsMetadata(&metadata_map));
  ASSERT_EQ(0U, metadata_map.size());
}

// Tests that adding server cards data does not delete the existing metadata.
TEST_F(AutofillTableTest, SetServerCardsData_ExistingMetadata) {
  // Create and set some metadata.
  AutofillMetadata input;
  input.id = "server id";
  input.use_count = 50;
  input.use_date = AutofillClock::Now();
  input.billing_address_id = "billing id";
  table_->AddServerCardMetadata(input);

  // Set a card data.
  std::vector<CreditCard> inputs;
  inputs.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "server id"));
  table_->SetServerCardsData(inputs);

  // Make sure the metadata is still intact.
  std::map<std::string, AutofillMetadata> outputs;
  ASSERT_TRUE(table_->GetServerCardsMetadata(&outputs));
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ(input, outputs[input.id]);
}

TEST_F(AutofillTableTest, SetServerAddressesData) {
  AutofillProfile one(AutofillProfile::SERVER_PROFILE, "a123");
  std::vector<AutofillProfile> inputs;
  inputs.push_back(one);
  table_->SetServerAddressesData(inputs);

  // Make sure the address was added correctly.
  std::vector<std::unique_ptr<AutofillProfile>> outputs;
  table_->GetServerProfiles(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(one.server_id(), outputs[0]->server_id());

  outputs.clear();

  // Make sure no metadata was added.
  std::map<std::string, AutofillMetadata> metadata_map;
  ASSERT_TRUE(table_->GetServerAddressesMetadata(&metadata_map));
  ASSERT_EQ(0U, metadata_map.size());

  // Set a different profile.
  AutofillProfile two(AutofillProfile::SERVER_PROFILE, "b456");
  inputs[0] = two;
  table_->SetServerAddressesData(inputs);

  // The original one should have been replaced.
  table_->GetServerProfiles(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(two.server_id(), outputs[0]->server_id());

  // Make sure no metadata was added.
  ASSERT_TRUE(table_->GetServerAddressesMetadata(&metadata_map));
  ASSERT_EQ(0U, metadata_map.size());
}

// Tests that adding server addresses data does not delete the existing
// metadata.
TEST_F(AutofillTableTest, SetServerAddressesData_ExistingMetadata) {
  // Create and set some metadata.
  AutofillMetadata input;
  input.id = "server id";
  input.use_count = 50;
  input.use_date = AutofillClock::Now();
  input.has_converted = true;
  table_->AddServerAddressMetadata(input);

  // Set an address data.
  std::vector<AutofillProfile> inputs;
  inputs.push_back(
      AutofillProfile(AutofillProfile::SERVER_PROFILE, "server id"));
  table_->SetServerAddressesData(inputs);

  // Make sure the metadata is still intact.
  std::map<std::string, AutofillMetadata> outputs;
  ASSERT_TRUE(table_->GetServerAddressesMetadata(&outputs));
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ(input, outputs[input.id]);
}

TEST_F(AutofillTableTest, RemoveWrongServerAddressMetadata) {
  // Crete and set some metadata.
  AutofillMetadata input;
  input.id = "server id";
  input.use_count = 50;
  input.use_date = AutofillClock::Now();
  input.has_converted = true;
  table_->AddServerAddressMetadata(input);

  // Make sure it was added correctly.
  std::map<std::string, AutofillMetadata> outputs;
  ASSERT_TRUE(table_->GetServerAddressesMetadata(&outputs));
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ(input, outputs[input.id]);

  // Try removing some non-existent metadata.
  EXPECT_FALSE(table_->RemoveServerAddressMetadata("a_wrong_id"));

  // Make sure the metadata was not removed.
  ASSERT_TRUE(table_->GetServerAddressesMetadata(&outputs));
  ASSERT_EQ(1U, outputs.size());
}

TEST_F(AutofillTableTest, MaskUnmaskServerCards) {
  std::u16string masked_number(u"1111");
  std::vector<CreditCard> inputs;
  inputs.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  inputs[0].SetRawInfo(CREDIT_CARD_NAME_FULL, u"Jay Johnson");
  inputs[0].SetRawInfo(CREDIT_CARD_EXP_MONTH, u"1");
  inputs[0].SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2020");
  inputs[0].SetRawInfo(CREDIT_CARD_NUMBER, masked_number);
  inputs[0].SetNetworkForMaskedCard(kVisaCard);
  test::SetServerCreditCards(table_.get(), inputs);

  // Unmask the number. The full number should be available.
  std::u16string full_number(u"4111111111111111");
  ASSERT_TRUE(table_->UnmaskServerCreditCard(inputs[0], full_number));

  std::vector<std::unique_ptr<CreditCard>> outputs;
  table_->GetServerCreditCards(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_TRUE(CreditCard::FULL_SERVER_CARD == outputs[0]->record_type());
  EXPECT_EQ(full_number, outputs[0]->GetRawInfo(CREDIT_CARD_NUMBER));

  outputs.clear();

  // Re-mask the number, we should only get the last 4 digits out.
  ASSERT_TRUE(table_->MaskServerCreditCard(inputs[0].server_id()));
  table_->GetServerCreditCards(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_TRUE(CreditCard::MASKED_SERVER_CARD == outputs[0]->record_type());
  EXPECT_EQ(masked_number, outputs[0]->GetRawInfo(CREDIT_CARD_NUMBER));

  outputs.clear();
}

// Calling SetServerCreditCards should replace all existing cards, but unmasked
// cards should not be re-masked.
TEST_F(AutofillTableTest, SetServerCardModify) {
  // Add a masked card.
  CreditCard masked_card(CreditCard::MASKED_SERVER_CARD, "a123");
  masked_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Paul F. Tompkins");
  masked_card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"1");
  masked_card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2020");
  masked_card.SetRawInfo(CREDIT_CARD_NUMBER, u"1111");
  masked_card.SetNetworkForMaskedCard(kVisaCard);

  std::vector<CreditCard> inputs;
  inputs.push_back(masked_card);
  test::SetServerCreditCards(table_.get(), inputs);

  // Now unmask it.
  std::u16string full_number = u"4111111111111111";
  table_->UnmaskServerCreditCard(masked_card, full_number);

  // The card should now be unmasked.
  std::vector<std::unique_ptr<CreditCard>> outputs;
  table_->GetServerCreditCards(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_TRUE(outputs[0]->record_type() == CreditCard::FULL_SERVER_CARD);
  EXPECT_EQ(full_number, outputs[0]->GetRawInfo(CREDIT_CARD_NUMBER));

  outputs.clear();

  // Call set again with the masked number.
  inputs[0] = masked_card;
  test::SetServerCreditCards(table_.get(), inputs);

  // The card should stay unmasked.
  table_->GetServerCreditCards(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_TRUE(outputs[0]->record_type() == CreditCard::FULL_SERVER_CARD);
  EXPECT_EQ(full_number, outputs[0]->GetRawInfo(CREDIT_CARD_NUMBER));

  outputs.clear();

  // Set inputs that do not include our old card.
  CreditCard random_card(CreditCard::MASKED_SERVER_CARD, "b456");
  random_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Rick Roman");
  random_card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"12");
  random_card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"1997");
  random_card.SetRawInfo(CREDIT_CARD_NUMBER, u"2222");
  random_card.SetNetworkForMaskedCard(kVisaCard);
  inputs[0] = random_card;
  test::SetServerCreditCards(table_.get(), inputs);

  // We should have only the new card, the other one should have been deleted.
  table_->GetServerCreditCards(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_TRUE(outputs[0]->record_type() == CreditCard::MASKED_SERVER_CARD);
  EXPECT_EQ(random_card.server_id(), outputs[0]->server_id());
  EXPECT_EQ(u"2222", outputs[0]->GetRawInfo(CREDIT_CARD_NUMBER));

  outputs.clear();

  // Putting back the original card masked should make it masked (this tests
  // that the unmasked data was really deleted).
  inputs[0] = masked_card;
  test::SetServerCreditCards(table_.get(), inputs);
  table_->GetServerCreditCards(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_TRUE(outputs[0]->record_type() == CreditCard::MASKED_SERVER_CARD);
  EXPECT_EQ(masked_card.server_id(), outputs[0]->server_id());
  EXPECT_EQ(u"1111", outputs[0]->GetRawInfo(CREDIT_CARD_NUMBER));

  outputs.clear();
}

TEST_F(AutofillTableTest, SetServerCardUpdateUsageStatsAndBillingAddress) {
  // Add a masked card.
  CreditCard masked_card(CreditCard::MASKED_SERVER_CARD, "a123");
  masked_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Paul F. Tompkins");
  masked_card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"1");
  masked_card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2020");
  masked_card.SetRawInfo(CREDIT_CARD_NUMBER, u"1111");
  masked_card.set_billing_address_id("1");
  masked_card.SetNetworkForMaskedCard(kVisaCard);

  std::vector<CreditCard> inputs;
  inputs.push_back(masked_card);
  test::SetServerCreditCards(table_.get(), inputs);

  std::vector<std::unique_ptr<CreditCard>> outputs;
  table_->GetServerCreditCards(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(masked_card.server_id(), outputs[0]->server_id());
  EXPECT_EQ(1U, outputs[0]->use_count());
  EXPECT_NE(base::Time(), outputs[0]->use_date());
  // We don't track modification date for server cards. It should always be
  // base::Time().
  EXPECT_EQ(base::Time(), outputs[0]->modification_date());
  outputs.clear();

  // Update the usage stats; make sure they're reflected in GetServerProfiles.
  inputs.back().set_use_count(4U);
  inputs.back().set_use_date(base::Time());
  inputs.back().set_billing_address_id("2");
  table_->UpdateServerCardMetadata(inputs.back());
  table_->GetServerCreditCards(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(masked_card.server_id(), outputs[0]->server_id());
  EXPECT_EQ(4U, outputs[0]->use_count());
  EXPECT_EQ(base::Time(), outputs[0]->use_date());
  EXPECT_EQ(base::Time(), outputs[0]->modification_date());
  EXPECT_EQ("2", outputs[0]->billing_address_id());
  outputs.clear();

  // Setting the cards again shouldn't delete the usage stats.
  table_->SetServerCreditCards(inputs);
  table_->GetServerCreditCards(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(masked_card.server_id(), outputs[0]->server_id());
  EXPECT_EQ(4U, outputs[0]->use_count());
  EXPECT_EQ(base::Time(), outputs[0]->use_date());
  EXPECT_EQ(base::Time(), outputs[0]->modification_date());
  EXPECT_EQ("2", outputs[0]->billing_address_id());
  outputs.clear();

  // Set a card list where the card is missing --- this should clear metadata.
  CreditCard masked_card2(CreditCard::MASKED_SERVER_CARD, "b456");
  inputs.back() = masked_card2;
  table_->SetServerCreditCards(inputs);

  // Back to the original card list.
  inputs.back() = masked_card;
  table_->SetServerCreditCards(inputs);
  table_->GetServerCreditCards(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(masked_card.server_id(), outputs[0]->server_id());
  EXPECT_EQ(1U, outputs[0]->use_count());
  EXPECT_NE(base::Time(), outputs[0]->use_date());
  EXPECT_EQ(base::Time(), outputs[0]->modification_date());
  EXPECT_EQ("1", outputs[0]->billing_address_id());
  outputs.clear();
}

TEST_F(AutofillTableTest, SetServerProfile) {
  AutofillProfile one(AutofillProfile::SERVER_PROFILE, "a123");
  std::vector<AutofillProfile> inputs;
  inputs.push_back(one);
  table_->SetServerProfiles(inputs);

  std::vector<std::unique_ptr<AutofillProfile>> outputs;
  table_->GetServerProfiles(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(one.server_id(), outputs[0]->server_id());

  outputs.clear();

  // Set a different profile.
  AutofillProfile two(AutofillProfile::SERVER_PROFILE, "b456");
  inputs[0] = two;
  table_->SetServerProfiles(inputs);

  // The original one should have been replaced.
  table_->GetServerProfiles(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(two.server_id(), outputs[0]->server_id());

  outputs.clear();
}

TEST_F(AutofillTableTest, SetServerProfileUpdateUsageStats) {
  AutofillProfile one(AutofillProfile::SERVER_PROFILE, "a123");
  std::vector<AutofillProfile> inputs;
  inputs.push_back(one);
  table_->SetServerProfiles(inputs);

  std::vector<std::unique_ptr<AutofillProfile>> outputs;
  table_->GetServerProfiles(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(one.server_id(), outputs[0]->server_id());
  EXPECT_EQ(1U, outputs[0]->use_count());
  EXPECT_NE(base::Time(), outputs[0]->use_date());
  // We don't track modification date for server profiles. It should always be
  // base::Time().
  EXPECT_EQ(base::Time(), outputs[0]->modification_date());
  outputs.clear();

  // Update the usage stats; make sure they're reflected in GetServerProfiles.
  inputs.back().set_use_count(4U);
  inputs.back().set_use_date(AutofillClock::Now());
  table_->UpdateServerAddressMetadata(inputs.back());
  table_->GetServerProfiles(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(one.server_id(), outputs[0]->server_id());
  EXPECT_EQ(4U, outputs[0]->use_count());
  EXPECT_NE(base::Time(), outputs[0]->use_date());
  EXPECT_EQ(base::Time(), outputs[0]->modification_date());
  outputs.clear();

  // Setting the profiles again shouldn't delete the usage stats.
  table_->SetServerProfiles(inputs);
  table_->GetServerProfiles(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(one.server_id(), outputs[0]->server_id());
  EXPECT_EQ(4U, outputs[0]->use_count());
  EXPECT_NE(base::Time(), outputs[0]->use_date());
  EXPECT_EQ(base::Time(), outputs[0]->modification_date());
  outputs.clear();
}

// Tests that deleting time ranges re-masks server credit cards that were
// unmasked in that time.
TEST_F(AutofillTableTest, DeleteUnmaskedCard) {
  // This isn't the exact unmasked time, since the database will use the
  // current time that it is called. The code below has to be approximate.
  base::Time unmasked_time = AutofillClock::Now();

  // Add a masked card.
  std::u16string masked_number = u"1111";
  CreditCard masked_card(CreditCard::MASKED_SERVER_CARD, "a123");
  masked_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Paul F. Tompkins");
  masked_card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"1");
  masked_card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2020");
  masked_card.SetRawInfo(CREDIT_CARD_NUMBER, masked_number);
  masked_card.SetNetworkForMaskedCard(kVisaCard);

  std::vector<CreditCard> inputs;
  inputs.push_back(masked_card);
  table_->SetServerCreditCards(inputs);

  // Unmask it.
  std::u16string full_number = u"4111111111111111";
  table_->UnmaskServerCreditCard(masked_card, full_number);

  // Delete data in a range a year in the future.
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  std::vector<std::unique_ptr<CreditCard>> credit_cards;
  ASSERT_TRUE(table_->RemoveAutofillDataModifiedBetween(
      unmasked_time + base::Days(365), unmasked_time + base::Days(530),
      &profiles, &credit_cards));

  // This should not affect the unmasked card (should be unmasked).
  std::vector<std::unique_ptr<CreditCard>> outputs;
  ASSERT_TRUE(table_->GetServerCreditCards(&outputs));
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(CreditCard::FULL_SERVER_CARD, outputs[0]->record_type());
  EXPECT_EQ(full_number, outputs[0]->GetRawInfo(CREDIT_CARD_NUMBER));
  outputs.clear();

  // Delete data in the range of the last 24 hours.
  // Fudge |now| to make sure it's strictly greater than the |now| that
  // the database uses.
  base::Time now = AutofillClock::Now() + base::Seconds(1);
  ASSERT_TRUE(table_->RemoveAutofillDataModifiedBetween(
      now - base::Days(1), now, &profiles, &credit_cards));

  // This should re-mask.
  ASSERT_TRUE(table_->GetServerCreditCards(&outputs));
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(CreditCard::MASKED_SERVER_CARD, outputs[0]->record_type());
  EXPECT_EQ(masked_number, outputs[0]->GetRawInfo(CREDIT_CARD_NUMBER));
  outputs.clear();

  // Unmask again, the card should be back.
  table_->UnmaskServerCreditCard(masked_card, full_number);
  ASSERT_TRUE(table_->GetServerCreditCards(&outputs));
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(CreditCard::FULL_SERVER_CARD, outputs[0]->record_type());
  EXPECT_EQ(full_number, outputs[0]->GetRawInfo(CREDIT_CARD_NUMBER));
  outputs.clear();

  // Delete all data.
  ASSERT_TRUE(table_->RemoveAutofillDataModifiedBetween(
      base::Time(), base::Time::Max(), &profiles, &credit_cards));

  // Should be masked again.
  ASSERT_TRUE(table_->GetServerCreditCards(&outputs));
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(CreditCard::MASKED_SERVER_CARD, outputs[0]->record_type());
  EXPECT_EQ(masked_number, outputs[0]->GetRawInfo(CREDIT_CARD_NUMBER));
  outputs.clear();
}

// Test that we can get what we set.
TEST_F(AutofillTableTest, SetGetPaymentsCustomerData) {
  PaymentsCustomerData input{/*customer_id=*/"deadbeef"};
  table_->SetPaymentsCustomerData(&input);

  std::unique_ptr<PaymentsCustomerData> output;
  ASSERT_TRUE(table_->GetPaymentsCustomerData(&output));
  EXPECT_EQ(input, *output);
}

// We don't set anything in the table. Test that we don't crash.
TEST_F(AutofillTableTest, GetPaymentsCustomerData_NoData) {
  std::unique_ptr<PaymentsCustomerData> output;
  ASSERT_TRUE(table_->GetPaymentsCustomerData(&output));
  EXPECT_FALSE(output);
}

// The latest PaymentsCustomerData that was set is returned.
TEST_F(AutofillTableTest, SetGetPaymentsCustomerData_MultipleSet) {
  PaymentsCustomerData input{/*customer_id=*/"deadbeef"};
  table_->SetPaymentsCustomerData(&input);

  PaymentsCustomerData input2{/*customer_id=*/"wallet"};
  table_->SetPaymentsCustomerData(&input2);

  PaymentsCustomerData input3{/*customer_id=*/"latest"};
  table_->SetPaymentsCustomerData(&input3);

  std::unique_ptr<PaymentsCustomerData> output;
  ASSERT_TRUE(table_->GetPaymentsCustomerData(&output));
  EXPECT_EQ(input3, *output);
}

TEST_F(AutofillTableTest, SetGetCreditCardCloudData_OneTimeSet) {
  std::vector<CreditCardCloudTokenData> inputs;
  inputs.push_back(test::GetCreditCardCloudTokenData1());
  inputs.push_back(test::GetCreditCardCloudTokenData2());
  table_->SetCreditCardCloudTokenData(inputs);

  std::vector<std::unique_ptr<CreditCardCloudTokenData>> outputs;
  ASSERT_TRUE(table_->GetCreditCardCloudTokenData(&outputs));
  EXPECT_EQ(outputs.size(), inputs.size());
  EXPECT_EQ(0, outputs[0]->Compare(test::GetCreditCardCloudTokenData1()));
  EXPECT_EQ(0, outputs[1]->Compare(test::GetCreditCardCloudTokenData2()));
}

TEST_F(AutofillTableTest, SetGetCreditCardCloudData_MultipleSet) {
  std::vector<CreditCardCloudTokenData> inputs;
  CreditCardCloudTokenData input1 = test::GetCreditCardCloudTokenData1();
  inputs.push_back(input1);
  table_->SetCreditCardCloudTokenData(inputs);

  inputs.clear();
  CreditCardCloudTokenData input2 = test::GetCreditCardCloudTokenData2();
  inputs.push_back(input2);
  table_->SetCreditCardCloudTokenData(inputs);

  std::vector<std::unique_ptr<CreditCardCloudTokenData>> outputs;
  ASSERT_TRUE(table_->GetCreditCardCloudTokenData(&outputs));
  EXPECT_EQ(1u, outputs.size());
  EXPECT_EQ(0, outputs[0]->Compare(test::GetCreditCardCloudTokenData2()));
}

TEST_F(AutofillTableTest, GetCreditCardCloudData_NoData) {
  std::vector<std::unique_ptr<CreditCardCloudTokenData>> output;
  ASSERT_TRUE(table_->GetCreditCardCloudTokenData(&output));
  EXPECT_TRUE(output.empty());
}

const size_t kMaxCount = 2;
struct GetFormValuesTestCase {
  const char16_t* const field_suggestion[kMaxCount];
  const char16_t* const field_contents;
  size_t expected_suggestion_count;
  const char16_t* const expected_suggestion[kMaxCount];
};

class GetFormValuesTest : public testing::TestWithParam<GetFormValuesTestCase> {
 public:
  GetFormValuesTest() = default;
  GetFormValuesTest(const GetFormValuesTest&) = delete;
  GetFormValuesTest& operator=(const GetFormValuesTest&) = delete;
  ~GetFormValuesTest() override = default;

 protected:
  void SetUp() override {
    OSCryptMocker::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.GetPath().AppendASCII("TestWebDatabase");

    table_ = std::make_unique<AutofillTable>();
    db_ = std::make_unique<WebDatabase>();
    db_->AddTable(table_.get());
    ASSERT_EQ(sql::INIT_OK, db_->Init(file_));
  }

  void TearDown() override { OSCryptMocker::TearDown(); }

  base::FilePath file_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<AutofillTable> table_;
  std::unique_ptr<WebDatabase> db_;
};

TEST_P(GetFormValuesTest, GetFormValuesForElementName_SubstringMatchEnabled) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(features::kAutofillTokenPrefixMatching);

  auto test_case = GetParam();
  SCOPED_TRACE(testing::Message()
               << "suggestion = " << test_case.field_suggestion[0]
               << ", contents = " << test_case.field_contents);

  Time t1 = AutofillClock::Now();

  // Simulate the submission of a handful of entries in a field called "Name".
  AutofillChangeList changes;
  FormFieldData field;
  for (size_t k = 0; k < kMaxCount; ++k) {
    field.name = u"Name";
    field.value = test_case.field_suggestion[k];
    table_->AddFormFieldValue(field, &changes);
  }

  std::vector<AutofillEntry> v;
  table_->GetFormValuesForElementName(u"Name", test_case.field_contents, &v, 6);

  EXPECT_EQ(test_case.expected_suggestion_count, v.size());
  for (size_t j = 0; j < test_case.expected_suggestion_count; ++j) {
    EXPECT_EQ(test_case.expected_suggestion[j], v[j].key().value());
  }

  changes.clear();
  table_->RemoveFormElementsAddedBetween(t1, Time(), &changes);
}

INSTANTIATE_TEST_SUITE_P(
    AutofillTableTest,
    GetFormValuesTest,
    testing::Values(GetFormValuesTestCase{{u"user.test", u"test_user"},
                                          u"TEST",
                                          2,
                                          {u"test_user", u"user.test"}},
                    GetFormValuesTestCase{{u"user test", u"test-user"},
                                          u"user",
                                          2,
                                          {u"user test", u"test-user"}},
                    GetFormValuesTestCase{{u"user test", u"test-rest"},
                                          u"user",
                                          1,
                                          {u"user test", nullptr}},
                    GetFormValuesTestCase{{u"user@test", u"test_user"},
                                          u"user@t",
                                          1,
                                          {u"user@test", nullptr}},
                    GetFormValuesTestCase{{u"user.test", u"test_user"},
                                          u"er.tes",
                                          0,
                                          {nullptr, nullptr}},
                    GetFormValuesTestCase{{u"user test", u"test_user"},
                                          u"_ser",
                                          0,
                                          {nullptr, nullptr}},
                    GetFormValuesTestCase{{u"user.test", u"test_user"},
                                          u"%ser",
                                          0,
                                          {nullptr, nullptr}},
                    GetFormValuesTestCase{{u"user.test", u"test_user"},
                                          u"; DROP TABLE autofill;",
                                          0,
                                          {nullptr, nullptr}}));

class AutofillTableTestPerModelType
    : public AutofillTableTest,
      public testing::WithParamInterface<syncer::ModelType> {
 public:
  AutofillTableTestPerModelType() = default;
  AutofillTableTestPerModelType(const AutofillTableTestPerModelType&) = delete;
  AutofillTableTestPerModelType& operator=(
      const AutofillTableTestPerModelType&) = delete;
  ~AutofillTableTestPerModelType() override = default;
};

TEST_P(AutofillTableTestPerModelType, AutofillNoMetadata) {
  syncer::ModelType model_type = GetParam();
  MetadataBatch metadata_batch;
  EXPECT_TRUE(table_->GetAllSyncMetadata(model_type, &metadata_batch));
  EXPECT_EQ(0u, metadata_batch.TakeAllMetadata().size());
  EXPECT_EQ(ModelTypeState().SerializeAsString(),
            metadata_batch.GetModelTypeState().SerializeAsString());
}

TEST_P(AutofillTableTestPerModelType, AutofillGetAllSyncMetadata) {
  syncer::ModelType model_type = GetParam();
  EntityMetadata metadata;
  std::string storage_key = "storage_key";
  std::string storage_key2 = "storage_key2";
  metadata.set_sequence_number(1);

  EXPECT_TRUE(table_->UpdateSyncMetadata(model_type, storage_key, metadata));

  ModelTypeState model_type_state;
  model_type_state.set_initial_sync_done(true);

  EXPECT_TRUE(table_->UpdateModelTypeState(model_type, model_type_state));

  metadata.set_sequence_number(2);
  EXPECT_TRUE(table_->UpdateSyncMetadata(model_type, storage_key2, metadata));

  MetadataBatch metadata_batch;
  EXPECT_TRUE(table_->GetAllSyncMetadata(model_type, &metadata_batch));

  EXPECT_TRUE(metadata_batch.GetModelTypeState().initial_sync_done());

  EntityMetadataMap metadata_records = metadata_batch.TakeAllMetadata();

  EXPECT_EQ(metadata_records.size(), 2u);
  EXPECT_EQ(metadata_records[storage_key]->sequence_number(), 1);
  EXPECT_EQ(metadata_records[storage_key2]->sequence_number(), 2);

  // Now check that a model type state update replaces the old value
  model_type_state.set_initial_sync_done(false);
  EXPECT_TRUE(table_->UpdateModelTypeState(model_type, model_type_state));

  EXPECT_TRUE(table_->GetAllSyncMetadata(model_type, &metadata_batch));
  EXPECT_FALSE(metadata_batch.GetModelTypeState().initial_sync_done());
}

TEST_P(AutofillTableTestPerModelType, AutofillWriteThenDeleteSyncMetadata) {
  syncer::ModelType model_type = GetParam();
  EntityMetadata metadata;
  MetadataBatch metadata_batch;
  std::string storage_key = "storage_key";
  ModelTypeState model_type_state;

  model_type_state.set_initial_sync_done(true);

  metadata.set_client_tag_hash("client_hash");

  // Write the data into the store.
  EXPECT_TRUE(table_->UpdateSyncMetadata(model_type, storage_key, metadata));
  EXPECT_TRUE(table_->UpdateModelTypeState(model_type, model_type_state));
  // Delete the data we just wrote.
  EXPECT_TRUE(table_->ClearSyncMetadata(model_type, storage_key));
  // It shouldn't be there any more.
  EXPECT_TRUE(table_->GetAllSyncMetadata(model_type, &metadata_batch));

  EntityMetadataMap metadata_records = metadata_batch.TakeAllMetadata();
  EXPECT_EQ(metadata_records.size(), 0u);

  // Now delete the model type state.
  EXPECT_TRUE(table_->ClearModelTypeState(model_type));
  EXPECT_TRUE(table_->GetAllSyncMetadata(model_type, &metadata_batch));
  EXPECT_EQ(ModelTypeState().SerializeAsString(),
            metadata_batch.GetModelTypeState().SerializeAsString());
}

TEST_P(AutofillTableTestPerModelType, AutofillCorruptSyncMetadata) {
  syncer::ModelType model_type = GetParam();
  MetadataBatch metadata_batch;
  sql::Statement s(db_->GetSQLConnection()->GetUniqueStatement(
      "INSERT OR REPLACE INTO autofill_sync_metadata "
      "(model_type, storage_key, value) VALUES(?, ?, ?)"));
  s.BindInt(0, syncer::ModelTypeToStableIdentifier(model_type));
  s.BindString(1, "storage_key");
  s.BindString(2, "unparseable");
  EXPECT_TRUE(s.Run());

  EXPECT_FALSE(table_->GetAllSyncMetadata(model_type, &metadata_batch));
}

TEST_P(AutofillTableTestPerModelType, AutofillCorruptModelTypeState) {
  syncer::ModelType model_type = GetParam();
  MetadataBatch metadata_batch;
  sql::Statement s(db_->GetSQLConnection()->GetUniqueStatement(
      "INSERT OR REPLACE INTO autofill_model_type_state "
      "(model_type, value) VALUES(?, ?)"));
  s.BindInt(0, syncer::ModelTypeToStableIdentifier(model_type));
  s.BindString(1, "unparseable");
  EXPECT_TRUE(s.Run());

  EXPECT_FALSE(table_->GetAllSyncMetadata(model_type, &metadata_batch));
}

INSTANTIATE_TEST_SUITE_P(AutofillTableTest,
                         AutofillTableTestPerModelType,
                         testing::Values(syncer::AUTOFILL,
                                         syncer::AUTOFILL_PROFILE));

TEST_F(AutofillTableTest, RemoveOrphanAutofillTableRows) {
  // Populate the different tables.
  ASSERT_TRUE(db_->GetSQLConnection()->Execute(
      // Add a profile in all the tables
      "INSERT INTO autofill_profiles (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000000', 11);"
      "INSERT INTO autofill_profile_names (guid, full_name) "
      "VALUES('00000000-0000-0000-0000-000000000000', 'John Jones');"
      "INSERT INTO autofill_profile_emails (guid, email) "
      "VALUES('00000000-0000-0000-0000-000000000000', 'john@jones.com');"
      "INSERT INTO autofill_profile_phones (guid, number) "
      "VALUES('00000000-0000-0000-0000-000000000000', '111-111-1111');"

      // Add a profile in profiles, names and emails tables.
      "INSERT INTO autofill_profiles (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000001', 21);"
      "INSERT INTO autofill_profile_names (guid, full_name) "
      "VALUES('00000000-0000-0000-0000-000000000001', 'John Jones2');"
      "INSERT INTO autofill_profile_emails (guid, email) "
      "VALUES('00000000-0000-0000-0000-000000000001', 'john@jones2.com');"

      // Add a profile in profiles, names and phones tables.
      "INSERT INTO autofill_profiles (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000002', 31);"
      "INSERT INTO autofill_profile_names (guid, full_name) "
      "VALUES('00000000-0000-0000-0000-000000000002', 'John Jones3');"
      "INSERT INTO autofill_profile_phones (guid, number) "
      "VALUES('00000000-0000-0000-0000-000000000002', '333-333-3333');"

      // Add a profile in profiles, emails and phones tables.
      "INSERT INTO autofill_profiles (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000003', 41);"
      "INSERT INTO autofill_profile_emails (guid, email) "
      "VALUES('00000000-0000-0000-0000-000000000003', 'john@jones4.com');"
      "INSERT INTO autofill_profile_phones (guid, number) "
      "VALUES('00000000-0000-0000-0000-000000000003', '444-444-4444');"

      // Add a orphan profile in names, emails and phones tables.
      "INSERT INTO autofill_profile_names (guid, full_name) "
      "VALUES('00000000-0000-0000-0000-000000000004', 'John Jones5');"
      "INSERT INTO autofill_profile_emails (guid, email) "
      "VALUES('00000000-0000-0000-0000-000000000004', 'john@jones5.com');"
      "INSERT INTO autofill_profile_phones (guid, number) "
      "VALUES('00000000-0000-0000-0000-000000000004', '555-555-5555');"

      // Add a orphan profile in names and emails tables.
      "INSERT INTO autofill_profile_names (guid, full_name) "
      "VALUES('00000000-0000-0000-0000-000000000005', 'John Jones6');"
      "INSERT INTO autofill_profile_emails (guid, email) "
      "VALUES('00000000-0000-0000-0000-000000000005', 'john@jones6.com');"

      // Add a orphan profile in names and phones tables.
      "INSERT INTO autofill_profile_names (guid, full_name) "
      "VALUES('00000000-0000-0000-0000-000000000006', 'John Jones7');"
      "INSERT INTO autofill_profile_phones (guid, number) "
      "VALUES('00000000-0000-0000-0000-000000000006', '777-777-7777');"

      // Add a orphan profile in emails and phones tables.
      "INSERT INTO autofill_profile_emails (guid, email) "
      "VALUES('00000000-0000-0000-0000-000000000007', 'john@jones8.com');"
      "INSERT INTO autofill_profile_phones (guid, number) "
      "VALUES('00000000-0000-0000-0000-000000000007', '999-999-9999');"

      // Add a orphan profile in the names table.
      "INSERT INTO autofill_profile_names (guid, full_name) "
      "VALUES('00000000-0000-0000-0000-000000000008', 'John Jones9');"

      // Add a orphan profile in the emails table.
      "INSERT INTO autofill_profile_emails (guid, email) "
      "VALUES('00000000-0000-0000-0000-000000000009', 'john@jones10.com');"

      // Add a orphan profile in the phones table.
      "INSERT INTO autofill_profile_phones (guid, number) "
      "VALUES('00000000-0000-0000-0000-000000000010', '101-010-1010');"));

  ASSERT_TRUE(table_->RemoveOrphanAutofillTableRows());

  // Make sure that all the rows in the autofill_profiles table are still
  // present.
  sql::Statement s_autofill_profiles(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT guid FROM autofill_profiles"));
  ASSERT_TRUE(s_autofill_profiles.is_valid());
  ASSERT_TRUE(s_autofill_profiles.Step());
  EXPECT_EQ("00000000-0000-0000-0000-000000000000",
            s_autofill_profiles.ColumnString(0));
  ASSERT_TRUE(s_autofill_profiles.Step());
  EXPECT_EQ("00000000-0000-0000-0000-000000000001",
            s_autofill_profiles.ColumnString(0));
  ASSERT_TRUE(s_autofill_profiles.Step());
  EXPECT_EQ("00000000-0000-0000-0000-000000000002",
            s_autofill_profiles.ColumnString(0));
  ASSERT_TRUE(s_autofill_profiles.Step());
  EXPECT_EQ("00000000-0000-0000-0000-000000000003",
            s_autofill_profiles.ColumnString(0));
  EXPECT_FALSE(s_autofill_profiles.Step());

  // Make sure that only the rows present in the autofill_profiles table are
  // present in the autofill_profile_names table.
  sql::Statement s_autofill_profile_names(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT guid FROM autofill_profile_names"));
  ASSERT_TRUE(s_autofill_profile_names.is_valid());
  ASSERT_TRUE(s_autofill_profile_names.Step());
  EXPECT_EQ("00000000-0000-0000-0000-000000000000",
            s_autofill_profile_names.ColumnString(0));
  ASSERT_TRUE(s_autofill_profile_names.Step());
  EXPECT_EQ("00000000-0000-0000-0000-000000000001",
            s_autofill_profile_names.ColumnString(0));
  ASSERT_TRUE(s_autofill_profile_names.Step());
  EXPECT_EQ("00000000-0000-0000-0000-000000000002",
            s_autofill_profile_names.ColumnString(0));
  EXPECT_FALSE(s_autofill_profile_names.Step());

  // Make sure that only the rows present in the autofill_profiles table are
  // present in the autofill_profile_emails table.
  sql::Statement s_autofill_profile_emails(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT guid FROM autofill_profile_emails"));
  ASSERT_TRUE(s_autofill_profile_emails.is_valid());
  ASSERT_TRUE(s_autofill_profile_emails.Step());
  EXPECT_EQ("00000000-0000-0000-0000-000000000000",
            s_autofill_profile_emails.ColumnString(0));
  ASSERT_TRUE(s_autofill_profile_emails.Step());
  EXPECT_EQ("00000000-0000-0000-0000-000000000001",
            s_autofill_profile_emails.ColumnString(0));
  ASSERT_TRUE(s_autofill_profile_emails.Step());
  EXPECT_EQ("00000000-0000-0000-0000-000000000003",
            s_autofill_profile_emails.ColumnString(0));
  EXPECT_FALSE(s_autofill_profile_emails.Step());

  // Make sure that only the rows present in the autofill_profiles table are
  // present in the autofill_profile_phones table.
  sql::Statement s_autofill_profile_phones(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT guid FROM autofill_profile_phones"));
  ASSERT_TRUE(s_autofill_profile_phones.is_valid());
  ASSERT_TRUE(s_autofill_profile_phones.Step());
  EXPECT_EQ("00000000-0000-0000-0000-000000000000",
            s_autofill_profile_phones.ColumnString(0));
  ASSERT_TRUE(s_autofill_profile_phones.Step());
  EXPECT_EQ("00000000-0000-0000-0000-000000000002",
            s_autofill_profile_phones.ColumnString(0));
  ASSERT_TRUE(s_autofill_profile_phones.Step());
  EXPECT_EQ("00000000-0000-0000-0000-000000000003",
            s_autofill_profile_phones.ColumnString(0));
  EXPECT_FALSE(s_autofill_profile_phones.Step());
}

TEST_F(AutofillTableTest, InsertUpiId) {
  EXPECT_TRUE(table_->InsertUpiId("name@indianbank"));

  sql::Statement s_inspect(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT vpa FROM payments_upi_vpa"));

  ASSERT_TRUE(s_inspect.is_valid());
  ASSERT_TRUE(s_inspect.Step());
  EXPECT_GE(s_inspect.ColumnString(0), "name@indianbank");
  EXPECT_FALSE(s_inspect.Step());
}

TEST_F(AutofillTableTest, GetAllUpiIds) {
  constexpr char upi_id1[] = "name@indianbank";
  constexpr char upi_id2[] = "vpa@icici";
  EXPECT_TRUE(table_->InsertUpiId(upi_id1));
  EXPECT_TRUE(table_->InsertUpiId(upi_id2));

  std::vector<std::string> upi_ids = table_->GetAllUpiIds();
  ASSERT_THAT(upi_ids, UnorderedElementsAre(upi_id1, upi_id2));
}

TEST_F(AutofillTableTest, SetAndGetCreditCardOfferData) {
  // Create test data.
  AutofillOfferData credit_card_offer_1;
  AutofillOfferData credit_card_offer_2;
  AutofillOfferData credit_card_offer_3;

  // Set Offer ID.
  credit_card_offer_1.offer_id = 1;
  credit_card_offer_2.offer_id = 2;
  credit_card_offer_3.offer_id = 3;

  // Set reward amounts for card-linked offers on offer 1 and 2.
  credit_card_offer_1.offer_reward_amount = "$5";
  credit_card_offer_2.offer_reward_amount = "10%";

  // Set promo code for offer 3.
  credit_card_offer_3.promo_code = "5PCTOFFSHOES";

  // Set expiry.
  credit_card_offer_1.expiry = base::Time::FromDoubleT(1000);
  credit_card_offer_2.expiry = base::Time::FromDoubleT(2000);
  credit_card_offer_3.expiry = base::Time::FromDoubleT(3000);

  // Set details URL.
  credit_card_offer_1.offer_details_url =
      GURL("https://www.offer_1_example.com/");
  credit_card_offer_2.offer_details_url =
      GURL("https://www.offer_2_example.com/");
  credit_card_offer_3.offer_details_url =
      GURL("https://www.offer_3_example.com/");

  // Set merchant domains for offer 1.
  credit_card_offer_1.merchant_origins.emplace_back(
      "http://www.merchant_domain_1_1.com/");
  credit_card_offer_1.merchant_origins.emplace_back(
      "http://www.merchant_domain_1_2.com/");
  credit_card_offer_1.merchant_origins.emplace_back(
      "http://www.merchant_domain_1_3.com/");
  // Set merchant domains for offer 2.
  credit_card_offer_2.merchant_origins.emplace_back(
      "http://www.merchant_domain_2_1.com/");
  // Set merchant domains for offer 3.
  credit_card_offer_3.merchant_origins.emplace_back(
      "http://www.merchant_domain_3_1.com/");
  credit_card_offer_3.merchant_origins.emplace_back(
      "http://www.merchant_domain_3_2.com/");

  // Set display strings for all 3 offers.
  credit_card_offer_1.display_strings.value_prop_text = "$5 off your purchase";
  credit_card_offer_2.display_strings.value_prop_text = "10% off your purchase";
  credit_card_offer_3.display_strings.value_prop_text =
      "5% off shoes. Up to $50.";
  credit_card_offer_1.display_strings.see_details_text = "Terms apply.";
  credit_card_offer_2.display_strings.see_details_text = "Terms apply.";
  credit_card_offer_3.display_strings.see_details_text = "See details.";
  credit_card_offer_1.display_strings.usage_instructions_text =
      "Check out with this card to activate.";
  credit_card_offer_2.display_strings.usage_instructions_text =
      "Check out with this card to activate.";
  credit_card_offer_3.display_strings.usage_instructions_text =
      "Click the promo code field at checkout to autofill it.";

  // Set eligible card-linked instrument ID for offer 1.
  credit_card_offer_1.eligible_instrument_id.push_back(10);
  credit_card_offer_1.eligible_instrument_id.push_back(11);
  // Set eligible card-linked instrument ID for offer 2.
  credit_card_offer_2.eligible_instrument_id.push_back(20);
  credit_card_offer_2.eligible_instrument_id.push_back(21);
  credit_card_offer_2.eligible_instrument_id.push_back(22);

  // Create vector of offer data.
  std::vector<AutofillOfferData> autofill_offer_data;
  autofill_offer_data.push_back(credit_card_offer_1);
  autofill_offer_data.push_back(credit_card_offer_2);
  autofill_offer_data.push_back(credit_card_offer_3);

  table_->SetAutofillOffers(autofill_offer_data);

  std::vector<std::unique_ptr<AutofillOfferData>> output_offer_data;

  EXPECT_TRUE(table_->GetAutofillOffers(&output_offer_data));
  EXPECT_EQ(autofill_offer_data.size(), output_offer_data.size());

  for (const auto& data : autofill_offer_data) {
    // Find output data with corresponding Offer ID.
    size_t output_index = 0;
    while (output_index < output_offer_data.size()) {
      if (data.offer_id == output_offer_data[output_index]->offer_id) {
        break;
      }
      output_index++;
    }

    // Expect to find matching Offer ID's.
    EXPECT_NE(output_index, output_offer_data.size());

    // All corresponding fields must be equal.
    EXPECT_EQ(data.offer_id, output_offer_data[output_index]->offer_id);
    EXPECT_EQ(data.offer_reward_amount,
              output_offer_data[output_index]->offer_reward_amount);
    EXPECT_EQ(data.promo_code, output_offer_data[output_index]->promo_code);
    EXPECT_EQ(data.expiry, output_offer_data[output_index]->expiry);
    EXPECT_EQ(data.offer_details_url.spec(),
              output_offer_data[output_index]->offer_details_url.spec());
    EXPECT_EQ(data.display_strings.value_prop_text,
              output_offer_data[output_index]->display_strings.value_prop_text);
    EXPECT_EQ(
        data.display_strings.see_details_text,
        output_offer_data[output_index]->display_strings.see_details_text);
    EXPECT_EQ(data.display_strings.usage_instructions_text,
              output_offer_data[output_index]
                  ->display_strings.usage_instructions_text);
    ASSERT_THAT(data.merchant_origins,
                testing::UnorderedElementsAreArray(
                    output_offer_data[output_index]->merchant_origins));
    ASSERT_THAT(data.eligible_instrument_id,
                testing::UnorderedElementsAreArray(
                    output_offer_data[output_index]->eligible_instrument_id));
  }
}

}  // namespace autofill
