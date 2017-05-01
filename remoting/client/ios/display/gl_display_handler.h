// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_DISPLAY_GL_DISPLAY_HANDLER_H_
#define REMOTING_CLIENT_IOS_DISPLAY_GL_DISPLAY_HANDLER_H_

#import <Foundation/Foundation.h>
#import <GLKit/GLKit.h>

#import "remoting/client/display/sys_opengl.h"

#include "base/memory/ptr_util.h"

namespace remoting {

class ChromotingClientRuntime;

namespace protocol {

class VideoRenderer;
class CursorShapeStub;

}  // namespace protocol
}  // namespace remoting

@interface GlDisplayHandler : NSObject {
}

- (void)stop;

// Called once the GLKView created.
- (void)onSurfaceCreated:(GLKView*)view;

// Called every time the GLKView dimension is initialized or changed.
- (void)onSurfaceChanged:(const CGRect&)frame;

- (std::unique_ptr<remoting::protocol::VideoRenderer>)CreateVideoRenderer;
- (std::unique_ptr<remoting::protocol::CursorShapeStub>)CreateCursorShapeStub;

- (EAGLContext*)GetEAGLContext;

@end

#endif  // REMOTING_CLIENT_IOS_DISPLAY_GL_DISPLAY_HANDLER_H_
