// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif


#import <Foundation/Foundation.h>
#import <GLKit/GLKit.h>

#import "remoting/client/display/sys_opengl.h"
#import "remoting/client/ios/display/gl_demo_screen.h"
#import "remoting/client/ios/display/gl_display_handler.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "remoting/client/chromoting_client.h"
#include "remoting/client/display/gl_canvas.h"
#include "remoting/client/display/gl_renderer.h"
#include "remoting/client/display/gl_renderer_delegate.h"
#include "remoting/client/dual_buffer_frame_consumer.h"
#include "remoting/client/ios/app_runtime.h"
#include "remoting/client/software_video_renderer.h"

namespace remoting {
namespace GlDisplayHandler {

// The core that lives on the display thread.
class Core :  public GlRendererDelegate {
 public:
  Core(remoting::ios::AppRuntime* runtime);
  ~Core() override;

  // GlRendererDelegate interface.
  bool CanRenderFrame() override;
  void OnFrameRendered() override;
  void OnSizeChanged(int width, int height) override;

  void Created();
  void SurfaceChanged(int width, int height);
  std::unique_ptr<protocol::FrameConsumer> GrabFrameConsumer();
  base::WeakPtr<Core> GetWeakPtr();

 private:
  // Will be std::move'd when GrabFrameConsumer() is called.
  remoting::ios::AppRuntime* runtime_;
  std::unique_ptr<DualBufferFrameConsumer> owned_frame_consumer_;

  base::WeakPtr<DualBufferFrameConsumer> frame_consumer_;
  EAGLContext* eagl_context_;
  GlRenderer renderer_;
  GlDemoScreen demo_screen_;

  // Used on display thread.
  base::WeakPtr<Core> weak_ptr_;
  base::WeakPtrFactory<Core> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

Core::Core(remoting::ios::AppRuntime* runtime)
    : runtime_(runtime), weak_factory_(this) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
  renderer_.SetDelegate(weak_ptr_);
  owned_frame_consumer_.reset(new remoting::DualBufferFrameConsumer(
      base::Bind(&remoting::GlRenderer::OnFrameReceived,
                 renderer_.GetWeakPtr()),
      runtime_->display_task_runner(),
      remoting::protocol::FrameConsumer::PixelFormat::FORMAT_RGBA));
  frame_consumer_ = owned_frame_consumer_->GetWeakPtr();
  renderer_.AddDrawable(demo_screen_.GetWeakPtr());
}

Core::~Core() {}

bool Core::CanRenderFrame() {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  return eagl_context_ != NULL;
}

std::unique_ptr<protocol::FrameConsumer> Core::GrabFrameConsumer() {
  DCHECK(owned_frame_consumer_) << "The frame consumer is already grabbed.";
  return std::move(owned_frame_consumer_);
}

void Core::OnFrameRendered() {
  // Nothing to do.
}

void Core::OnSizeChanged(int width, int height) {
  // Nothing to do.
}

void Core::Created() {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  DCHECK(!eagl_context_);

  eagl_context_ = [EAGLContext currentContext];
  renderer_.RequestCanvasSize();

  renderer_.OnSurfaceCreated(base::MakeUnique<GlCanvas>(
      static_cast<int>([eagl_context_ API])));
}

void Core::SurfaceChanged(int width, int height) {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  renderer_.OnSurfaceChanged(width, height);
}

base::WeakPtr<remoting::GlDisplayHandler::Core> Core::GetWeakPtr() {
  return weak_ptr_;
}

}  // namespace GlDisplayHandler
}  // namespace remoting

@interface GlDisplayHandler ()
@property(nonatomic) remoting::GlDisplayHandler::Core* core_;
@property(nonatomic) remoting::ios::AppRuntime* runtime_;
@end

@implementation GlDisplayHandler

@synthesize core_ = _core_;
@synthesize runtime_ = _runtime_;

- (id)initWithRuntime:(remoting::ios::AppRuntime*)runtime {
  self.runtime_ = runtime;
  return self;
}

- (void)created {
  _core_ = new remoting::GlDisplayHandler::Core(self.runtime_);

  self.runtime_->display_task_runner()->PostTask(
      FROM_HERE, base::Bind(&remoting::GlDisplayHandler::Core::Created,
                            self.core_->GetWeakPtr()));
}

- (std::unique_ptr<remoting::protocol::VideoRenderer>)CreateVideoRenderer {
  return base::MakeUnique<remoting::SoftwareVideoRenderer>(
      _core_->GrabFrameConsumer());
}

// In general, avoid expensive work in this function to maximize frame rate.
- (void)glkView:(GLKView*)view drawInRect:(CGRect)rect {
  if (_core_) {
    _core_->SurfaceChanged(rect.size.width, rect.size.height);
  }
}

@end
