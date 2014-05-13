// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/host_refresh.h"

#import "base/compiler_specific.h"
#import "testing/gtest_mac.h"

#import "remoting/ios/host.h"
#import "remoting/ios/host_refresh_test_helper.h"

@interface HostRefreshDelegateTester : NSObject<HostRefreshDelegate>

@property(nonatomic) NSArray* hostList;
@property(nonatomic) NSString* errorMessage;

@end

@implementation HostRefreshDelegateTester

@synthesize hostList = _hostList;
@synthesize errorMessage = _errorMessage;

- (void)hostListRefresh:(NSArray*)hostList
           errorMessage:(NSString*)errorMessage {
  _hostList = hostList;
  _errorMessage = errorMessage;
}

- (bool)receivedHosts {
  return (_hostList.count > 0);
}

- (bool)receivedErrorMessage {
  return (_errorMessage != nil);
}

@end

namespace remoting {

namespace {

NSString* kErrorMessageTest = @"TestErrorMessage";

}  // namespace

class HostRefreshTest : public ::testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    hostRefreshProcessor_ = [[HostRefresh allocWithZone:nil] init];
    delegateTester_ = [[HostRefreshDelegateTester alloc] init];
    [hostRefreshProcessor_ setDelegate:delegateTester_];
  }

  void CreateHostList(int numHosts) {
    [hostRefreshProcessor_
        setJsonData:HostRefreshTestHelper::GetHostList(numHosts)];
  }

  NSError* CreateErrorFromString(NSString* message) {
    NSDictionary* errorDictionary = nil;

    if (message != nil) {
      errorDictionary = @{NSLocalizedDescriptionKey : message};
    }

    return [[NSError alloc] initWithDomain:@"HostRefreshTest"
                                      code:EPERM
                                  userInfo:errorDictionary];
  }

  HostRefresh* hostRefreshProcessor_;
  HostRefreshDelegateTester* delegateTester_;
};

TEST_F(HostRefreshTest, ErrorFormatter) {
  [hostRefreshProcessor_ connection:nil
                   didFailWithError:CreateErrorFromString(nil)];
  ASSERT_FALSE(hostRefreshProcessor_.errorMessage == nil);

  [hostRefreshProcessor_ connection:nil
                   didFailWithError:CreateErrorFromString(@"")];
  ASSERT_FALSE(hostRefreshProcessor_.errorMessage == nil);

  [hostRefreshProcessor_ connection:nil
                   didFailWithError:CreateErrorFromString(kErrorMessageTest)];
  ASSERT_TRUE([hostRefreshProcessor_.errorMessage
                  rangeOfString:kErrorMessageTest].location != NSNotFound);
}

TEST_F(HostRefreshTest, JSONParsing) {
  // There were no hosts returned
  CreateHostList(0);
  [hostRefreshProcessor_ connectionDidFinishLoading:nil];
  ASSERT_TRUE(delegateTester_.hostList.count == 0);

  CreateHostList(1);
  [hostRefreshProcessor_ connectionDidFinishLoading:nil];
  ASSERT_TRUE(delegateTester_.hostList.count == 1);

  Host* host = static_cast<Host*>([delegateTester_.hostList objectAtIndex:0]);
  ASSERT_NSEQ(HostRefreshTestHelper::CreatedTimeTest, host.createdTime);
  ASSERT_NSEQ(HostRefreshTestHelper::HostIdTest, host.hostId);
  ASSERT_NSEQ(HostRefreshTestHelper::HostNameTest, host.hostName);
  ASSERT_NSEQ(HostRefreshTestHelper::HostVersionTest, host.hostVersion);
  ASSERT_NSEQ(HostRefreshTestHelper::KindTest, host.kind);
  ASSERT_NSEQ(HostRefreshTestHelper::JabberIdTest, host.jabberId);
  ASSERT_NSEQ(HostRefreshTestHelper::PublicKeyTest, host.publicKey);
  ASSERT_NSEQ(HostRefreshTestHelper::StatusTest, host.status);
  ASSERT_NSEQ(HostRefreshTestHelper::UpdatedTimeTest, host.updatedTime);

  CreateHostList(11);
  [hostRefreshProcessor_ connectionDidFinishLoading:nil];
  ASSERT_TRUE(delegateTester_.hostList.count == 11);

  // An error in parsing returns no hosts
  [hostRefreshProcessor_
      setJsonData:
          [NSMutableData
              dataWithData:
                  [@"{\"dataaaaaafa\":{\"kiffffnd\":\"chromoting#hostList\"}}"
                      dataUsingEncoding:NSUTF8StringEncoding]]];
  [hostRefreshProcessor_ connectionDidFinishLoading:nil];
  ASSERT_TRUE(delegateTester_.hostList.count == 0);
}

TEST_F(HostRefreshTest, HostListDelegateNoList) {
  // Hosts were not processed at all
  [hostRefreshProcessor_ connectionDidFinishLoading:nil];
  ASSERT_FALSE([delegateTester_ receivedHosts]);
  ASSERT_TRUE([delegateTester_ receivedErrorMessage]);

  // There were no hosts returned
  CreateHostList(0);
  [hostRefreshProcessor_ connectionDidFinishLoading:nil];
  ASSERT_FALSE([delegateTester_ receivedHosts]);
  ASSERT_TRUE([delegateTester_ receivedErrorMessage]);
}

TEST_F(HostRefreshTest, HostListDelegateHasList) {
  CreateHostList(1);
  [hostRefreshProcessor_ connectionDidFinishLoading:nil];
  ASSERT_TRUE([delegateTester_ receivedHosts]);
  ASSERT_FALSE([delegateTester_ receivedErrorMessage]);
}

TEST_F(HostRefreshTest, HostListDelegateHasListWithError) {
  CreateHostList(1);

  [hostRefreshProcessor_ connection:nil
                   didFailWithError:CreateErrorFromString(kErrorMessageTest)];

  [hostRefreshProcessor_ connectionDidFinishLoading:nil];
  ASSERT_TRUE([delegateTester_ receivedHosts]);
  ASSERT_TRUE([delegateTester_ receivedErrorMessage]);
}

TEST_F(HostRefreshTest, ConnectionFailed) {
  [hostRefreshProcessor_ connection:nil didFailWithError:nil];
  ASSERT_FALSE([delegateTester_ receivedHosts]);
  ASSERT_TRUE([delegateTester_ receivedErrorMessage]);
}

}  // namespace remoting