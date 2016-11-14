// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/client/ios/utility.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"

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
  UIAlertController* alert;
  alert =
      [UIAlertController alertControllerWithTitle:title
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];

  UIAlertAction* defaultAction =
      [UIAlertAction actionWithTitle:@"OK"
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* action){
                             }];

  [alert addAction:defaultAction];
  [[UIApplication sharedApplication].delegate.window.rootViewController
      presentViewController:alert
                   animated:YES
                 completion:nil];
}

+ (NSString*)appVersionNumberDisplayString {
  NSDictionary* infoDictionary = [[NSBundle mainBundle] infoDictionary];

  NSString* version =
      [infoDictionary objectForKey:@"CFBundleShortVersionString"];

  return [NSString stringWithFormat:@"Version %@", version];
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
    VLOG_IF(0, errorCode) << "GLerror in " <<
        [funcName cStringUsingEncoding:NSUTF8StringEncoding] << ": "
            << std::hex << errorCode << std::dec;
  }
}

+ (void)drawSubRectToGLFromRectOfSize:(const webrtc::DesktopSize&)rectSize
                              subRect:(const webrtc::DesktopRect&)subRect
                                 data:(const uint8_t*)data {
  DCHECK(rectSize.width() >= subRect.width());
  DCHECK(rectSize.height() >= subRect.height());
  DCHECK(subRect.left() >= 0);
  DCHECK(subRect.top() >= 0);
  DCHECK(data);

  glTexSubImage2D(GL_TEXTURE_2D, 0, subRect.left(), subRect.top(),
                  subRect.width(), subRect.height(), GL_RGBA, GL_UNSIGNED_BYTE,
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

+ (NSString*)localizedStringForKey:(NSString*)key dummyId:(int)dummyId {
  return NSLocalizedString(key, nil);
}

+ (NSString*)stringWithL10nFormat:(NSString*)format
                 withReplacements:(NSArray*)replacements {
  std::string format_str(base::SysNSStringToUTF8(format));
  std::vector<std::string> replacements_str;

  for (NSString* replacement in replacements) {
    DCHECK([replacement isKindOfClass:[NSString class]]);
    replacements_str.push_back(base::SysNSStringToUTF8(replacement));
  }

  std::string formatted_str(
      base::ReplaceStringPlaceholders(format_str, replacements_str, nullptr));
  return base::SysUTF8ToNSString(formatted_str);
}

@end
