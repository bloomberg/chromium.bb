/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#import "plugin_mac.h"
#import <OpenGL/OpenGL.h>
#import "o3d_layer.h"

#include "display_window_mac.h"
using o3d::DisplayWindowMac;

@implementation O3DLayer


- (id)init {
  self = [super init];

  // Set ourselves up to composite correctly. Users can write
  // arbitrary alpha values into the back buffer but O3D's output is
  // defined as being opaque. For correct rendering results we must
  // basically drop the alpha channel while drawing to the screen
  // using Core Animation. Setting the opaque flag achieves this.
  if (self != nil) {
    self.opaque = YES;
  }

  return self;
}

- (void)setPluginObject:(PluginObject *)obj {
  obj_ = obj;
}


/* Called when a new frame needs to be generated for layer time 't'.
 * 'ctx' is attached to the rendering destination. It's state is
 * otherwise undefined. When non-null 'ts'  describes the display
 * timestamp associated with layer time 't'. The default implementation
 * of the method flushes the context. */


- (void)drawInCGLContext:(CGLContextObj)ctx
             pixelFormat:(CGLPixelFormatObj)pf
            forLayerTime:(CFTimeInterval)t
             displayTime:(const CVTimeStamp *)ts {
  // Watch out for the plugin being destroyed out from under us.
  if (!obj_) {
    return;
  }

  // Set the current context to the one given to us.
  CGLSetCurrentContext(ctx);

  if (created_context_) {
    DCHECK_EQ(ctx, obj_->mac_cgl_context_);
    o3d::DisplayWindowMac default_display;
    default_display.set_agl_context(obj_->mac_agl_context_);
    default_display.set_cgl_context(obj_->mac_cgl_context_);
    obj_->CreateRenderer(default_display);
    obj_->client()->Init();
    created_context_ = false;
  }

  if (was_resized_) {
    obj_->Resize(width_, height_);
    was_resized_ = false;
  }


  if (obj_) {
    obj_->client()->Tick();
    obj_->client()->RenderClient(true);
  }

  // Call super to finalize the drawing. By default it just calls glFlush().
  [super drawInCGLContext:ctx pixelFormat:pf forLayerTime:t displayTime:ts];
}

- (BOOL)canDrawInCGLContext:(CGLContextObj)ctx
                pixelFormat:(CGLPixelFormatObj)pf
               forLayerTime:(CFTimeInterval)t
                displayTime:(const CVTimeStamp *)ts {
  return YES;
}




/* Called by the CAOpenGLLayer implementation when a rendering context
 * is needed by the layer. Should return an OpenGL context with
 * renderers from pixel format 'pixelFormat'. The default implementation
 * allocates a new context with a null share context. */
- (CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pixelFormat {
  if (glContext_ == NULL) {
    CGLContextObj share_context = obj_->GetFullscreenShareContext();
    DCHECK(share_context);
    if (CGLCreateContext(pixelFormat, share_context, &glContext_) !=
        kCGLNoError) {
      glContext_ = [super copyCGLContextForPixelFormat:pixelFormat];
    }
    obj_->SetMacCGLContext(glContext_);
    created_context_ = true;
  }
  return glContext_;
}

/*
- (CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask {
  return [super copyCGLPixelFormatForDisplayMask:mask];
}*/


#define PFA(number) static_cast<CGLPixelFormatAttribute>(number)

#define O3D_COLOR_AND_DEPTH_SETTINGS kCGLPFAClosestPolicy, \
                                     kCGLPFAColorSize, PFA(24), \
                                     kCGLPFAAlphaSize, PFA(8),  \
                                     kCGLPFADepthSize, PFA(24), \
                                     kCGLPFADoubleBuffer,
#define O3D_STENCIL_SETTINGS kCGLPFAStencilSize, PFA(8),
#define O3D_HARDWARE_RENDERER kCGLPFAAccelerated, kCGLPFANoRecovery,
#define O3D_MULTISAMPLE kCGLPFAMultisample, kCGLPFASamples, PFA(4),
#define O3D_DISPLAY_MASK(mask) kCGLPFADisplayMask, PFA(mask),
#define O3D_END PFA(0)
- (CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask {
  const CGLPixelFormatAttribute attributes[] = {
    O3D_COLOR_AND_DEPTH_SETTINGS
    O3D_STENCIL_SETTINGS
    O3D_HARDWARE_RENDERER
    O3D_DISPLAY_MASK(mask)
    O3D_MULTISAMPLE
    O3D_END
  };
  CGLPixelFormatObj pixel_format = NULL;
  GLint num_screens = 0;
  if (!CGLChoosePixelFormat(attributes, &pixel_format, &num_screens) &&
      pixel_format) {
    return pixel_format;
  } else {
    // Try a less capable set.
    static const CGLPixelFormatAttribute low_end_attributes[] = {
      O3D_COLOR_AND_DEPTH_SETTINGS
      O3D_STENCIL_SETTINGS
      O3D_HARDWARE_RENDERER
      O3D_DISPLAY_MASK(mask)
      O3D_END
    };
    if (!CGLChoosePixelFormat(low_end_attributes,
                              &pixel_format, &num_screens) && pixel_format) {
      return pixel_format;
    } else {
      // Do whatever the superclass supports.
      return [super copyCGLPixelFormatForDisplayMask:mask];
    }
  }
}


- (CGLContextObj)glContext {
  return glContext_;
}

- (void)setWidth:(int)width height:(int)height {
  width_ = width;
  height_ = height;
  was_resized_ = true;
}

@end
