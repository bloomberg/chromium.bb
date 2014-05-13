// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/ui/cursor_texture.h"

@implementation CursorTexture

- (id)init {
  self = [super init];
  if (self) {
    _needCursorRedraw = NO;
    _cursorDrawnToGL = webrtc::DesktopRect::MakeXYWH(0, 0, 0, 0);
  }
  return self;
}

- (const webrtc::DesktopSize&)textureSize {
  return _textureSize;
}

- (void)setTextureSize:(const webrtc::DesktopSize&)size {
  if (!_textureSize.equals(size)) {
    _textureSize.set(size.width(), size.height());
    _needInitialize = true;
  }
}

- (const webrtc::MouseCursor&)cursor {
  return *_cursor.get();
}

- (void)setCursor:(webrtc::MouseCursor*)cursor {
  _cursor.reset(cursor);

  if (_cursor.get() != NULL && _cursor->image().data()) {
    _needCursorRedraw = true;
  }
}

- (void)bindToEffect:(GLKEffectPropertyTexture*)effectProperty {
  glGenTextures(1, &_textureId);
  [Utility bindTextureForIOS:_textureId];

  // This is the Cursor layer, and is stamped on top of Desktop as a
  // transparent image
  effectProperty.target = GLKTextureTarget2D;
  effectProperty.name = _textureId;
  effectProperty.envMode = GLKTextureEnvModeDecal;
  effectProperty.enabled = GL_TRUE;

  [Utility logGLErrorCode:@"CursorTexture bindToTexture"];
  // Release context
  glBindTexture(GL_TEXTURE_2D, 0);
}

- (BOOL)needDrawAtPosition:(const webrtc::DesktopVector&)position {
  return (_cursor.get() != NULL &&
          (_needInitialize || _needCursorRedraw == YES ||
           _cursorDrawnToGL.left() != position.x() - _cursor->hotspot().x() ||
           _cursorDrawnToGL.top() != position.y() - _cursor->hotspot().y()));
}

- (void)drawWithMousePosition:(const webrtc::DesktopVector&)position {
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

    [Utility logGLErrorCode:@"CursorTexture initializeTextureSurfaceWithSize"];
    _needInitialize = false;
  }
  // When the cursor needs to be redraw in a different spot then we must clear
  // the previous area.

  DCHECK([self needDrawAtPosition:position]);

  if (_cursorDrawnToGL.width() > 0 && _cursorDrawnToGL.height() > 0) {
    webrtc::BasicDesktopFrame transparentCursor(_cursorDrawnToGL.size());

    if (transparentCursor.data() != NULL) {
      DCHECK(transparentCursor.kBytesPerPixel ==
             _cursor->image().kBytesPerPixel);
      memset(transparentCursor.data(),
             0,
             transparentCursor.stride() * transparentCursor.size().height());

      [Utility drawSubRectToGLFromRectOfSize:_textureSize
                                     subRect:_cursorDrawnToGL
                                        data:transparentCursor.data()];

      // there is no longer any cursor drawn to screen
      _cursorDrawnToGL = webrtc::DesktopRect::MakeXYWH(0, 0, 0, 0);
    }
  }

  if (_cursor.get() != NULL) {

    CGRect screen =
        CGRectMake(0.0, 0.0, _textureSize.width(), _textureSize.height());
    CGRect cursor = CGRectMake(position.x() - _cursor->hotspot().x(),
                               position.y() - _cursor->hotspot().y(),
                               _cursor->image().size().width(),
                               _cursor->image().size().height());

    if (CGRectContainsRect(screen, cursor)) {
      _cursorDrawnToGL = webrtc::DesktopRect::MakeXYWH(cursor.origin.x,
                                                       cursor.origin.y,
                                                       cursor.size.width,
                                                       cursor.size.height);

      [Utility drawSubRectToGLFromRectOfSize:_textureSize
                                     subRect:_cursorDrawnToGL
                                        data:_cursor->image().data()];

    } else if (CGRectIntersectsRect(screen, cursor)) {
      // Some of the cursor falls off screen, need to clip it
      CGRect intersection = CGRectIntersection(screen, cursor);
      _cursorDrawnToGL =
          webrtc::DesktopRect::MakeXYWH(intersection.origin.x,
                                        intersection.origin.y,
                                        intersection.size.width,
                                        intersection.size.height);

      webrtc::BasicDesktopFrame partialCursor(_cursorDrawnToGL.size());

      if (partialCursor.data()) {
        DCHECK(partialCursor.kBytesPerPixel == _cursor->image().kBytesPerPixel);

        uint32_t src_stride = _cursor->image().stride();
        uint32_t dst_stride = partialCursor.stride();

        uint8_t* source = _cursor->image().data();
        source += abs((static_cast<int32_t>(cursor.origin.y) -
                       _cursorDrawnToGL.top())) *
                  src_stride;
        source += abs((static_cast<int32_t>(cursor.origin.x) -
                       _cursorDrawnToGL.left())) *
                  _cursor->image().kBytesPerPixel;
        uint8_t* dst = partialCursor.data();

        for (uint32_t y = 0; y < _cursorDrawnToGL.height(); y++) {
          memcpy(dst, source, dst_stride);
          source += src_stride;
          dst += dst_stride;
        }

        [Utility drawSubRectToGLFromRectOfSize:_textureSize
                                       subRect:_cursorDrawnToGL
                                          data:partialCursor.data()];
      }
    }
  }

  _needCursorRedraw = false;
  [Utility logGLErrorCode:@"CursorTexture drawWithMousePosition"];
  // Release context
  glBindTexture(GL_TEXTURE_2D, 0);
}

- (void)releaseTexture {
  glDeleteTextures(1, &_textureId);
}

@end
