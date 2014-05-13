// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_UI_HOST_VIEW_CONTROLLER_H_
#define REMOTING_IOS_UI_HOST_VIEW_CONTROLLER_H_

#import <GLKit/GLKit.h>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"

#import "remoting/ios/host.h"
#import "remoting/ios/key_input.h"
#import "remoting/ios/utility.h"
#import "remoting/ios/bridge/host_proxy.h"
#import "remoting/ios/ui/desktop_texture.h"
#import "remoting/ios/ui/cursor_texture.h"
#import "remoting/ios/ui/pin_entry_view_controller.h"
#import "remoting/ios/ui/scene_view.h"

@interface HostViewController
    : GLKViewController<PinEntryViewControllerDelegate,
                        KeyInputDelegate,
                        // Communication channel from HOST to CLIENT
                        ClientProxyDelegate,
                        UIGestureRecognizerDelegate,
                        UIToolbarDelegate> {
 @private
  IBOutlet UIActivityIndicatorView* _busyIndicator;
  IBOutlet UIButton* _barBtnDisconnect;
  IBOutlet UIButton* _barBtnKeyboard;
  IBOutlet UIButton* _barBtnNavigation;
  IBOutlet UIButton* _barBtnCtrlAltDel;
  IBOutlet UILongPressGestureRecognizer* _longPressRecognizer;
  IBOutlet UIPanGestureRecognizer* _panRecognizer;
  IBOutlet UIPanGestureRecognizer* _threeFingerPanRecognizer;
  IBOutlet UIPinchGestureRecognizer* _pinchRecognizer;
  IBOutlet UITapGestureRecognizer* _singleTapRecognizer;
  IBOutlet UITapGestureRecognizer* _twoFingerTapRecognizer;
  IBOutlet UITapGestureRecognizer* _threeFingerTapRecognizer;
  IBOutlet UIToolbar* _toolbar;
  IBOutlet UIToolbar* _hiddenToolbar;
  IBOutlet NSLayoutConstraint* _toolBarYPosition;
  IBOutlet NSLayoutConstraint* _hiddenToolbarYPosition;

  KeyInput* _keyEntryView;
  NSString* _statusMessage;

  // The GLES2 context being drawn too.
  EAGLContext* _context;

  // GLKBaseEffect encapsulates the GL Shaders needed to draw at most two
  // textures |_textureIds| given vertex information.  The draw surface consists
  // of two layers (GL Textures). The bottom layer is the desktop of the HOST.
  // The top layer is mostly transparent and is used to overlay the current
  // cursor.
  GLKBaseEffect* _effect;

  // All the details needed to draw our GL Scene, and our two textures.
  SceneView* _scene;
  DesktopTexture* _desktop;
  CursorTexture* _mouse;

  // List of regions and data that have pending draws to |_desktop| .
  ScopedVector<GLRegion> _glRegions;

  // Lock for |_glRegions|, regions are delivered from HOST on another thread,
  // and drawn to |_desktop| from a GL Context thread
  NSLock* _glBufferLock;

  // Lock for |_mouse.cursor|, cursor updates are delivered from HOST on another
  // thread, and drawn to |_mouse| from a GL Context thread
  NSLock* _glCursorLock;

  // Communication channel from CLIENT to HOST
  HostProxy* _clientToHostProxy;
}

// Details for the host and user
@property(nonatomic, readonly) Host* host;
@property(nonatomic, readonly) NSString* userEmail;
@property(nonatomic, readonly) NSString* userAuthorizationToken;

- (void)setHostDetails:(Host*)host
             userEmail:(NSString*)userEmail
    authorizationToken:(NSString*)authorizationToken;

// Zoom in/out
- (IBAction)pinchGestureTriggered:(UIPinchGestureRecognizer*)sender;
// Left mouse click, moves cursor
- (IBAction)tapGestureTriggered:(UITapGestureRecognizer*)sender;
// Scroll the view in 2d
- (IBAction)panGestureTriggered:(UIPanGestureRecognizer*)sender;
// Right mouse click and drag, moves cursor
- (IBAction)longPressGestureTriggered:(UILongPressGestureRecognizer*)sender;
// Right mouse click
- (IBAction)twoFingerTapGestureTriggered:(UITapGestureRecognizer*)sender;
// Middle mouse click
- (IBAction)threeFingerTapGestureTriggered:(UITapGestureRecognizer*)sender;
// Show hidden menus.  Swipe up for keyboard, swipe down for navigation menu
- (IBAction)threeFingerPanGestureTriggered:(UIPanGestureRecognizer*)sender;

// Do navigation 'back'
- (IBAction)barBtnNavigationBackPressed:(id)sender;
// Show keyboard
- (IBAction)barBtnKeyboardPressed:(id)sender;
// Trigger |_toolbar| animation
- (IBAction)barBtnToolBarHidePressed:(id)sender;
// Send Keys for ctrl, atl, delete
- (IBAction)barBtnCtrlAltDelPressed:(id)sender;

@end

#endif  // REMOTING_IOS_UI_HOST_VIEW_CONTROLLER_H_
