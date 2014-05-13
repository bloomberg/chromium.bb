// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_UI_PIN_ENTRY_VIEW_CONTROLLER_H_
#define REMOTING_IOS_UI_PIN_ENTRY_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// Contract to handle finalization for Pin Prompt
@protocol PinEntryViewControllerDelegate<NSObject>

// Returns with user's Pin.  Pin has not been validated with the server yet.
// |shouldPrompt| indicates whether a prompt should be needed for the next login
// attempt with this host.
- (void)connectToHostWithPin:(UIViewController*)controller
                     hostPin:(NSString*)hostPin
                shouldPrompt:(BOOL)shouldPrompt;

// Returns when the user has cancelled the input, effectively closing the
// connection attempt.
- (void)cancelledConnectToHostWithPin:(UIViewController*)controller;

@end

// Dialog for user's Pin input. If a host has |pairingSupported| then user has
// the option to save a token for authentication.
@interface PinEntryViewController : UIViewController<UITextFieldDelegate> {
 @private
  IBOutlet UIView* _controlView;
  IBOutlet UIButton* _cancelButton;
  IBOutlet UIButton* _connectButton;
  IBOutlet UILabel* _host;
  IBOutlet UISwitch* _switchAskAgain;
  IBOutlet UILabel* _shouldSavePin;
  IBOutlet UITextField* _hostPin;
}

@property(weak, nonatomic) id<PinEntryViewControllerDelegate> delegate;
@property(nonatomic, copy) NSString* hostName;
@property(nonatomic) BOOL shouldPrompt;
@property(nonatomic) BOOL pairingSupported;

- (IBAction)buttonCancelClicked:(id)sender;
- (IBAction)buttonConnectClicked:(id)sender;

@end

#endif  // REMOTING_IOS_UI_PIN_ENTRY_VIEW_CONTROLLER_H_