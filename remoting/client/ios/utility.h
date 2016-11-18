// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_UTILITY_H_
#define REMOTING_CLIENT_IOS_UTILITY_H_

#import <Foundation/Foundation.h>
#import <GLKit/GLKit.h>
#import <OpenGLES/ES2/gl.h>
#include <memory>

#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

#import "remoting/client/ios/bridge/host_proxy.h"

#define CRD_LOCALIZED_STRING(stringId) \
  [Utility localizedStringForKey:@ #stringId dummyId:stringId]

typedef struct {
  GLKVector2 geometryVertex;
  GLKVector2 textureVertex;
} TexturedVertex;

typedef struct {
  TexturedVertex bl;
  TexturedVertex br;
  TexturedVertex tl;
  TexturedVertex tr;
} TexturedQuad;

typedef struct {
  GLuint name;
  webrtc::DesktopRect rect;
  // The draw surface is a triangle strip (triangles defined by the intersecting
  // vertexes) to create a rectangle surface.
  // 1****3
  // |  / |
  // | /  |
  // 2****4
  // This also determines the resolution of our surface, being a unit (NxN) grid
  // with finite divisions.  For our surface N = 1, and the number of divisions
  // respects the CLIENT's desktop resolution.
  TexturedQuad quad;
} TextureContainer;

typedef struct {
  std::unique_ptr<webrtc::BasicDesktopFrame> image;
  std::unique_ptr<webrtc::DesktopVector> offset;
} GLRegion;

@interface Utility : NSObject

+ (BOOL)isPad;

+ (BOOL)isInLandscapeMode;

// Return the resolution in respect to orientation.
+ (CGSize)getOrientatedSize:(CGSize)size
    shouldWidthBeLongestSide:(BOOL)shouldWidthBeLongestSide;

+ (void)showAlert:(NSString*)title message:(NSString*)message;

+ (NSString*)appVersionNumberDisplayString;

// GL Binding Context requires some specific flags for the type of textures
// being drawn.
+ (void)bindTextureForIOS:(GLuint)glName;

// Sometimes it is necessary to read gl errors.  This is called in various
// places while working in the GL Context.
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

// Wrapper around NSLocalizedString. Don't call this directly. Use
// CRD_LOCALIZED_STRING instead. |dummyId| is ignored. It's used by
// CRD_LOCALIZED_STRING to force the compiler to resolve the constant name.
+ (NSString*)localizedStringForKey:(NSString*)key dummyId:(int)dummyId;

// Objective-C friendly wrapper around ReplaceStringPlaceholders.
+ (NSString*)stringWithL10nFormat:(NSString*)format
                 withReplacements:(NSArray*)replacements;

@end

#endif  // REMOTING_CLIENT_IOS_UTILITY_H_
