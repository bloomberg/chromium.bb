// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_UI_DESKTOP_TEXTURE_H_
#define REMOTING_IOS_UI_DESKTOP_TEXTURE_H_

#import <Foundation/Foundation.h>
#import <GLKit/GLKit.h>

#import "remoting/ios/utility.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

@interface DesktopTexture : NSObject {
 @private
  // GL name
  GLuint _textureId;
  webrtc::DesktopSize _textureSize;
  BOOL _needInitialize;
}

- (const webrtc::DesktopSize&)textureSize;

- (void)setTextureSize:(const webrtc::DesktopSize&)size;

// bind this object to an effect's via the effects properties
- (void)bindToEffect:(GLKEffectPropertyTexture*)effectProperty;

- (BOOL)needDraw;

// draw a region of the texture
- (void)drawRegion:(GLRegion*)region rect:(CGRect)rect;

- (void)releaseTexture;

@end

#endif  // REMOTING_IOS_UI_DESKTOP_TEXTURE_H_