// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_UTILITY_H_
#define REMOTING_IOS_UTILITY_H_

#import <Foundation/Foundation.h>

#include "base/memory/scoped_ptr.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

#import "remoting/ios/bridge/host_proxy.h"

typedef struct {
  scoped_ptr<webrtc::BasicDesktopFrame> image;
  scoped_ptr<webrtc::DesktopVector> offset;
} GLRegion;

@interface Utility : NSObject

+ (BOOL)isPad;

+ (BOOL)isInLandscapeMode;

// Return the resolution in respect to orientation
+ (CGSize)getOrientatedSize:(CGSize)size
    shouldWidthBeLongestSide:(BOOL)shouldWidthBeLongestSide;

+ (void)showAlert:(NSString*)title message:(NSString*)message;

+ (NSString*)appVersionNumberDisplayString;

// GL Binding Context requires some specific flags for the type of textures
// being drawn
+ (void)bindTextureForIOS:(GLuint)glName;

// Sometimes its necessary to read gl errors.  This is called in various places
// while working in the GL Context
+ (void)logGLErrorCode:(NSString*)funcName;

+ (void)drawSubRectToGLFromRectOfSize:(const webrtc::DesktopSize&)rectSize
                              subRect:(const webrtc::DesktopRect&)subRect
                                 data:(const uint8_t*)data;

+ (void)moveMouse:(HostProxy*)controller at:(const webrtc::DesktopVector&)point;

+ (void)leftClickOn:(HostProxy*)controller
                 at:(const webrtc::DesktopVector&)point;

+ (void)middleClickOn:(HostProxy*)controller
                   at:(const webrtc::DesktopVector&)point;

+ (void)rightClickOn:(HostProxy*)controller
                  at:(const webrtc::DesktopVector&)point;

+ (void)mouseScroll:(HostProxy*)controller
                 at:(const webrtc::DesktopVector&)point
              delta:(const webrtc::DesktopVector&)delta;

@end

#endif  // REMOTING_IOS_UTILITY_H_
