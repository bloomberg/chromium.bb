// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_APP_CLIENT_CONNECTION_VIEW_CONTROLLER_H_
#define REMOTING_CLIENT_IOS_APP_CLIENT_CONNECTION_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// This enumerated the differnt modes this Client Connection View can be in.
typedef NS_ENUM(NSInteger, ClientConnectionViewState) {
  ClientViewConnecting,
  ClientViewPinPrompt,
  ClientViewConnected,
  ClientViewClosed,
};

// The host connection view controller delegate provides feedback for state
// changes on Host Connection that the calling view should respond to.
@protocol ClientConnectionViewControllerDelegate<NSObject>

// Notifies the delegate the client is connected to the host.
- (void)clientConnected;

// Gets the current host name the client is attempting to connect to.
- (NSString*)getConnectingHostName;

@end

// This is the view that shows the user feedback while the client connection is
// being established. If requested the view can also display the pin entry view.
// State communication for this view is handled by NSNotifications, it listens
// to kHostSessionStatusChanged events on the default NSNotificationCenter.
// Internally the notification is tied to [self setState] so view setup will
// work the same way if state is set directly.
@interface ClientConnectionViewController : UIViewController

// Setting state will change the view
@property(nonatomic, assign) ClientConnectionViewState state;

// This delegate is used to ask for Host Name and to notify when the connection
// has been established.
@property(weak, nonatomic) id<ClientConnectionViewControllerDelegate> delegate;

@end

#endif  // REMOTING_CLIENT_IOS_APP_CLIENT_CONNECTION_VIEW_CONTROLLER_H_
