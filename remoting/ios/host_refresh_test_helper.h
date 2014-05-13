// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_HOST_REFRESH_TEST_HELPER_H_
#define REMOTING_IOS_HOST_REFRESH_TEST_HELPER_H_

#import <Foundation/Foundation.h>

namespace remoting {

class HostRefreshTestHelper {
 public:
  constexpr static NSString* CloseTag = @"\",";

  constexpr static NSString* CreatedTimeTag = @"\"createdTime\":\"";
  constexpr static NSString* HostIdTag = @"\"hostId\":\"";
  constexpr static NSString* HostNameTag = @"\"hostName\":\"";
  constexpr static NSString* HostVersionTag = @"\"hostVersion\":\"";
  constexpr static NSString* KindTag = @"\"kind\":\"";
  constexpr static NSString* JabberIdTag = @"\"jabberId\":\"";
  constexpr static NSString* PublicKeyTag = @"\"publicKey\":\"";
  constexpr static NSString* StatusTag = @"\"status\":\"";
  constexpr static NSString* UpdatedTimeTag = @"\"updatedTime\":\"";

  constexpr static NSString* CreatedTimeTest = @"2000-01-01T00:00:01.000Z";
  constexpr static NSString* HostIdTest = @"Host1";
  constexpr static NSString* HostNameTest = @"HostName1";
  constexpr static NSString* HostVersionTest = @"2.22.5.4";
  constexpr static NSString* KindTest = @"chromoting#host";
  constexpr static NSString* JabberIdTest = @"JabberingOn";
  constexpr static NSString* PublicKeyTest = @"AAAAABBBBBZZZZZ";
  constexpr static NSString* StatusTest = @"TESTING";
  constexpr static NSString* UpdatedTimeTest = @"2004-01-01T00:00:01.000Z";

  static NSMutableData* GetHostList(int numHosts) {
    return [NSMutableData
        dataWithData:[GetMultipleHosts(numHosts)
                         dataUsingEncoding:NSUTF8StringEncoding]];
  }

  static NSMutableData* GetHostList(NSString* hostList) {
    return [NSMutableData
        dataWithData:[hostList dataUsingEncoding:NSUTF8StringEncoding]];
  }

  static NSString* GetMultipleHosts(int numHosts) {
    NSString* client = [NSString
        stringWithFormat:
            @"%@%@%@%@%@%@%@%@%@%@%@%@%@%@%@%@%@%@%@%@%@%@%@%@%@%@%@%@%@%@%@",
            @"{",
            CreatedTimeTag,
            CreatedTimeTest,
            CloseTag,
            HostIdTag,
            HostIdTest,
            CloseTag,
            HostNameTag,
            HostNameTest,
            CloseTag,
            HostNameTag,
            HostNameTest,
            CloseTag,
            HostVersionTag,
            HostVersionTest,
            CloseTag,
            KindTag,
            KindTest,
            CloseTag,
            JabberIdTag,
            JabberIdTest,
            CloseTag,
            PublicKeyTag,
            PublicKeyTest,
            CloseTag,
            StatusTag,
            StatusTest,
            CloseTag,
            UpdatedTimeTag,
            UpdatedTimeTest,
            @"\"}"];

    NSMutableString* hostList = [NSMutableString
        stringWithString:
            @"{\"data\":{\"kind\":\"chromoting#hostList\",\"items\":["];

    for (int i = 0; i < numHosts; i++) {
      [hostList appendString:client];
      if (i < numHosts - 1) {
        [hostList appendString:@","];  // common separated
      }
    }

    [hostList appendString:@"]}}"];

    return [hostList copy];
  }
};

}  // namespace remoting

#endif  // REMOTING_IOS_HOST_REFRESH_TEST_HELPER_H_