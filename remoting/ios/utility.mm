// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "Utility.h"

@implementation Utility

+ (BOOL)isPad {
  return (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad);
}

+ (BOOL)isInLandscapeMode {
  UIInterfaceOrientation orientation =
      [UIApplication sharedApplication].statusBarOrientation;

  if ((orientation == UIInterfaceOrientationLandscapeLeft) ||
      (orientation == UIInterfaceOrientationLandscapeRight)) {
    return YES;
  }
  return NO;
}

+ (CGSize)getOrientatedSize:(CGSize)size
    shouldWidthBeLongestSide:(BOOL)shouldWidthBeLongestSide {
  if (shouldWidthBeLongestSide && (size.height > size.width)) {
    return CGSizeMake(size.height, size.width);
  }
  return size;
}

+ (void)showAlert:(NSString*)title message:(NSString*)message {
  UIAlertView* alert;
  alert = [[UIAlertView alloc] init];
  alert.title = title;
  alert.message = message;
  alert.delegate = nil;
  [alert addButtonWithTitle:@"OK"];
  [alert show];
}

+ (NSString*)appVersionNumberDisplayString {
  NSDictionary* infoDictionary = [[NSBundle mainBundle] infoDictionary];

  NSString* majorVersion =
      [infoDictionary objectForKey:@"CFBundleShortVersionString"];
  NSString* minorVersion = [infoDictionary objectForKey:@"CFBundleVersion"];

  return [NSString
      stringWithFormat:@"Version %@ (%@)", majorVersion, minorVersion];
}

+ (void)bindTextureForIOS:(GLuint)glName {
  glBindTexture(GL_TEXTURE_2D, glName);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

+ (void)logGLErrorCode:(NSString*)funcName {
  GLenum errorCode = 1;

  while (errorCode != 0) {
    errorCode = glGetError();  // I don't know why this is returning an error
                               // on the first call to this function, but if I
                               // don't read it, then stuff doesn't work...
#if DEBUG
    if (errorCode != 0) {
      NSLog(@"glerror in %@: %X", funcName, errorCode);
    }
#endif  // DEBUG
  }
}

+ (void)drawSubRectToGLFromRectOfSize:(const webrtc::DesktopSize&)rectSize
                              subRect:(const webrtc::DesktopRect&)subRect
                                 data:(const uint8_t*)data {
  DCHECK(rectSize.width() >= subRect.width());
  DCHECK(rectSize.height() >= subRect.height());
  DCHECK(rectSize.width() >= (subRect.left() + subRect.width()));
  DCHECK(rectSize.height() >= (subRect.top() + subRect.height()));
  DCHECK(data);

  glTexSubImage2D(GL_TEXTURE_2D,
                  0,
                  subRect.left(),
                  subRect.top(),
                  subRect.width(),
                  subRect.height(),
                  GL_RGBA,
                  GL_UNSIGNED_BYTE,
                  data);
}

+ (void)moveMouse:(HostProxy*)controller
               at:(const webrtc::DesktopVector&)point {
  [controller mouseAction:point
               wheelDelta:webrtc::DesktopVector(0, 0)
              whichButton:0
               buttonDown:NO];
}

+ (void)leftClickOn:(HostProxy*)controller
                 at:(const webrtc::DesktopVector&)point {
  [controller mouseAction:point
               wheelDelta:webrtc::DesktopVector(0, 0)
              whichButton:1
               buttonDown:YES];
  [controller mouseAction:point
               wheelDelta:webrtc::DesktopVector(0, 0)
              whichButton:1
               buttonDown:NO];
}

+ (void)middleClickOn:(HostProxy*)controller
                   at:(const webrtc::DesktopVector&)point {
  [controller mouseAction:point
               wheelDelta:webrtc::DesktopVector(0, 0)
              whichButton:2
               buttonDown:YES];
  [controller mouseAction:point
               wheelDelta:webrtc::DesktopVector(0, 0)
              whichButton:2
               buttonDown:NO];
}

+ (void)rightClickOn:(HostProxy*)controller
                  at:(const webrtc::DesktopVector&)point {
  [controller mouseAction:point
               wheelDelta:webrtc::DesktopVector(0, 0)
              whichButton:3
               buttonDown:YES];
  [controller mouseAction:point
               wheelDelta:webrtc::DesktopVector(0, 0)
              whichButton:3
               buttonDown:NO];
}

+ (void)mouseScroll:(HostProxy*)controller
                 at:(const webrtc::DesktopVector&)point
              delta:(const webrtc::DesktopVector&)delta {
  [controller mouseAction:point wheelDelta:delta whichButton:0 buttonDown:NO];
}

@end