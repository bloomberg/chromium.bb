// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/ui/pin_entry_view_controller.h"

#import "remoting/ios/utility.h"

@implementation PinEntryViewController

@synthesize delegate = _delegate;
@synthesize shouldPrompt = _shouldPrompt;
@synthesize pairingSupported = _pairingSupported;

// Override UIViewController
- (id)initWithNibName:(NSString*)nibNameOrNil bundle:(NSBundle*)nibBundleOrNil {
  // NibName is the * part of your *.xib file

  if ([Utility isPad]) {
    self = [super initWithNibName:@"pin_entry_view_controller_ipad" bundle:nil];
  } else {
    self =
        [super initWithNibName:@"pin_entry_view_controller_iphone" bundle:nil];
  }
  if (self) {
    // Custom initialization
  }
  return self;
}

// Override UIViewController
// Controls are not created immediately, properties must be set before the form
// is displayed
- (void)viewWillAppear:(BOOL)animated {
  _host.text = _hostName;

  [_switchAskAgain setOn:!_shouldPrompt];

  // TODO (aboone) The switch is being hidden in all cases, this functionality
  // is not scheduled for QA yet.
  // if (!_pairingSupported) {
  _switchAskAgain.hidden = YES;
  _shouldSavePin.hidden = YES;
  _switchAskAgain.enabled = NO;
  //}
  [_hostPin becomeFirstResponder];
}

// @protocol UITextFieldDelegate, called when the 'enter' key is pressed
- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [textField resignFirstResponder];
  if (textField == _hostPin)
    [self buttonConnectClicked:self];
  return YES;
}

- (IBAction)buttonCancelClicked:(id)sender {
  [_delegate cancelledConnectToHostWithPin:self];
}

- (IBAction)buttonConnectClicked:(id)sender {
  [_delegate connectToHostWithPin:self
                          hostPin:_hostPin.text
                     shouldPrompt:!_switchAskAgain.isOn];
}

@end
