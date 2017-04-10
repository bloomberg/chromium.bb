// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_DISPLAY_GL_DISPLAY_HANDLER_H_
#define REMOTING_CLIENT_IOS_DISPLAY_GL_DISPLAY_HANDLER_H_

#import <Foundation/Foundation.h>
#import <GLKit/GLKit.h>

#import "remoting/client/display/sys_opengl.h"

#include "remoting/client/client_context.h"
#include "remoting/protocol/cursor_shape_stub.h"
#include "remoting/protocol/frame_consumer.h"
#include "remoting/protocol/video_renderer.h"

namespace remoting {

class ChromotingClientRuntime;

}  // namespace remoting

@interface GlDisplayHandler : NSObject {
}

- (id)initWithRuntime:(remoting::ChromotingClientRuntime*)runtime;

- (void)created;
- (void)stop;
- (void)glkView:(GLKView*)view drawInRect:(CGRect)rect;
- (std::unique_ptr<remoting::protocol::VideoRenderer>)CreateVideoRenderer;
- (std::unique_ptr<remoting::protocol::CursorShapeStub>)CreateCursorShapeStub;

@end

#endif  // REMOTING_CLIENT_IOS_DISPLAY_GL_DISPLAY_HANDLER_H_
