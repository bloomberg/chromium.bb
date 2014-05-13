// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_UI_CURSOR_TEXTURE_H_
#define REMOTING_IOS_UI_CURSOR_TEXTURE_H_

#import <Foundation/Foundation.h>
#import <GLKit/GLKit.h>

#import "base/memory/scoped_ptr.h"

#import "remoting/ios/utility.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"

@interface CursorTexture : NSObject {
 @private
  // GL name
  GLuint _textureId;
  webrtc::DesktopSize _textureSize;
  BOOL _needInitialize;

  // The current cursor
  scoped_ptr<webrtc::MouseCursor> _cursor;

  BOOL _needCursorRedraw;

  // Rectangle of the most recent cursor drawn to a GL Texture.  On each
  // successive frame when a new cursor is available this region is cleared on
  // the GL Texture, so that the GL Texture is completely transparent again, and
  // the cursor is then redrawn.
  webrtc::DesktopRect _cursorDrawnToGL;
}

- (const webrtc::DesktopSize&)textureSize;

- (void)setTextureSize:(const webrtc::DesktopSize&)size;

- (const webrtc::MouseCursor&)cursor;

- (void)setCursor:(webrtc::MouseCursor*)cursor;

// bind this object to an effect's via the effects properties
- (void)bindToEffect:(GLKEffectPropertyTexture*)effectProperty;

// True if the cursor has changed in a way that requires it to be redrawn
- (BOOL)needDrawAtPosition:(const webrtc::DesktopVector&)position;

// needDrawAtPosition must be checked prior to calling drawWithMousePosition.
// Draw mouse at the new position.
- (void)drawWithMousePosition:(const webrtc::DesktopVector&)position;

- (void)releaseTexture;

@end

#endif  // REMOTING_IOS_UI_CURSOR_TEXTURE_H_