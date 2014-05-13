// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/host_refresh.h"

#import "remoting/ios/authorize.h"
#import "remoting/ios/host.h"
#import "remoting/ios/utility.h"

namespace {
NSString* kDefaultErrorMessage = @"The Host list refresh is not available at "
    @"this time.  Please try again later.";
}  // namespace

@interface HostRefresh (Private)
- (void)authentication:(GTMOAuth2Authentication*)auth
               request:(NSMutableURLRequest*)request
                 error:(NSError*)error;
- (void)formatErrorMessage:(NSString*)error;
- (void)notifyDelegate;
@end

// Logic flow begins with refreshHostList, and continues until an error occurs,
// or the host list is returned to the delegate
@implementation HostRefresh

@synthesize jsonData = _jsonData;
@synthesize errorMessage = _errorMessage;
@synthesize delegate = _delegate;

// Override default constructor and initialize internals
- (id)init {
  self = [super init];
  if (self) {
    _jsonData = [[NSMutableData alloc] init];
  }
  return self;
}

// Begin the authentication and authorization process.  Begin the process by
// creating an oAuth2 request to google api's including the needed scopes to
// fetch the users host list.
- (void)refreshHostList:(GTMOAuth2Authentication*)authReq
               delegate:(id<HostRefreshDelegate>)delegate {

  CHECK(_delegate == nil);  // Do not reuse an instance of this class

  _delegate = delegate;

  [Authorize beginRequest:authReq
                 delegate:self
        didFinishSelector:@selector(authentication:request:error:)];
}

// Handle completion of the authorization process. Append service credentials
// for jabber.  If an error occurred, notify user.
- (void)authentication:(NSObject*)auth
               request:(NSMutableURLRequest*)request
                 error:(NSError*)error {
  if (error != nil) {
    [self formatErrorMessage:error.localizedDescription];
  } else {
    // Add credentials for service
    [Authorize appendCredentials:request];

    // Begin connection, the returned reference is not useful right now and
    // marked as __unused
    __unused NSURLConnection* connection =
        [[NSURLConnection alloc] initWithRequest:request delegate:self];
  }
}

// @protocol NSURLConnectionDelegate, handle any error during connection
- (void)connection:(NSURLConnection*)connection
    didFailWithError:(NSError*)error {
  [self formatErrorMessage:[error localizedDescription]];

  [self notifyDelegate];
}

// @protocol NSURLConnectionDataDelegate, may be called async multiple times.
// Each call appends the new data to the known data until completed.
- (void)connection:(NSURLConnection*)connection didReceiveData:(NSData*)data {
  [_jsonData appendData:data];
}

// @protocol NSURLConnectionDataDelegate
// Ensure connection succeeded: HTTP 200 OK
- (void)connection:(NSURLConnection*)connection
    didReceiveResponse:(NSURLResponse*)response {
  NSHTTPURLResponse* httpResponse = (NSHTTPURLResponse*)response;
  if ([response respondsToSelector:@selector(allHeaderFields)]) {
    NSNumber* responseCode =
        [[NSNumber alloc] initWithInteger:[httpResponse statusCode]];
    if (responseCode.intValue != 200) {
      [self formatErrorMessage:[NSString
                                   stringWithFormat:@"HTTP STATUS CODE: %d",
                                                    [httpResponse statusCode]]];
    }
  }
}

// @protocol NSURLConnectionDataDelegate handle a completed connection, parse
// received data, and return host list to delegate
- (void)connectionDidFinishLoading:(NSURLConnection*)connection {
  [self notifyDelegate];
}

// Store a formatted error message to return later
- (void)formatErrorMessage:(NSString*)error {
  _errorMessage = kDefaultErrorMessage;
  if (error != nil && error.length > 0) {
    _errorMessage = [_errorMessage
        stringByAppendingString:[@" " stringByAppendingString:error]];
  }
}

// The connection has finished, call to delegate
- (void)notifyDelegate {
  if (_jsonData.length == 0 && _errorMessage == nil) {
    [self formatErrorMessage:nil];
  }

  [_delegate hostListRefresh:[Host parseListFromJSON:_jsonData]
                errorMessage:_errorMessage];
}
@end
