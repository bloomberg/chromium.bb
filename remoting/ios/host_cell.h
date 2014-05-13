// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_HOST_CELL_H_
#define REMOTING_IOS_HOST_CELL_H_

#import <UIKit/UIKit.h>

// HostCell represents a Host as a row in a tableView, where the row
// contains a single cell.  Several button and outlet are reserved here for
// future functionality
@interface HostCell : UITableViewCell

@property(weak, nonatomic) IBOutlet UILabel* labelHostName;
@property(weak, nonatomic) IBOutlet UILabel* labelStatus;

@end

#endif  // REMOTING_IOS_HOST_CELL_H_
