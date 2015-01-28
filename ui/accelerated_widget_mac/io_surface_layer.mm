// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/io_surface_layer.h"

#include <CoreFoundation/CoreFoundation.h>
#include <OpenGL/CGLIOSurface.h>
#include <OpenGL/CGLRenderers.h>
#include <OpenGL/gl.h>
#include <OpenGL/OpenGL.h>

#include "base/mac/sdk_forward_declarations.h"
#include "base/trace_event/trace_event.h"
#include "ui/accelerated_widget_mac/io_surface_context.h"
#include "ui/accelerated_widget_mac/io_surface_texture.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gl/gpu_switching_manager.h"

////////////////////////////////////////////////////////////////////////////////
// IOSurfaceLayerHelper

namespace ui {

IOSurfaceLayerHelper::IOSurfaceLayerHelper(
    IOSurfaceLayerClient* client,
    IOSurfaceLayer* layer)
        : client_(client),
          layer_(layer),
          needs_display_(false),
          has_pending_frame_(false),
          did_not_draw_counter_(0),
          is_pumping_frames_(false),
          timer_(
              FROM_HERE,
              base::TimeDelta::FromSeconds(1) / 6,
              this,
              &IOSurfaceLayerHelper::TimerFired) {}

IOSurfaceLayerHelper::~IOSurfaceLayerHelper() {
  // Any acks that were waiting on this layer to draw will not occur, so ack
  // them now to prevent blocking the renderer.
  AckPendingFrame(true);
}

void IOSurfaceLayerHelper::GotNewFrame() {
  // A trace value of 2 indicates that there is a pending swap ack. See
  // canDrawInCGLContext for other value meanings.
  TRACE_COUNTER_ID1("browser", "PendingSwapAck", this, 2);

  has_pending_frame_ = true;
  needs_display_ = true;
  timer_.Reset();

  // If reqested, draw immediately and don't bother trying to use the
  // isAsynchronous property to ensure smooth animation. If this is while
  // frames are being pumped then ack and display immediately to get a
  // correct-sized frame displayed as soon as possible.
  if (is_pumping_frames_ || client_->IOSurfaceLayerShouldAckImmediately()) {
    SetNeedsDisplayAndDisplayAndAck();
  } else {
    if (![layer_ isAsynchronous])
      [layer_ setAsynchronous:YES];
  }
}

void IOSurfaceLayerHelper::SetNeedsDisplay() {
  needs_display_ = true;
}

bool IOSurfaceLayerHelper::CanDraw() {
  // If we return NO 30 times in a row, switch to being synchronous to avoid
  // burning CPU cycles on this callback.
  if (needs_display_) {
    did_not_draw_counter_ = 0;
  } else {
    did_not_draw_counter_ += 1;
    if (did_not_draw_counter_ == 30)
      [layer_ setAsynchronous:NO];
  }

  // Add an instantaneous blip to the PendingSwapAck state to indicate
  // that CoreAnimation asked if a frame is ready. A blip up to to 3 (usually
  // from 2, indicating that a swap ack is pending) indicates that we
  // requested a draw. A blip up to 1 (usually from 0, indicating there is no
  // pending swap ack) indicates that we did not request a draw. This would
  // be more natural to do with a tracing pseudo-thread
  // http://crbug.com/366300
  TRACE_COUNTER_ID1("browser", "PendingSwapAck", this, needs_display_ ? 3 : 1);
  TRACE_COUNTER_ID1("browser", "PendingSwapAck", this,
                    has_pending_frame_ ? 2 : 0);

  return needs_display_;
}

void IOSurfaceLayerHelper::DidDraw(bool success) {
  needs_display_ = false;
  AckPendingFrame(success);
}

void IOSurfaceLayerHelper::AckPendingFrame(bool success) {
  if (!has_pending_frame_)
    return;
  has_pending_frame_ = false;
  if (success)
    client_->IOSurfaceLayerDidDrawFrame();
  else
    client_->IOSurfaceLayerHitError();
  // A trace value of 0 indicates that there is no longer a pending swap ack.
  TRACE_COUNTER_ID1("browser", "PendingSwapAck", this, 0);
}

void IOSurfaceLayerHelper::SetNeedsDisplayAndDisplayAndAck() {
  // Drawing using setNeedsDisplay and displayIfNeeded will result in
  // subsequent canDrawInCGLContext callbacks getting dropped, and jerky
  // animation. Disable asynchronous drawing before issuing these calls as a
  // workaround.
  // http://crbug.com/395827
  if ([layer_ isAsynchronous])
    [layer_ setAsynchronous:NO];

  [layer_ setNeedsDisplay];
  DisplayIfNeededAndAck();
}

void IOSurfaceLayerHelper::DisplayIfNeededAndAck() {
  if (!needs_display_)
    return;

  // As in SetNeedsDisplayAndDisplayAndAck, disable asynchronous drawing before
  // issuing displayIfNeeded.
  // http://crbug.com/395827
  if ([layer_ isAsynchronous])
    [layer_ setAsynchronous:NO];

  // Do not bother drawing while pumping new frames -- wait until the waiting
  // block ends to draw any of the new frames.
  if (!is_pumping_frames_)
    [layer_ displayIfNeeded];

  // Calls to setNeedsDisplay can sometimes be ignored, especially if issued
  // rapidly (e.g, with vsync off). This is unacceptable because the failure
  // to ack a single frame will hang the renderer. Ensure that the renderer
  // not be blocked by lying and claiming that we drew the frame.
  AckPendingFrame(true);
}

void IOSurfaceLayerHelper::TimerFired() {
  DisplayIfNeededAndAck();
}

void IOSurfaceLayerHelper::BeginPumpingFrames() {
  is_pumping_frames_ = true;
}

void IOSurfaceLayerHelper::EndPumpingFrames() {
  is_pumping_frames_ = false;
  DisplayIfNeededAndAck();
}

}  // namespace ui

////////////////////////////////////////////////////////////////////////////////
// IOSurfaceLayer

@implementation IOSurfaceLayer

- (id)initWithClient:(ui::IOSurfaceLayerClient*)client
            withScaleFactor:(float)scale_factor
    needsGLFinishWorkaround:(bool)needs_gl_finish_workaround {
  if (self = [super init]) {
    helper_.reset(new ui::IOSurfaceLayerHelper(client, self));

    iosurface_ = ui::IOSurfaceTexture::Create(needs_gl_finish_workaround);
    context_ = ui::IOSurfaceContext::Get(
        ui::IOSurfaceContext::kCALayerContext);
    if (!iosurface_.get() || !context_.get()) {
      LOG(ERROR) << "Failed create CompositingIOSurface or context";
      [self resetClient];
      [self release];
      return nil;
    }

    [self setBackgroundColor:CGColorGetConstantColor(kCGColorWhite)];
    [self setAnchorPoint:CGPointMake(0, 0)];
    // Setting contents gravity is necessary to prevent the layer from being
    // scaled during dyanmic resizes (especially with devtools open).
    [self setContentsGravity:kCAGravityTopLeft];
    if ([self respondsToSelector:(@selector(setContentsScale:))]) {
      [self setContentsScale:scale_factor];
    }
  }
  return self;
}

- (void)dealloc {
  DCHECK(!helper_);
  [super dealloc];
}

- (bool)gotFrameWithIOSurface:(IOSurfaceID)io_surface_id
                withPixelSize:(gfx::Size)pixel_size
              withScaleFactor:(float)scale_factor {
  return iosurface_->SetIOSurface(io_surface_id, pixel_size);
}

- (void)poisonContextAndSharegroup {
  context_->PoisonContextAndSharegroup();
}

- (bool)hasBeenPoisoned {
  return context_->HasBeenPoisoned();
}

- (float)scaleFactor {
  if ([self respondsToSelector:(@selector(contentsScale))])
    return [self contentsScale];
  return 1;
}

- (int)rendererID {
  GLint current_renderer_id = -1;
  if (CGLGetParameter(context_->cgl_context(),
                      kCGLCPCurrentRendererID,
                      &current_renderer_id) == kCGLNoError) {
    return current_renderer_id & kCGLRendererIDMatchingMask;
  }
  return -1;
}

- (void)resetClient {
  helper_.reset();
}

- (void)gotNewFrame {
  helper_->GotNewFrame();
}

- (void)setNeedsDisplayAndDisplayAndAck {
  helper_->SetNeedsDisplayAndDisplayAndAck();
}

- (void)displayIfNeededAndAck {
  helper_->DisplayIfNeededAndAck();
}

- (void)beginPumpingFrames {
  helper_->BeginPumpingFrames();
}

- (void)endPumpingFrames {
  helper_->EndPumpingFrames();
}

// The remaining methods implement the CAOpenGLLayer interface.

- (CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask {
  if (!context_.get())
    return [super copyCGLPixelFormatForDisplayMask:mask];
  return CGLRetainPixelFormat(CGLGetPixelFormat(context_->cgl_context()));
}

- (CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pixelFormat {
  if (!context_.get())
    return [super copyCGLContextForPixelFormat:pixelFormat];
  return CGLRetainContext(context_->cgl_context());
}

- (void)setNeedsDisplay {
  if (helper_)
    helper_->SetNeedsDisplay();
  [super setNeedsDisplay];
}

- (BOOL)canDrawInCGLContext:(CGLContextObj)glContext
                pixelFormat:(CGLPixelFormatObj)pixelFormat
               forLayerTime:(CFTimeInterval)timeInterval
                displayTime:(const CVTimeStamp*)timeStamp {
  if (helper_)
    return helper_->CanDraw();
  return NO;
}

- (void)drawInCGLContext:(CGLContextObj)glContext
             pixelFormat:(CGLPixelFormatObj)pixelFormat
            forLayerTime:(CFTimeInterval)timeInterval
             displayTime:(const CVTimeStamp*)timeStamp {
  TRACE_EVENT0("browser", "IOSurfaceLayer::drawInCGLContext");

  bool draw_succeeded = iosurface_->DrawIOSurface();
  if (helper_)
    helper_->DidDraw(draw_succeeded);

  [super drawInCGLContext:glContext
              pixelFormat:pixelFormat
             forLayerTime:timeInterval
              displayTime:timeStamp];
}

@end
