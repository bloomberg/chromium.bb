// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/ui/host_list_view_controller.h"

#import "base/compiler_specific.h"
#import "testing/gtest_mac.h"

#import "remoting/ios/host.h"
#import "remoting/ios/host_refresh_test_helper.h"
#import "remoting/ios/ui/host_view_controller.h"

namespace remoting {

class HostListViewControllerTest : public ::testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    controller_ = [[HostListViewController alloc] init];
    SetHostByCount(1);
  }

  void SetHostByCount(int numHosts) {
    NSArray* array =
        [Host parseListFromJSON:HostRefreshTestHelper::GetHostList(numHosts)];
    RefreshHostList(array);
  }

  void SetHostByString(NSString* string) {
    NSArray* array =
        [Host parseListFromJSON:HostRefreshTestHelper::GetHostList(string)];
    RefreshHostList(array);
  }

  void RefreshHostList(NSArray* array) {
    [controller_ hostListRefresh:array errorMessage:nil];
  }

  HostListViewController* controller_;
};

TEST_F(HostListViewControllerTest, DefaultAuthorization) {
  ASSERT_TRUE(controller_.authorization == nil);

  [controller_ viewWillAppear:YES];

  ASSERT_TRUE(controller_.authorization != nil);
}

TEST_F(HostListViewControllerTest, hostListRefresh) {
  SetHostByCount(2);
  ASSERT_EQ(2, [controller_ tableView:nil numberOfRowsInSection:0]);

  SetHostByCount(10);
  ASSERT_EQ(10, [controller_ tableView:nil numberOfRowsInSection:0]);
}

TEST_F(HostListViewControllerTest,
       ShouldPerformSegueWithIdentifierOfConnectToHost) {
  ASSERT_FALSE([controller_ shouldPerformSegueWithIdentifier:@"ConnectToHost"
                                                      sender:nil]);

  NSString* host = HostRefreshTestHelper::GetMultipleHosts(1);
  host = [host stringByReplacingOccurrencesOfString:@"TESTING"
                                         withString:@"ONLINE"];
  SetHostByString(host);
  ASSERT_TRUE([controller_ shouldPerformSegueWithIdentifier:@"ConnectToHost"
                                                     sender:nil]);
}

TEST_F(HostListViewControllerTest, prepareSegueWithIdentifierOfConnectToHost) {
  HostViewController* destination = [[HostViewController alloc] init];

  ASSERT_NSNE(HostRefreshTestHelper::HostNameTest, destination.host.hostName);

  UIStoryboardSegue* seque =
      [[UIStoryboardSegue alloc] initWithIdentifier:@"ConnectToHost"
                                             source:controller_
                                        destination:destination];

  [controller_ prepareForSegue:seque sender:nil];

  ASSERT_NSEQ(HostRefreshTestHelper::HostNameTest, destination.host.hostName);
}

}  // namespace remoting