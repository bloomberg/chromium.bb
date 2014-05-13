// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_UI_HOST_LIST_VIEW_CONTROLLER_H_
#define REMOTING_IOS_UI_HOST_LIST_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>
#import <GLKit/GLKit.h>

#import "host_refresh.h"

// HostListViewController presents the user with a list of hosts which has
// been shared from other platforms to connect to
@interface HostListViewController : UIViewController<HostRefreshDelegate,
                                                     UITableViewDelegate,
                                                     UITableViewDataSource> {
 @private
  IBOutlet UITableView* _tableHostList;
  IBOutlet UIButton* _btnAccount;
  IBOutlet UIActivityIndicatorView* _refreshActivityIndicator;
  IBOutlet UIBarButtonItem* _versionInfo;

  NSArray* _hostList;
}

@property(nonatomic, readonly) GTMOAuth2Authentication* authorization;
@property(nonatomic, readonly) NSString* userEmail;

// Triggered by UI 'refresh' button
- (IBAction)btnRefreshHostListPressed:(id)sender;
// Triggered by UI 'log in' button, if user is already logged in then the user
// is logged out and a new session begins by requesting the user to log in,
// possibly with a different account
- (IBAction)btnAccountPressed:(id)sender;

@end

#endif  // REMOTING_IOS_UI_HOST_LIST_VIEW_CONTROLLER_H_