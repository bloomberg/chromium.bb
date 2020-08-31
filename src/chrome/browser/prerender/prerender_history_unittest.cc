// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_history.h"

#include <stddef.h>

#include <memory>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace prerender {

namespace {

bool ListEntryMatches(base::ListValue* list,
                      size_t index,
                      const char* expected_url,
                      FinalStatus expected_final_status,
                      Origin expected_origin,
                      const std::string& expected_end_time) {
  if (index >= list->GetSize())
    return false;
  base::DictionaryValue* dict = NULL;
  if (!list->GetDictionary(index, &dict))
    return false;
  if (dict->size() != 4u)
    return false;
  std::string url;
  if (!dict->GetString("url", &url))
    return false;
  if (url != expected_url)
    return false;
  std::string final_status;
  if (!dict->GetString("final_status", &final_status))
    return false;
  if (final_status != NameFromFinalStatus(expected_final_status))
    return false;
  std::string origin;
  if (!dict->GetString("origin", &origin))
    return false;
  if (origin != NameFromOrigin(expected_origin))
    return false;
  std::string end_time;
  if (!dict->GetString("end_time", &end_time))
    return false;
  if (end_time != expected_end_time)
    return false;
  return true;
}

TEST(PrerenderHistoryTest, GetAsValue)  {
  std::unique_ptr<base::Value> entry_value;
  base::ListValue* entry_list = NULL;

  // Create a history with only 2 values.
  PrerenderHistory history(2);

  // Make sure an empty list exists when retrieving as value.
  entry_value = history.CopyEntriesAsValue();
  ASSERT_TRUE(entry_value.get() != NULL);
  ASSERT_TRUE(entry_value->GetAsList(&entry_list));
  EXPECT_TRUE(entry_list->empty());

  // Base time used for all events.  Each event is given a time 1 millisecond
  // after that of the previous one.
  base::Time epoch_start = base::Time::UnixEpoch();

  // Add a single entry and make sure that it matches up.
  const char* const kFirstUrl = "http://www.alpha.com/";
  const FinalStatus kFirstFinalStatus = FINAL_STATUS_USED;
  const Origin kFirstOrigin = ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN;
  PrerenderHistory::Entry entry_first(
      GURL(kFirstUrl), kFirstFinalStatus, kFirstOrigin, epoch_start);
  history.AddEntry(entry_first);
  entry_value = history.CopyEntriesAsValue();
  ASSERT_TRUE(entry_value.get() != NULL);
  ASSERT_TRUE(entry_value->GetAsList(&entry_list));
  EXPECT_EQ(1u, entry_list->GetSize());
  EXPECT_TRUE(ListEntryMatches(entry_list, 0u, kFirstUrl, kFirstFinalStatus,
                               kFirstOrigin, "0"));

  // Add a second entry and make sure both first and second appear.
  const char* const kSecondUrl = "http://www.beta.com/";
  const FinalStatus kSecondFinalStatus = FINAL_STATUS_DUPLICATE;
  const Origin kSecondOrigin = ORIGIN_OMNIBOX;
  PrerenderHistory::Entry entry_second(
      GURL(kSecondUrl), kSecondFinalStatus, kSecondOrigin,
      epoch_start + base::TimeDelta::FromMilliseconds(1));
  history.AddEntry(entry_second);
  entry_value = history.CopyEntriesAsValue();
  ASSERT_TRUE(entry_value.get() != NULL);
  ASSERT_TRUE(entry_value->GetAsList(&entry_list));
  EXPECT_EQ(2u, entry_list->GetSize());
  EXPECT_TRUE(ListEntryMatches(entry_list, 0u, kSecondUrl, kSecondFinalStatus,
                               kSecondOrigin, "1"));
  EXPECT_TRUE(ListEntryMatches(entry_list, 1u, kFirstUrl, kFirstFinalStatus,
                               kFirstOrigin, "0"));

  // Add a third entry and make sure that the first one drops off.
  const char* const kThirdUrl = "http://www.gamma.com/";
  const FinalStatus kThirdFinalStatus = FINAL_STATUS_AUTH_NEEDED;
  const Origin kThirdOrigin = ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN;
  PrerenderHistory::Entry entry_third(
      GURL(kThirdUrl), kThirdFinalStatus, kThirdOrigin,
      epoch_start + base::TimeDelta::FromMilliseconds(2));
  history.AddEntry(entry_third);
  entry_value = history.CopyEntriesAsValue();
  ASSERT_TRUE(entry_value.get() != NULL);
  ASSERT_TRUE(entry_value->GetAsList(&entry_list));
  EXPECT_EQ(2u, entry_list->GetSize());
  EXPECT_TRUE(ListEntryMatches(entry_list, 0u, kThirdUrl, kThirdFinalStatus,
                               kThirdOrigin, "2"));
  EXPECT_TRUE(ListEntryMatches(entry_list, 1u, kSecondUrl, kSecondFinalStatus,
                               kSecondOrigin, "1"));

  // Make sure clearing history acts as expected.
  history.Clear();
  entry_value = history.CopyEntriesAsValue();
  ASSERT_TRUE(entry_value.get() != NULL);
  ASSERT_TRUE(entry_value->GetAsList(&entry_list));
  EXPECT_TRUE(entry_list->empty());
}

}  // namespace

}  // namespace prerender
