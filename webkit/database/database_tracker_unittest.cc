// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/database/database_tracker.h"

namespace {

class TestObserver : public webkit_database::DatabaseTracker::Observer {
 public:
  TestObserver() : new_notification_received_(false) {}
  virtual ~TestObserver() {}
  virtual void OnDatabaseSizeChanged(const string16& origin_identifier,
                                     const string16& database_name,
                                     int64 database_size,
                                     int64 space_available) {
    new_notification_received_ = true;
    origin_identifier_ = origin_identifier;
    database_name_ = database_name;
    database_size_ = database_size;
    space_available_ = space_available;
  }
  bool DidReceiveNewNotification() {
    bool temp_new_notification_received = new_notification_received_;
    new_notification_received_ = false;
    return temp_new_notification_received;
  }
  string16 GetNotificationOriginIdentifier() { return origin_identifier_; }
  string16 GetNotificationDatabaseName() { return database_name_; }
  int64 GetNotificationDatabaseSize() { return database_size_; }
  int64 GetNotificationSpaceAvailable() { return space_available_; }

 private:
  bool new_notification_received_;
  string16 origin_identifier_;
  string16 database_name_;
  int64 database_size_;
  int64 space_available_;
};

void CheckNotificationReceived(TestObserver* observer,
                               const string16& expected_origin_identifier,
                               const string16& expected_database_name,
                               int64 expected_database_size,
                               int64 expected_space_available) {
  EXPECT_TRUE(observer->DidReceiveNewNotification());
  EXPECT_EQ(expected_origin_identifier,
            observer->GetNotificationOriginIdentifier());
  EXPECT_EQ(expected_database_name,
            observer->GetNotificationDatabaseName());
  EXPECT_EQ(expected_database_size,
            observer->GetNotificationDatabaseSize());
  EXPECT_EQ(expected_space_available,
            observer->GetNotificationSpaceAvailable());
}

}  // namespace

namespace webkit_database {

TEST(DatabaseTrackerTest, TestIt) {
  // Initialize the tracker database.
  ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  scoped_refptr<DatabaseTracker> tracker(new DatabaseTracker(temp_dir.path()));

  // Add two observers.
  TestObserver observer1;
  TestObserver observer2;
  tracker->AddObserver(&observer1);
  tracker->AddObserver(&observer2);

  // Open three new databases.
  int64 database_size = 0;
  int64 space_available = 0;
  const string16 kOrigin1 = ASCIIToUTF16("origin1");
  const string16 kOrigin2 = ASCIIToUTF16("origin2");
  const string16 kDB1 = ASCIIToUTF16("db1");
  const string16 kDB2 = ASCIIToUTF16("db2");
  const string16 kDB3 = ASCIIToUTF16("db3");
  const string16 kDescription = ASCIIToUTF16("database_description");

  // Get the quota for kOrigin1 and kOrigin2
  DatabaseTracker::CachedOriginInfo* origin1_info =
      tracker->GetCachedOriginInfo(kOrigin1);
  DatabaseTracker::CachedOriginInfo* origin2_info =
      tracker->GetCachedOriginInfo(kOrigin1);
  EXPECT_TRUE(origin1_info);
  EXPECT_TRUE(origin2_info);
  int64 origin1_quota = origin1_info->Quota();
  int64 origin2_quota = origin2_info->Quota();
  EXPECT_EQ(origin1_quota, tracker->GetOriginSpaceAvailable(kOrigin1));
  EXPECT_EQ(origin2_quota, tracker->GetOriginSpaceAvailable(kOrigin2));

  // Set a new quota for kOrigin1
  origin1_quota *= 2;
  tracker->SetOriginQuota(kOrigin1, origin1_quota);
  origin1_info = tracker->GetCachedOriginInfo(kOrigin1);
  EXPECT_TRUE(origin1_info);
  EXPECT_EQ(origin1_quota, origin1_info->Quota());

  tracker->DatabaseOpened(kOrigin1, kDB1, kDescription, 0,
                          &database_size, &space_available);
  EXPECT_EQ(0, database_size);
  EXPECT_EQ(origin1_quota, space_available);
  tracker->DatabaseOpened(kOrigin2, kDB2, kDescription, 0,
                          &database_size, &space_available);
  EXPECT_EQ(0, database_size);
  EXPECT_EQ(origin2_quota, space_available);
  tracker->DatabaseOpened(kOrigin1, kDB3, kDescription, 0,
                          &database_size, &space_available);
  EXPECT_EQ(0, database_size);
  EXPECT_EQ(origin1_quota, space_available);

  // Tell the tracker that a database has changed.
  // Even though nothing has changed, the observers should be notified.
  tracker->DatabaseModified(kOrigin1, kDB1);
  CheckNotificationReceived(&observer1, kOrigin1, kDB1, 0, origin1_quota);
  CheckNotificationReceived(&observer2, kOrigin1, kDB1, 0, origin1_quota);

  // Write some data to each file and check that the listeners are
  // called with the appropriate values.
  EXPECT_TRUE(file_util::CreateDirectory(tracker->DatabaseDirectory().Append(
      FilePath::FromWStringHack(UTF16ToWide(kOrigin1)))));
  EXPECT_TRUE(file_util::CreateDirectory(tracker->DatabaseDirectory().Append(
      FilePath::FromWStringHack(UTF16ToWide(kOrigin2)))));
  EXPECT_EQ(1, file_util::WriteFile(
      tracker->GetFullDBFilePath(kOrigin1, kDB1), "a", 1));
  EXPECT_EQ(2, file_util::WriteFile(
      tracker->GetFullDBFilePath(kOrigin2, kDB2), "aa", 2));
  EXPECT_EQ(4, file_util::WriteFile(
      tracker->GetFullDBFilePath(kOrigin1, kDB3), "aaaa", 4));
  tracker->DatabaseModified(kOrigin1, kDB1);
  CheckNotificationReceived(&observer1, kOrigin1, kDB1, 1, origin1_quota - 1);
  CheckNotificationReceived(&observer2, kOrigin1, kDB1, 1, origin1_quota - 1);
  tracker->DatabaseModified(kOrigin2, kDB2);
  CheckNotificationReceived(&observer1, kOrigin2, kDB2, 2, origin2_quota - 2);
  CheckNotificationReceived(&observer2, kOrigin2, kDB2, 2, origin2_quota - 2);
  tracker->DatabaseModified(kOrigin1, kDB3);
  CheckNotificationReceived(&observer1, kOrigin1, kDB3, 4, origin1_quota - 5);
  CheckNotificationReceived(&observer2, kOrigin1, kDB3, 4, origin1_quota - 5);

  // Make sure the available space for kOrigin1 and kOrigin2 changed accordingly
  EXPECT_EQ(origin1_quota - 5, tracker->GetOriginSpaceAvailable(kOrigin1));
  EXPECT_EQ(origin2_quota - 2, tracker->GetOriginSpaceAvailable(kOrigin2));

  // Close all databases
  tracker->DatabaseClosed(kOrigin1, kDB1);
  tracker->DatabaseClosed(kOrigin2, kDB2);
  tracker->DatabaseClosed(kOrigin1, kDB3);

  // Open an existing database and check the reported size
  tracker->DatabaseOpened(kOrigin1, kDB1, kDescription, 0,
                          &database_size, &space_available);
  EXPECT_EQ(1, database_size);
  EXPECT_EQ(origin1_quota - 5, space_available);

  // Make sure that the observers are notified even if
  // the size of the database hasn't changed.
  EXPECT_EQ(1, file_util::WriteFile(
      tracker->GetFullDBFilePath(kOrigin1, kDB1), "b", 1));
  tracker->DatabaseModified(kOrigin1, kDB1);
  CheckNotificationReceived(&observer1, kOrigin1, kDB1, 1, origin1_quota - 5);
  CheckNotificationReceived(&observer2, kOrigin1, kDB1, 1, origin1_quota - 5);
  tracker->DatabaseClosed(kOrigin1, kDB1);

  // Remove an observer; this should clear all caches.
  tracker->RemoveObserver(&observer2);

  // Change kDB1's and kDB3's size and call tracker->DatabaseModified()
  // for kDB1 only. If the caches were indeed cleared, then calling
  // tracker->DatabaseModified() should re-populate the cache for
  // kOrigin1 == kOrigin1, and thus, should pick up kDB3's size change too.
  EXPECT_EQ(5, file_util::WriteFile(
      tracker->GetFullDBFilePath(kOrigin1, kDB1), "ccccc", 5));
  EXPECT_EQ(6, file_util::WriteFile(
      tracker->GetFullDBFilePath(kOrigin1, kDB3), "dddddd", 6));
  tracker->DatabaseModified(kOrigin1, kDB1);
  CheckNotificationReceived(&observer1, kOrigin1, kDB1, 5, origin1_quota - 11);
  EXPECT_FALSE(observer2.DidReceiveNewNotification());
  EXPECT_EQ(origin1_quota - 11, tracker->GetOriginSpaceAvailable(kOrigin1));

  // Close the tracker database and clear all caches.
  // Then make sure that DatabaseOpened() still returns the correct result.
  tracker->CloseTrackerDatabaseAndClearCaches();
  tracker->DatabaseOpened(kOrigin1, kDB1, kDescription, 0,
                          &database_size, &space_available);
  EXPECT_EQ(5, database_size);
  EXPECT_EQ(origin1_quota - 11, space_available);

  // Close the tracker database and clear all caches. Then make sure that
  // DatabaseModified() still calls the observers with correct values.
  tracker->CloseTrackerDatabaseAndClearCaches();
  tracker->DatabaseModified(kOrigin1, kDB3);
  CheckNotificationReceived(&observer1, kOrigin1, kDB3, 6, origin1_quota - 11);
  tracker->DatabaseClosed(kOrigin1, kDB1);

  // Remove all observers.
  tracker->RemoveObserver(&observer1);

  // Trying to delete a database in use should fail
  tracker->DatabaseOpened(kOrigin1, kDB3, kDescription, 0,
                          &database_size, &space_available);
  EXPECT_FALSE(tracker->DeleteDatabase(kOrigin1, kDB3));
  origin1_info = tracker->GetCachedOriginInfo(kOrigin1);
  EXPECT_TRUE(origin1_info);
  EXPECT_EQ(6, origin1_info->GetDatabaseSize(kDB3));
  tracker->DatabaseClosed(kOrigin1, kDB3);

  // Delete a database and make sure the space used by that origin is updated
  EXPECT_TRUE(tracker->DeleteDatabase(kOrigin1, kDB3));
  origin1_info = tracker->GetCachedOriginInfo(kOrigin1);
  EXPECT_TRUE(origin1_info);
  EXPECT_EQ(origin1_quota - 5, tracker->GetOriginSpaceAvailable(kOrigin1));
  EXPECT_EQ(5, origin1_info->GetDatabaseSize(kDB1));
  EXPECT_EQ(0, origin1_info->GetDatabaseSize(kDB3));

  // Get all data for all origins
  std::vector<OriginInfo> origins_info;
  EXPECT_TRUE(tracker->GetAllOriginsInfo(&origins_info));
  EXPECT_EQ(size_t(2), origins_info.size());
  EXPECT_EQ(kOrigin1, origins_info[0].GetOrigin());
  EXPECT_EQ(5, origins_info[0].TotalSize());
  EXPECT_EQ(origin1_quota, origins_info[0].Quota());
  EXPECT_EQ(5, origins_info[0].GetDatabaseSize(kDB1));
  EXPECT_EQ(0, origins_info[0].GetDatabaseSize(kDB3));

  EXPECT_EQ(kOrigin2, origins_info[1].GetOrigin());
  EXPECT_EQ(2, origins_info[1].TotalSize());
  EXPECT_EQ(origin2_quota, origins_info[1].Quota());

  // Trying to delete an origin with databases in use should fail
  tracker->DatabaseOpened(kOrigin1, kDB1, kDescription, 0,
                          &database_size, &space_available);
  EXPECT_FALSE(tracker->DeleteOrigin(kOrigin1));
  origin1_info = tracker->GetCachedOriginInfo(kOrigin1);
  EXPECT_TRUE(origin1_info);
  EXPECT_EQ(5, origin1_info->GetDatabaseSize(kDB1));
  tracker->DatabaseClosed(kOrigin1, kDB1);

  // Delete an origin that doesn't have any database in use
  EXPECT_TRUE(tracker->DeleteOrigin(kOrigin1));
  origins_info.clear();
  EXPECT_TRUE(tracker->GetAllOriginsInfo(&origins_info));
  EXPECT_EQ(size_t(1), origins_info.size());
  EXPECT_EQ(kOrigin2, origins_info[0].GetOrigin());

  origin1_info = tracker->GetCachedOriginInfo(kOrigin1);
  EXPECT_TRUE(origin1_info);
  EXPECT_EQ(origin1_quota, origin1_info->Quota());
  EXPECT_EQ(0, origin1_info->TotalSize());
  EXPECT_EQ(origin1_quota, tracker->GetOriginSpaceAvailable(kOrigin1));
}

}  // namespace webkit_database
