// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include <array>

#import "remoting/client/ios/display/gl_display_handler.h"

#import "base/mac/bind_objc_block.h"
#import "remoting/client/display/sys_opengl.h"
#import "remoting/client/ios/display/gl_demo_screen.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "remoting/client/chromoting_client_runtime.h"
#include "remoting/client/cursor_shape_stub_proxy.h"
#include "remoting/client/display/gl_canvas.h"
#include "remoting/client/display/gl_renderer.h"
#include "remoting/client/display/gl_renderer_delegate.h"
#include "remoting/client/dual_buffer_frame_consumer.h"
#include "remoting/client/software_video_renderer.h"

namespace remoting {
namespace GlDisplayHandler {

// The core that lives on the display thread.
class Core : public protocol::CursorShapeStub, public GlRendererDelegate {
 public:
  Core();
  ~Core() override;

  void Initialize();

  // CursorShapeStub interface.
  void SetCursorShape(const protocol::CursorShapeInfo& cursor_shape) override;

  // GlRendererDelegate interface.
  bool CanRenderFrame() override;
  void OnFrameRendered() override;
  void OnSizeChanged(int width, int height) override;

  void OnFrameReceived(std::unique_ptr<webrtc::DesktopFrame> frame,
                       const base::Closure& done);
  void Stop();
  void SurfaceChanged(int width, int height);
  std::unique_ptr<protocol::FrameConsumer> GrabFrameConsumer();
  EAGLContext* GetEAGLContext();
  base::WeakPtr<Core> GetWeakPtr();

 private:
  remoting::ChromotingClientRuntime* runtime_;

  // Will be std::move'd when GrabFrameConsumer() is called.
  std::unique_ptr<DualBufferFrameConsumer> owned_frame_consumer_;
  base::WeakPtr<DualBufferFrameConsumer> frame_consumer_;

  EAGLContext* eagl_context_;
  std::unique_ptr<GlRenderer> renderer_;
  //  GlDemoScreen *demo_screen_;

  // Used on display thread.
  base::WeakPtr<Core> weak_ptr_;
  base::WeakPtrFactory<Core> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

Core::Core() : weak_factory_(this) {
  runtime_ = ChromotingClientRuntime::GetInstance();
  DCHECK(!runtime_->display_task_runner()->BelongsToCurrentThread());

  weak_ptr_ = weak_factory_.GetWeakPtr();

  runtime_->display_task_runner()->PostTask(
      FROM_HERE, base::Bind(&Core::Initialize, base::Unretained(this)));

  // Do not bind GlRenderer::OnFrameReceived. |renderer_| is not ready yet.
  owned_frame_consumer_.reset(new remoting::DualBufferFrameConsumer(
      base::Bind(&Core::OnFrameReceived, weak_ptr_),
      runtime_->display_task_runner(),
      protocol::FrameConsumer::PixelFormat::FORMAT_RGBA));
  frame_consumer_ = owned_frame_consumer_->GetWeakPtr();
}

Core::~Core() {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
}

void Core::Initialize() {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());

  eagl_context_ = [EAGLContext currentContext];
  if (!eagl_context_) {
    // TODO(nicholss): For prod code, make sure to check for ES3 support and
    // fall back to ES2 if needed.
    eagl_context_ =
        [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
    [EAGLContext setCurrentContext:eagl_context_];
  }

  renderer_ = remoting::GlRenderer::CreateGlRendererWithDesktop();

  //  renderer_.RequestCanvasSize();

  renderer_->OnSurfaceCreated(
      base::MakeUnique<GlCanvas>(static_cast<int>([eagl_context_ API])));

  SurfaceChanged(1024, 640);  // TODO(nicholss): Where does this data comefrom?

  // TODO(nicholss): This are wrong values but it lets us get something on the
  // screen.
  std::array<float, 9> matrix = {{1, 0, 0,
                                  0, 1, 0,
                                  0, 0, 1}};

  renderer_->OnPixelTransformationChanged(matrix);

  // demo_screen_ = new GlDemoScreen();
  // renderer_->AddDrawable(demo_screen_->GetWeakPtr());
  renderer_->SetDelegate(weak_ptr_);
}

void Core::SetCursorShape(const protocol::CursorShapeInfo& cursor_shape) {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  renderer_->OnCursorShapeChanged(cursor_shape);
}

bool Core::CanRenderFrame() {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  return eagl_context_ != NULL;
}

std::unique_ptr<protocol::FrameConsumer> Core::GrabFrameConsumer() {
  DCHECK(owned_frame_consumer_) << "The frame consumer is already grabbed.";
  return std::move(owned_frame_consumer_);
}

void Core::OnFrameReceived(std::unique_ptr<webrtc::DesktopFrame> frame,
                           const base::Closure& done) {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  renderer_->OnFrameReceived(std::move(frame), done);
}

void Core::OnFrameRendered() {
  // Nothing to do.
}

void Core::OnSizeChanged(int width, int height) {
  // Nothing to do.
}

void Core::Stop() {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());

  eagl_context_ = nil;
  // demo_screen_ = nil;
}

void Core::SurfaceChanged(int width, int height) {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  renderer_->OnSurfaceChanged(width, height);
}

EAGLContext* Core::GetEAGLContext() {
  return eagl_context_;
}

base::WeakPtr<remoting::GlDisplayHandler::Core> Core::GetWeakPtr() {
  return weak_ptr_;
}

}  // namespace GlDisplayHandler
}  // namespace remoting

@interface GlDisplayHandler () {
  remoting::GlDisplayHandler::Core* _core;
  remoting::ChromotingClientRuntime* _runtime;
}
@end

@implementation GlDisplayHandler

- (id)init {
  self = [super init];
  if (self) {
    _runtime = remoting::ChromotingClientRuntime::GetInstance();
    _core = new remoting::GlDisplayHandler::Core();
  }
  return self;
}

#pragma mark - Public

- (void)stop {
  _runtime->display_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&remoting::GlDisplayHandler::Core::Stop, _core->GetWeakPtr()));
}

- (std::unique_ptr<remoting::protocol::VideoRenderer>)CreateVideoRenderer {
  return base::MakeUnique<remoting::SoftwareVideoRenderer>(
      _core->GrabFrameConsumer());
}

- (std::unique_ptr<remoting::protocol::CursorShapeStub>)CreateCursorShapeStub {
  return base::MakeUnique<remoting::CursorShapeStubProxy>(
      _core->GetWeakPtr(), _runtime->display_task_runner());
}

- (EAGLContext*)GetEAGLContext {
  return _core->GetEAGLContext();
}

// TODO(nicholss): Remove this function, it is not used in the final impl,
// or it should call RequestRender.
- (void)glkView:(GLKView*)view drawInRect:(CGRect)rect {
  if (_core) {
    _runtime->display_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&remoting::GlDisplayHandler::Core::SurfaceChanged,
                   _core->GetWeakPtr(), rect.size.width, rect.size.height));
  }
}

@end
