// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/pedal_suggestion_wrapper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PedalSuggestionWrapper

- (instancetype)initWithPedal:(id<OmniboxPedal, OmniboxIcon>)pedal {
  self = [super init];
  if (self) {
    _innerPedal = pedal;
  }
  return self;
}

#pragma mark - AutocompleteSuggestion

// Do not expose any pedal, pretend that this is a normal suggestion.
- (id<OmniboxPedal>)pedal {
  return nil;
}

- (BOOL)supportsDeletion {
  return NO;
}
- (BOOL)hasAnswer {
  return NO;
}
- (BOOL)isURL {
  return NO;
}

- (BOOL)isAppendable {
  return NO;
}
- (BOOL)isTabMatch {
  return NO;
}
- (BOOL)isTailSuggestion {
  return NO;
}
- (NSString*)commonPrefix {
  return nil;
}
- (NSInteger)numberOfLines {
  return 1;
}

- (NSAttributedString*)text {
  return [[NSAttributedString alloc] initWithString:self.innerPedal.title];
}

- (NSAttributedString*)detailText {
  return [[NSAttributedString alloc] initWithString:self.innerPedal.subtitle];
}

- (id<OmniboxIcon>)icon {
  return self.innerPedal;
}

@end
