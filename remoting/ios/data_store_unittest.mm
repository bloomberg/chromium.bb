// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/data_store.h"

#import "base/compiler_specific.h"
#import "testing/gtest_mac.h"

namespace remoting {

namespace {

NSString* kHostId = @"testHost";
NSString* kHostPin = @"testHostPin";
NSString* kPairId = @"testPairId";
NSString* kPairSecret = @"testPairSecret";

}  // namespace

class DataStoreTest : public ::testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    store_ = [[DataStore allocWithZone:nil] init];
    RemoveAllHosts();
    EXPECT_EQ(0, HostCount());
  }
  virtual void TearDown() OVERRIDE { RemoveAllHosts(); }

  int HostCount() { return [[store_ allHosts] count]; }

  void RemoveAllHosts() {
    while (HostCount() > 0) {
      [store_ removeHost:[store_ allHosts].firstObject];
    }
    [store_ saveChanges];
  }

  DataStore* store_;
};

TEST(DataStoreTest_Static, IsSingleInstance) {
  DataStore* firstStore = [DataStore sharedStore];

  ASSERT_NSEQ(firstStore, [DataStore sharedStore]);
}

TEST(DataStoreTest_Static, RemoveAllHost) {
  // Test this functionality independently before expecting the fixture to do
  // this correctly during cleanup
  DataStore* store = [DataStore sharedStore];

  while ([[store allHosts] count]) {
    [store removeHost:[store allHosts].firstObject];
  }

  ASSERT_EQ(0, [[store allHosts] count]);
  store = nil;
}

TEST_F(DataStoreTest, CreateHost) {

  const HostPreferences* host = [store_ createHost:kHostId];
  ASSERT_STREQ([kHostId UTF8String], [host.hostId UTF8String]);
  ASSERT_EQ(1, HostCount());
}

TEST_F(DataStoreTest, GetHostForId) {
  const HostPreferences* host = [store_ getHostForId:kHostId];
  ASSERT_TRUE(host == nil);

  [store_ createHost:kHostId];

  host = [store_ getHostForId:kHostId];

  ASSERT_TRUE(host != nil);
  ASSERT_STREQ([kHostId UTF8String], [host.hostId UTF8String]);
}

TEST_F(DataStoreTest, SaveChanges) {

  const HostPreferences* newHost = [store_ createHost:kHostId];

  ASSERT_EQ(1, HostCount());

  // Default values for a new host
  ASSERT_TRUE([newHost.askForPin boolValue] == NO);
  ASSERT_TRUE(newHost.hostPin == nil);
  ASSERT_TRUE(newHost.pairId == nil);
  ASSERT_TRUE(newHost.pairSecret == nil);

  // Set new values and save
  newHost.askForPin = [NSNumber numberWithBool:YES];
  newHost.hostPin = kHostPin;
  newHost.pairId = kPairId;
  newHost.pairSecret = kPairSecret;

  [store_ saveChanges];

  // The next time the store is loaded the host will still be present, even
  // though we are about to release and reinit a new object
  store_ = nil;
  store_ = [[DataStore allocWithZone:nil] init];
  ASSERT_EQ(1, HostCount());

  const HostPreferences* host = [store_ getHostForId:kHostId];
  ASSERT_TRUE(host != nil);
  ASSERT_STREQ([kHostId UTF8String], [host.hostId UTF8String]);
  ASSERT_TRUE([host.askForPin boolValue] == YES);
  ASSERT_STREQ([kHostPin UTF8String], [host.hostPin UTF8String]);
  ASSERT_STREQ([kPairId UTF8String], [host.pairId UTF8String]);
  ASSERT_STREQ([kPairSecret UTF8String], [host.pairSecret UTF8String]);
}

}  // namespace remoting
