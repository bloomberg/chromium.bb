// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/commands/bookmark_add_command.h"

#import "ios/chrome/browser/ui/util/url_with_title.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BookmarkAddCommand ()

@property(nonatomic, strong) NSArray<URLWithTitle*>* URLs;
@property(nonatomic, assign) BOOL presentFolderChooser;

@end

@implementation BookmarkAddCommand

@synthesize URLs = _URLs;
@synthesize presentFolderChooser = _presentFolderChooser;

- (instancetype)initWithURL:(const GURL&)URL
                      title:(NSString*)title
       presentFolderChooser:(BOOL)presentFolderChooser {
  if (self = [super init]) {
    _URLs = @[ [[URLWithTitle alloc] initWithURL:URL title:title] ];
    _presentFolderChooser = presentFolderChooser;
  }
  return self;
}

- (instancetype)initWithURLs:(NSArray<URLWithTitle*>*)URLs {
  if (self = [super init]) {
    _URLs = [URLs copy];
    _presentFolderChooser = YES;
  }
  return self;
}

@end
