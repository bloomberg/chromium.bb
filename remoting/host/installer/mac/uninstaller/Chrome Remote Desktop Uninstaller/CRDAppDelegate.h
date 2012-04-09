//
//  CRDAppDelegate.h
//  Chrome Remote Desktop Uninstaller
//
//  Created by Gary Kacmarcik on 4/3/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>

@interface CRDAppDelegate : NSObject <NSApplicationDelegate>

@property (assign) IBOutlet NSWindow *window;

- (IBAction)uninstall:(NSButton *)sender;
- (IBAction)cancel:(id)sender;

- (IBAction)handleMenuClose:(NSMenuItem *)sender;

@end
