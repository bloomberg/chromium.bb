// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_window_ios.h"

#include "base/check.h"
#include "base/format_macros.h"
#import "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/sessions/NSCoder+Compatibility.h"
#import "ios/chrome/browser/sessions/session_features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Serialization keys.
NSString* const kSessionsKey = @"sessions";
NSString* const kSessionsSummaryKey = @"sessionsSummary";
NSString* const kSelectedIndexKey = @"selectedIndex";
NSString* const kSessionStableIdentifierKey = @"stableIdentifier";
NSString* const kSessionCurrentURLKey = @"sessionCurrentURL";
NSString* const kSessionCurrentTitleKey = @"sessionCurrentTitle";

// Returns whether |index| is valid for a SessionWindowIOS with |session_count|
// entries.
BOOL IsIndexValidForSessionCount(NSUInteger index, NSUInteger session_count) {
  return (session_count == 0) ? (index == static_cast<NSUInteger>(NSNotFound))
                              : (index < session_count);
}
}  // namespace

@implementation SessionSummary
@synthesize url = _url;
@synthesize title = _title;
@synthesize stableIdentifier = _stableIdentifier;

- (instancetype)initWithURL:(NSURL*)url
                      title:(NSString*)title
           stableIdentifier:(NSString*)stableIdentifier {
  self = [super init];
  if (self) {
    _url = url;
    _title = title;
    _stableIdentifier = stableIdentifier;
  }
  return self;
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  NSURL* url = [aDecoder decodeObjectForKey:kSessionCurrentURLKey];
  NSString* title = [aDecoder decodeObjectForKey:kSessionCurrentTitleKey];
  NSString* stableIdentifier =
      [aDecoder decodeObjectForKey:kSessionStableIdentifierKey];

  if (!url || !title || !stableIdentifier) {
    return nil;
  }
  return [self initWithURL:url title:title stableIdentifier:stableIdentifier];
}

- (void)encodeWithCoder:(NSCoder*)aCoder {
  [aCoder encodeObject:_url forKey:kSessionCurrentURLKey];
  [aCoder encodeObject:_title forKey:kSessionCurrentTitleKey];
  [aCoder encodeObject:_stableIdentifier forKey:kSessionStableIdentifierKey];
}
@end

@implementation SessionWindowIOS

@synthesize sessions = _sessions;
@synthesize sessionsSummary = _sessionsSummary;
@synthesize tabContents = _tabContents;
@synthesize selectedIndex = _selectedIndex;

- (instancetype)init {
  return [self initWithSessions:@[]
                sessionsSummary:@[]
                    tabContents:@{}
                  selectedIndex:NSNotFound];
}

#pragma mark - Public

- (instancetype)initWithSessions:(NSArray<CRWSessionStorage*>*)sessions
                 sessionsSummary:(NSArray<NSString*>*)sessionsSummary
                     tabContents:(NSDictionary<NSString*, NSData*>*)tabContents
                   selectedIndex:(NSUInteger)selectedIndex {
  DCHECK(sessions);
  DCHECK(IsIndexValidForSessionCount(selectedIndex, [sessions count]));
  DCHECK(!sessionsSummary || sessionsSummary.count == sessions.count);
  self = [super init];
  if (self) {
    _sessions = [sessions copy];
    _sessionsSummary = [sessionsSummary copy];
    _tabContents = [tabContents copy];
    _selectedIndex = selectedIndex;
  }
  return self;
}

#pragma mark - NSCoding

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  NSUInteger selectedIndex = [aDecoder cr_decodeIndexForKey:kSelectedIndexKey];
  NSArray<CRWSessionStorage*>* sessions =
      base::mac::ObjCCast<NSArray<CRWSessionStorage*>>(
          [aDecoder decodeObjectForKey:kSessionsKey]);

  if (!sessions) {
    sessions = @[];
  }

  if (!IsIndexValidForSessionCount(selectedIndex, [sessions count])) {
    if (![sessions count]) {
      selectedIndex = NSNotFound;
    } else {
      selectedIndex = 0;
    }
  }

  return [self initWithSessions:sessions
                sessionsSummary:nil
                    tabContents:nil
                  selectedIndex:selectedIndex];
}

- (void)encodeWithCoder:(NSCoder*)aCoder {
  [aCoder cr_encodeIndex:_selectedIndex forKey:kSelectedIndexKey];
  [aCoder encodeObject:_sessions forKey:kSessionsKey];
  if (sessions::ShouldSaveSessionTabsToSeparateFiles() && _sessionsSummary) {
    [aCoder encodeObject:_sessionsSummary forKey:kSessionsSummaryKey];
  }
}

#pragma mark - Debugging

- (NSString*)description {
  return [NSString stringWithFormat:@"selected index: %" PRIuNS
                                     "\nsessions:\n%@\n",
                                    _selectedIndex, _sessions];
}

@end
