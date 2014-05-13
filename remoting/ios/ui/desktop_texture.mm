// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/ui/desktop_texture.h"

@implementation DesktopTexture

- (const webrtc::DesktopSize&)textureSize {
  return _textureSize;
}

- (void)setTextureSize:(const webrtc::DesktopSize&)size {
  if (!_textureSize.equals(size)) {
    _textureSize.set(size.width(), size.height());
    _needInitialize = true;
  }
}

- (void)bindToEffect:(GLKEffectPropertyTexture*)effectProperty {
  glGenTextures(1, &_textureId);
  [Utility bindTextureForIOS:_textureId];

  // This is the HOST Desktop layer, and each draw will always replace what is
  // currently in the draw context
  effectProperty.target = GLKTextureTarget2D;
  effectProperty.name = _textureId;
  effectProperty.envMode = GLKTextureEnvModeReplace;
  effectProperty.enabled = GL_TRUE;

  [Utility logGLErrorCode:@"DesktopTexture bindToTexture"];
  // Release context
  glBindTexture(GL_TEXTURE_2D, 0);
}

- (BOOL)needDraw {
  return _needInitialize;
}

- (void)drawRegion:(GLRegion*)region rect:(CGRect)rect {
  if (_textureSize.height() == 0 && _textureSize.width() == 0) {
    return;
  }

  [Utility bindTextureForIOS:_textureId];

  if (_needInitialize) {
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 _textureSize.width(),
                 _textureSize.height(),
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 NULL);

    [Utility logGLErrorCode:@"DesktopTexture initializeTextureSurfaceWithSize"];
    _needInitialize = false;
  }

  [Utility drawSubRectToGLFromRectOfSize:_textureSize
                                 subRect:webrtc::DesktopRect::MakeXYWH(
                                             region->offset->x(),
                                             region->offset->y(),
                                             region->image->size().width(),
                                             region->image->size().height())
                                    data:region->image->data()];

  [Utility logGLErrorCode:@"DesktopTexture drawRegion"];
  // Release context
  glBindTexture(GL_TEXTURE_2D, 0);
}

- (void)releaseTexture {
  glDeleteTextures(1, &_textureId);
}

@end
