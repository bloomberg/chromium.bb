// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/ui/host_view_controller.h"

#include <OpenGLES/ES2/gl.h>

#import "remoting/ios/data_store.h"

namespace {

// TODO (aboone) Some of the layout is not yet set in stone, so variables have
// been used to position and turn items on and off.  Eventually these may be
// stabilized and removed.

// Scroll speed multiplier for mouse wheel
const static int kMouseWheelSensitivity = 20;

// Area the navigation bar consumes when visible in pixels
const static int kTopMargin = 20;
// Area the footer consumes when visible (no footer currently exists)
const static int kBottomMargin = 0;

}  // namespace

@interface HostViewController (Private)
- (void)setupGL;
- (void)tearDownGL;
- (void)goBack;
- (void)updateLabels;
- (BOOL)isToolbarHidden;
- (void)updatePanVelocityShouldCancel:(bool)canceled;
- (void)orientationChanged:(NSNotification*)note;
- (void)applySceneChange:(CGPoint)translation scaleBy:(float)ratio;
- (void)showToolbar:(BOOL)visible;
@end

@implementation HostViewController

@synthesize host = _host;
@synthesize userEmail = _userEmail;
@synthesize userAuthorizationToken = _userAuthorizationToken;

// Override UIViewController
- (void)viewDidLoad {
  [super viewDidLoad];

  _context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
  DCHECK(_context);
  static_cast<GLKView*>(self.view).context = _context;

  [_keyEntryView setDelegate:self];

  _clientToHostProxy = [[HostProxy alloc] init];

  // There is a 1 pixel top border which is actually the background not being
  // covered.  There is no obvious way to remove that pixel 'border'.  Set the
  // background clear, and also reset the backgroundimage and shawdowimage to an
  // empty image any time the view is moved.
  _hiddenToolbar.backgroundColor = [UIColor clearColor];
  if ([_hiddenToolbar respondsToSelector:@selector(setBackgroundImage:
                                                   forToolbarPosition:
                                                           barMetrics:)]) {
    [_hiddenToolbar setBackgroundImage:[UIImage new]
                    forToolbarPosition:UIToolbarPositionAny
                            barMetrics:UIBarMetricsDefault];
  }
  if ([_hiddenToolbar
          respondsToSelector:@selector(setShadowImage:forToolbarPosition:)]) {
    [_hiddenToolbar setShadowImage:[UIImage new]
                forToolbarPosition:UIToolbarPositionAny];
  }

  // 1/2 circle rotation for an icon ~ 180 degree ~ 1 radian
  _barBtnNavigation.imageView.transform = CGAffineTransformMakeRotation(M_PI);

  _scene = [[SceneView alloc] init];
  [_scene setMarginsFromLeft:0 right:0 top:kTopMargin bottom:kBottomMargin];
  _desktop = [[DesktopTexture alloc] init];
  _mouse = [[CursorTexture alloc] init];

  _glBufferLock = [[NSLock alloc] init];
  _glCursorLock = [[NSLock alloc] init];

  [_scene
      setContentSize:[Utility getOrientatedSize:self.view.bounds.size
                         shouldWidthBeLongestSide:[Utility isInLandscapeMode]]];
  [self showToolbar:YES];
  [self updateLabels];

  [self setupGL];

  [_singleTapRecognizer requireGestureRecognizerToFail:_twoFingerTapRecognizer];
  [_twoFingerTapRecognizer
      requireGestureRecognizerToFail:_threeFingerTapRecognizer];
  //[_pinchRecognizer requireGestureRecognizerToFail:_twoFingerTapRecognizer];
  [_panRecognizer requireGestureRecognizerToFail:_singleTapRecognizer];
  [_threeFingerPanRecognizer
      requireGestureRecognizerToFail:_threeFingerTapRecognizer];
  //[_pinchRecognizer requireGestureRecognizerToFail:_threeFingerPanRecognizer];

  // Subscribe to changes in orientation
  [[UIDevice currentDevice] beginGeneratingDeviceOrientationNotifications];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(orientationChanged:)
             name:UIDeviceOrientationDidChangeNotification
           object:[UIDevice currentDevice]];
}

- (void)setupGL {
  [EAGLContext setCurrentContext:_context];

  _effect = [[GLKBaseEffect alloc] init];
  [Utility logGLErrorCode:@"setupGL begin"];

  // Initialize each texture
  [_desktop bindToEffect:[_effect texture2d0]];
  [_mouse bindToEffect:[_effect texture2d1]];
  [Utility logGLErrorCode:@"setupGL textureComplete"];
}

// Override UIViewController
- (void)viewDidUnload {
  [super viewDidUnload];
  [self tearDownGL];

  if ([EAGLContext currentContext] == _context) {
    [EAGLContext setCurrentContext:nil];
  }
  _context = nil;
}

- (void)tearDownGL {
  [EAGLContext setCurrentContext:_context];

  // Release Textures
  [_desktop releaseTexture];
  [_mouse releaseTexture];
}

// Override UIViewController
- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:NO];
  [self.navigationController setNavigationBarHidden:YES animated:YES];
  [self updateLabels];
  if (![_clientToHostProxy isConnected]) {
    [_busyIndicator startAnimating];

    [_clientToHostProxy connectToHost:_userEmail
                            authToken:_userAuthorizationToken
                             jabberId:_host.jabberId
                               hostId:_host.hostId
                            publicKey:_host.publicKey
                             delegate:self];
  }
}

// Override UIViewController
- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:NO];
  NSArray* viewControllers = self.navigationController.viewControllers;
  if (viewControllers.count > 1 &&
      [viewControllers objectAtIndex:viewControllers.count - 2] == self) {
    // View is disappearing because a new view controller was pushed onto the
    // stack
  } else if ([viewControllers indexOfObject:self] == NSNotFound) {
    // View is disappearing because it was popped from the stack
    [_clientToHostProxy disconnectFromHost];
  }
}

// "Back" goes to the root controller for now
- (void)goBack {
  [self.navigationController popToRootViewControllerAnimated:YES];
}

// @protocol PinEntryViewControllerDelegate
// Return the PIN input by User, indicate if the User should be prompted to
// re-enter the pin in the future
- (void)connectToHostWithPin:(UIViewController*)controller
                     hostPin:(NSString*)hostPin
                shouldPrompt:(BOOL)shouldPrompt {
  const HostPreferences* hostPrefs =
      [[DataStore sharedStore] getHostForId:_host.hostId];
  if (!hostPrefs) {
    hostPrefs = [[DataStore sharedStore] createHost:_host.hostId];
  }
  if (hostPrefs) {
    hostPrefs.hostPin = hostPin;
    hostPrefs.askForPin = [NSNumber numberWithBool:shouldPrompt];
    [[DataStore sharedStore] saveChanges];
  }

  [[controller presentingViewController] dismissViewControllerAnimated:NO
                                                            completion:nil];

  [_clientToHostProxy authenticationResponse:hostPin createPair:!shouldPrompt];
}

// @protocol PinEntryViewControllerDelegate
// Returns if the user canceled while entering their PIN
- (void)cancelledConnectToHostWithPin:(UIViewController*)controller {
  [[controller presentingViewController] dismissViewControllerAnimated:NO
                                                            completion:nil];

  [self goBack];
}

- (void)setHostDetails:(Host*)host
             userEmail:(NSString*)userEmail
    authorizationToken:(NSString*)authorizationToken {
  DCHECK(host.jabberId);
  _host = host;
  _userEmail = userEmail;
  _userAuthorizationToken = authorizationToken;
}

// Set various labels on the form for iPad vs iPhone, and orientation
- (void)updateLabels {
  if (![Utility isPad] && ![Utility isInLandscapeMode]) {
    [_barBtnDisconnect setTitle:@"" forState:(UIControlStateNormal)];
    [_barBtnCtrlAltDel setTitle:@"CtAtD" forState:UIControlStateNormal];
  } else {
    [_barBtnCtrlAltDel setTitle:@"Ctrl+Alt+Del" forState:UIControlStateNormal];

    NSString* hostStatus = _host.hostName;
    if (![_statusMessage isEqual:@"Connected"]) {
      hostStatus = [NSString
          stringWithFormat:@"%@ - %@", _host.hostName, _statusMessage];
    }
    [_barBtnDisconnect setTitle:hostStatus forState:UIControlStateNormal];
  }

  [_barBtnDisconnect sizeToFit];
  [_barBtnCtrlAltDel sizeToFit];
}

// Resize the view of the desktop - Zoom in/out.  This can occur during a Pan.
- (IBAction)pinchGestureTriggered:(UIPinchGestureRecognizer*)sender {
  if ([sender state] == UIGestureRecognizerStateChanged) {
    [self applySceneChange:CGPointMake(0.0, 0.0) scaleBy:sender.scale];

    sender.scale = 1.0;  // reset scale so next iteration is a relative ratio
  }
}

- (IBAction)tapGestureTriggered:(UITapGestureRecognizer*)sender {
  if ([_scene containsTouchPoint:[sender locationInView:self.view]]) {
    [Utility leftClickOn:_clientToHostProxy at:_scene.mousePosition];
  }
}

// Change position of scene.  This can occur during a pinch or longpress.
// Or perform a Mouse Wheel Scroll
- (IBAction)panGestureTriggered:(UIPanGestureRecognizer*)sender {
  CGPoint translation = [sender translationInView:self.view];

  // If we start with 2 touches, and the pinch gesture is not in progress yet,
  // then disable it, so mouse scrolling and zoom do not occur at the same
  // time.
  if ([sender numberOfTouches] == 2 &&
      [sender state] == UIGestureRecognizerStateBegan &&
      !(_pinchRecognizer.state == UIGestureRecognizerStateBegan ||
        _pinchRecognizer.state == UIGestureRecognizerStateChanged)) {
    _pinchRecognizer.enabled = NO;
  }

  if (!_pinchRecognizer.enabled) {
    // Began with 2 touches, so this is a scroll event
    translation.x *= kMouseWheelSensitivity;
    translation.y *= kMouseWheelSensitivity;
    [Utility mouseScroll:_clientToHostProxy
                      at:_scene.mousePosition
                   delta:webrtc::DesktopVector(translation.x, translation.y)];
  } else {
    // Did not begin with 2 touches, doing a pan event
    if ([sender state] == UIGestureRecognizerStateChanged) {
      CGPoint translation = [sender translationInView:self.view];

      [self applySceneChange:translation scaleBy:1.0];

    } else if ([sender state] == UIGestureRecognizerStateEnded) {
      // After user removes their fingers from the screen, apply an acceleration
      // effect
      [_scene setPanVelocity:[sender velocityInView:self.view]];
    }
  }

  // Finished the event chain
  if (!([sender state] == UIGestureRecognizerStateBegan ||
        [sender state] == UIGestureRecognizerStateChanged)) {
    _pinchRecognizer.enabled = YES;
  }

  // Reset translation so next iteration is relative.
  [sender setTranslation:CGPointZero inView:self.view];
}

// Click-Drag mouse operation.  This can occur during a Pan.
- (IBAction)longPressGestureTriggered:(UILongPressGestureRecognizer*)sender {

  if ([sender state] == UIGestureRecognizerStateBegan) {
    [_clientToHostProxy mouseAction:_scene.mousePosition
                         wheelDelta:webrtc::DesktopVector(0, 0)
                        whichButton:1
                         buttonDown:YES];
  } else if (!([sender state] == UIGestureRecognizerStateBegan ||
               [sender state] == UIGestureRecognizerStateChanged)) {
    [_clientToHostProxy mouseAction:_scene.mousePosition
                         wheelDelta:webrtc::DesktopVector(0, 0)
                        whichButton:1
                         buttonDown:NO];
  }
}

- (IBAction)twoFingerTapGestureTriggered:(UITapGestureRecognizer*)sender {
  if ([_scene containsTouchPoint:[sender locationInView:self.view]]) {
    [Utility rightClickOn:_clientToHostProxy at:_scene.mousePosition];
  }
}

- (IBAction)threeFingerTapGestureTriggered:(UITapGestureRecognizer*)sender {

  if ([_scene containsTouchPoint:[sender locationInView:self.view]]) {
    [Utility middleClickOn:_clientToHostProxy at:_scene.mousePosition];
  }
}

- (IBAction)threeFingerPanGestureTriggered:(UIPanGestureRecognizer*)sender {
  if ([sender state] == UIGestureRecognizerStateChanged) {
    CGPoint translation = [sender translationInView:self.view];
    if (translation.y > 0) {
      // Swiped down
      [self showToolbar:YES];
    } else if (translation.y < 0) {
      // Swiped up
      [_keyEntryView becomeFirstResponder];
      [self updateLabels];
    }
    [sender setTranslation:CGPointZero inView:self.view];
  }
}

- (IBAction)barBtnNavigationBackPressed:(id)sender {
  [self goBack];
}

- (IBAction)barBtnKeyboardPressed:(id)sender {
  if ([_keyEntryView isFirstResponder]) {
    [_keyEntryView endEditing:NO];
  } else {
    [_keyEntryView becomeFirstResponder];
  }

  [self updateLabels];
}

- (IBAction)barBtnToolBarHidePressed:(id)sender {
  [self showToolbar:[self isToolbarHidden]];  // Toolbar is either on
                                              // screen or off screen
}

- (IBAction)barBtnCtrlAltDelPressed:(id)sender {
  [_keyEntryView ctrlAltDel];
}

// Override UIResponder
// When any gesture begins, remove any acceleration effects currently being
// applied.  Example, Panning view and let it shoot off into the distance, but
// then I see a spot I'm interested in so I will touch to capture that locations
// focus.
- (void)touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event {
  [self updatePanVelocityShouldCancel:YES];
  [super touchesBegan:touches withEvent:event];
}

// @protocol UIGestureRecognizerDelegate
// Allow panning and zooming to occur simultaneously.
// Allow panning and long press to occur simultaneously.
// Pinch requires 2 touches, and long press requires a single touch, so they are
// mutually exclusive regardless of if panning is the initiating gesture
- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  if (gestureRecognizer == _pinchRecognizer ||
      (gestureRecognizer == _panRecognizer)) {
    if (otherGestureRecognizer == _pinchRecognizer ||
        otherGestureRecognizer == _panRecognizer) {
      return YES;
    }
  }

  if (gestureRecognizer == _longPressRecognizer ||
      gestureRecognizer == _panRecognizer) {
    if (otherGestureRecognizer == _longPressRecognizer ||
        otherGestureRecognizer == _panRecognizer) {
      return YES;
    }
  }
  return NO;
}

// @protocol ClientControllerDelegate
// Prompt the user for their PIN if pairing has not already been established
- (void)requestHostPin:(BOOL)pairingSupported {
  BOOL requestPin = YES;
  const HostPreferences* hostPrefs =
      [[DataStore sharedStore] getHostForId:_host.hostId];
  if (hostPrefs) {
    requestPin = [hostPrefs.askForPin boolValue];
    if (!requestPin) {
      if (hostPrefs.hostPin == nil || hostPrefs.hostPin.length == 0) {
        requestPin = YES;
      }
    }
  }
  if (requestPin == YES) {
    PinEntryViewController* pinEntry = [[PinEntryViewController alloc] init];
    [pinEntry setDelegate:self];
    [pinEntry setHostName:_host.hostName];
    [pinEntry setShouldPrompt:YES];
    [pinEntry setPairingSupported:pairingSupported];

    [self presentViewController:pinEntry animated:YES completion:nil];
  } else {
    [_clientToHostProxy authenticationResponse:hostPrefs.hostPin
                                    createPair:pairingSupported];
  }
}

// @protocol ClientControllerDelegate
// Occurs when a connection to a HOST is established successfully
- (void)connected {
  // Everything is good, nothing to do
}

// @protocol ClientControllerDelegate
- (void)connectionStatus:(NSString*)statusMessage {
  _statusMessage = statusMessage;

  if ([_statusMessage isEqual:@"Connection closed"]) {
    [self goBack];
  } else {
    [self updateLabels];
  }
}

// @protocol ClientControllerDelegate
// Occurs when a connection to a HOST has failed
- (void)connectionFailed:(NSString*)errorMessage {
  [_busyIndicator stopAnimating];
  NSString* errorMsg;
  if ([_clientToHostProxy isConnected]) {
    errorMsg = @"Lost Connection";
  } else {
    errorMsg = @"Unable to connect";
  }
  [Utility showAlert:errorMsg message:errorMessage];
  [self goBack];
}

// @protocol ClientControllerDelegate
// Copy the updated regions to a backing store to be consumed by the GL Context
// on a different thread.  A region is stored in disjoint memory locations, and
// must be transformed to a contiguous memory buffer for a GL Texture write.
// /-----\
// |  2-4|  This buffer is 5x3 bytes large, a region exists at bytes 2 to 4 and
// |  7-9|  bytes 7 to 9.  The region is extracted to a new contiguous buffer
// |     |  of 6 bytes in length.
// \-----/
// More than 1 region may exist in the frame from each call, in which case a new
// buffer is created for each region
- (void)applyFrame:(const webrtc::DesktopSize&)size
            stride:(NSInteger)stride
              data:(uint8_t*)data
           regions:(const std::vector<webrtc::DesktopRect>&)regions {
  [_glBufferLock lock];  // going to make changes to |_glRegions|

  if (!_scene.frameSize.equals(size)) {
    // When this is the initial frame, the busyIndicator is still spinning. Now
    // is a good time to stop it.
    [_busyIndicator stopAnimating];

    // If the |_toolbar| is still showing, hide it.
    [self showToolbar:NO];
    [_scene setContentSize:
                [Utility getOrientatedSize:self.view.bounds.size
                    shouldWidthBeLongestSide:[Utility isInLandscapeMode]]];
    [_scene setFrameSize:size];
    [_desktop setTextureSize:size];
    [_mouse setTextureSize:size];
  }

  uint32_t src_stride = stride;

  for (uint32_t i = 0; i < regions.size(); i++) {
    scoped_ptr<GLRegion> region(new GLRegion());

    if (region.get()) {
      webrtc::DesktopRect rect = regions.at(i);

      webrtc::DesktopSize(rect.width(), rect.height());
      region->offset.reset(new webrtc::DesktopVector(rect.left(), rect.top()));
      region->image.reset(new webrtc::BasicDesktopFrame(
          webrtc::DesktopSize(rect.width(), rect.height())));

      if (region->image->data()) {
        uint32_t bytes_per_row =
            region->image->kBytesPerPixel * region->image->size().width();

        uint32_t offset =
            (src_stride * region->offset->y()) +                    // row
            (region->offset->x() * region->image->kBytesPerPixel);  // column

        uint8_t* src_buffer = data + offset;
        uint8_t* dst_buffer = region->image->data();

        // row by row copy
        for (uint32_t j = 0; j < region->image->size().height(); j++) {
          memcpy(dst_buffer, src_buffer, bytes_per_row);
          dst_buffer += bytes_per_row;
          src_buffer += src_stride;
        }
        _glRegions.push_back(region.release());
      }
    }
  }
  [_glBufferLock unlock];  // done making changes to |_glRegions|
}

// @protocol ClientControllerDelegate
// Copy the delivered cursor to a backing store to be consumed by the GL Context
// on a different thread.  Note only the most recent cursor is of importance,
// discard the previous cursor.
- (void)applyCursor:(const webrtc::DesktopSize&)size
            hotspot:(const webrtc::DesktopVector&)hotspot
         cursorData:(uint8_t*)data {

  [_glCursorLock lock];  // going to make changes to |_cursor|

  // MouseCursor takes ownership of DesktopFrame
  [_mouse setCursor:new webrtc::MouseCursor(new webrtc::BasicDesktopFrame(size),
                                            hotspot)];

  if (_mouse.cursor.image().data()) {
    memcpy(_mouse.cursor.image().data(),
           data,
           size.width() * size.height() * _mouse.cursor.image().kBytesPerPixel);
  } else {
    [_mouse setCursor:NULL];
  }

  [_glCursorLock unlock];  // done making changes to |_cursor|
}

// @protocol GLKViewDelegate
// There is quite a few gotchas involved in working with this function.  For
// sanity purposes, I've just assumed calls to the function are on a different
// thread which I've termed GL Context.  Any variables consumed by this function
// should be thread safe.
//
// Clear Screen, update desktop, update cursor, define position, and finally
// present
//
// In general, avoid expensive work in this function to maximize frame rate.
- (void)glkView:(GLKView*)view drawInRect:(CGRect)rect {
  [self updatePanVelocityShouldCancel:NO];

  // Clear to black, to give the background color
  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);

  [Utility logGLErrorCode:@"drawInRect bindBuffer"];

  if (_glRegions.size() > 0 || [_desktop needDraw]) {
    [_glBufferLock lock];

    for (uint32_t i = 0; i < _glRegions.size(); i++) {
      // |_glRegions[i].data| has been properly ordered by [self applyFrame]
      [_desktop drawRegion:_glRegions[i] rect:rect];
    }

    _glRegions.clear();
    [_glBufferLock unlock];
  }

  if ([_mouse needDrawAtPosition:_scene.mousePosition]) {
    [_glCursorLock lock];
    [_mouse drawWithMousePosition:_scene.mousePosition];
    [_glCursorLock unlock];
  }

  [_effect transform].projectionMatrix = _scene.projectionMatrix;
  [_effect transform].modelviewMatrix = _scene.modelViewMatrix;
  [_effect prepareToDraw];

  [Utility logGLErrorCode:@"drawInRect prepareToDrawComplete"];

  [_scene draw];
}

// @protocol KeyInputDelegate
- (void)keyboardDismissed {
  [self updateLabels];
}

// @protocol KeyInputDelegate
// Send keyboard input to HOST
- (void)keyboardActionKeyCode:(uint32_t)keyPressed isKeyDown:(BOOL)keyDown {
  [_clientToHostProxy keyboardAction:keyPressed keyDown:keyDown];
}

- (BOOL)isToolbarHidden {
  return (_toolbar.frame.origin.y < 0);
}

// Update the scene acceleration vector
- (void)updatePanVelocityShouldCancel:(bool)canceled {
  if (canceled) {
    [_scene setPanVelocity:CGPointMake(0, 0)];
  }
  BOOL inMotion = [_scene tickPanVelocity];

  _singleTapRecognizer.enabled = !inMotion;
  _longPressRecognizer.enabled = !inMotion;
}

- (void)applySceneChange:(CGPoint)translation scaleBy:(float)ratio {
  [_scene panAndZoom:translation scaleBy:ratio];
  // Notify HOST that the mouse moved
  [Utility moveMouse:_clientToHostProxy at:_scene.mousePosition];
}

// Callback from NSNotificationCenter when the User changes orientation
- (void)orientationChanged:(NSNotification*)note {
  [_scene
      setContentSize:[Utility getOrientatedSize:self.view.bounds.size
                         shouldWidthBeLongestSide:[Utility isInLandscapeMode]]];
  [self showToolbar:![self isToolbarHidden]];
  [self updateLabels];
}

// Animate |_toolbar| by moving it on or offscreen
- (void)showToolbar:(BOOL)visible {
  CGRect frame = [_toolbar frame];

  _toolBarYPosition.constant = -frame.size.height;
  int topOffset = kTopMargin;

  if (visible) {
    topOffset += frame.size.height;
    _toolBarYPosition.constant = kTopMargin;
  }

  _hiddenToolbarYPosition.constant = topOffset;
  [_scene setMarginsFromLeft:0 right:0 top:topOffset bottom:kBottomMargin];

  // hidden when |_toolbar| is |visible|
  _hiddenToolbar.hidden = (visible == YES);

  [UIView animateWithDuration:0.5
      animations:^{ [self.view layoutIfNeeded]; }
      completion:^(BOOL finished) {// Nothing to do for now
                 }];

  // Center view if needed for any reason.
  // Specificallly, if the top anchor is active.
  [self applySceneChange:CGPointMake(0.0, 0.0) scaleBy:1.0];
}
@end
