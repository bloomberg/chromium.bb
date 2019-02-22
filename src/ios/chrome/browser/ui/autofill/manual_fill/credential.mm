// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/credential.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ManualFillCredential

@synthesize username = _username;
@synthesize password = _password;
@synthesize siteName = _siteName;
@synthesize host = _host;

- (instancetype)initWithUsername:(NSString*)username
                        password:(NSString*)password
                        siteName:(NSString*)siteName
                            host:(NSString*)host {
  self = [super init];
  if (self) {
    _host = [host copy];
    _siteName = [siteName copy];
    _username = [username copy];
    _password = [password copy];
  }
  return self;
}

- (BOOL)isEqual:(id)object {
  if (!object) {
    return NO;
  }
  if (self == object) {
    return YES;
  }
  if (![object isMemberOfClass:[ManualFillCredential class]]) {
    return NO;
  }
  ManualFillCredential* otherObject = (ManualFillCredential*)object;
  if (![otherObject.host isEqual:self.host]) {
    return NO;
  }
  if (![otherObject.username isEqual:self.username]) {
    return NO;
  }
  if (![otherObject.password isEqual:self.password]) {
    return NO;
  }
  if (![otherObject.siteName isEqual:self.siteName]) {
    return NO;
  }
  return YES;
}

- (NSUInteger)hash {
  return [self.host hash] ^ [self.username hash] ^ [self.password hash];
}

- (NSString*)description {
  return [NSString
      stringWithFormat:@"<%@ (%p): username: %@, siteName: %@, host: %@>",
                       NSStringFromClass([self class]), self, self.username,
                       self.siteName, self.host];
}
@end
