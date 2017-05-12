// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_DISPLAY_GL_DISPLAY_HANDLER_H_
#define REMOTING_IOS_DISPLAY_GL_DISPLAY_HANDLER_H_

#import <Foundation/Foundation.h>
#import <GLKit/GLKit.h>

#import "remoting/client/display/sys_opengl.h"

#include "base/memory/ptr_util.h"
#include "remoting/client/view_matrix.h"

namespace remoting {

class ChromotingClientRuntime;

namespace protocol {

class VideoRenderer;
class CursorShapeStub;

}  // namespace protocol
}  // namespace remoting

// This protocol is for receiving notifications from the renderer when its state
// changes. Implementations can use this to reposition viewport, process
// animations, etc.
@protocol GlDisplayHandlerDelegate<NSObject>

// Notifies the delegate that the size of the desktop image has changed.
- (void)canvasSizeChanged:(CGSize)size;

- (void)rendererTicked;

@end

@interface GlDisplayHandler : NSObject {
}

- (void)stop;

// Called once the GLKView created.
- (void)onSurfaceCreated:(GLKView*)view;

// Called every time the GLKView dimension is initialized or changed.
- (void)onSurfaceChanged:(const CGRect&)frame;

- (void)onPixelTransformationChanged:(const remoting::ViewMatrix&)matrix;

- (std::unique_ptr<remoting::protocol::VideoRenderer>)CreateVideoRenderer;
- (std::unique_ptr<remoting::protocol::CursorShapeStub>)CreateCursorShapeStub;

- (EAGLContext*)GetEAGLContext;

// This is write-only but @property doesn't support write-only modifier.
@property id<GlDisplayHandlerDelegate> delegate;
- (id<GlDisplayHandlerDelegate>)delegate UNAVAILABLE_ATTRIBUTE;

@end

#endif  // REMOTING_IOS_DISPLAY_GL_DISPLAY_HANDLER_H_
