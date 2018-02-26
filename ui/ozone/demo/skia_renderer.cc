// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/demo/skia_renderer.h"

#include "base/command_line.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/gl/GrGLAssembleInterface.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace ui {

namespace {

const GrGLInterface* GrGLCreateNativeInterface() {
  return GrGLAssembleInterface(nullptr, [](void* ctx, const char name[]) {
    return gl::GetGLProcAddress(name);
  });
}

const char kUseDDL[] = "use-ddl";

}  // namespace

SkiaRenderer::SkiaRenderer(gfx::AcceleratedWidget widget,
                           const scoped_refptr<gl::GLSurface>& surface,
                           const gfx::Size& size)
    : RendererBase(widget, size),
      gl_surface_(surface),
      use_ddl_(base::CommandLine::ForCurrentProcess()->HasSwitch(kUseDDL)),
      condition_variable_(&lock_),
      weak_ptr_factory_(this) {}

SkiaRenderer::~SkiaRenderer() {
  if (use_ddl_)
    StopDDLRenderThread();
}

bool SkiaRenderer::Initialize() {
  gl_context_ = gl::init::CreateGLContext(nullptr, gl_surface_.get(),
                                          gl::GLContextAttribs());
  if (!gl_context_.get()) {
    LOG(FATAL) << "Failed to create GL context";
    return false;
  }

  gl_surface_->Resize(size_, 1.f, gl::GLSurface::ColorSpace::UNSPECIFIED, true);

  if (!gl_context_->MakeCurrent(gl_surface_.get())) {
    LOG(FATAL) << "Failed to make GL context current";
    return false;
  }

  auto native_interface =
      sk_sp<const GrGLInterface>(GrGLCreateNativeInterface());
  DCHECK(native_interface);
  GrContextOptions options;
  if (use_ddl_)
    options.fExplicitlyAllocateGPUResources = GrContextOptions::Enable::kYes;
  gr_context_ = GrContext::MakeGL(std::move(native_interface), options);
  DCHECK(gr_context_);

  PostRenderFrameTask(gfx::SwapResult::SWAP_ACK);
  return true;
}

void SkiaRenderer::RenderFrame() {
  TRACE_EVENT0("ozone", "SkiaRenderer::RenderFrame");

  // LegacyFontHost will get LCD text and skia figures out what type to use.
  SkSurfaceProps surface_props =
      SkSurfaceProps(0, SkSurfaceProps::kLegacyFontHost_InitType);

  if (!sk_surface_) {
    GrGLFramebufferInfo framebuffer_info;
    framebuffer_info.fFBOID = 0;
    GrBackendRenderTarget render_target(size_.width(), size_.height(), 0, 8,
                                        kRGBA_8888_GrPixelConfig,
                                        framebuffer_info);

    sk_surface_ = SkSurface::MakeFromBackendRenderTarget(
        gr_context_.get(), render_target, kBottomLeft_GrSurfaceOrigin, nullptr,
        &surface_props);
  }

  if (use_ddl_) {
    StartDDLRenderThreadIfNecessary(sk_surface_.get());
    auto ddl = GetDDL();
    sk_surface_->draw(ddl.get());
  } else {
    Draw(sk_surface_->getCanvas(), NextFraction());
  }
  gr_context_->flush();
  glFinish();

  if (gl_surface_->SupportsAsyncSwap()) {
    gl_surface_->SwapBuffersAsync(
        base::BindRepeating(&SkiaRenderer::PostRenderFrameTask,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindRepeating(&SkiaRenderer::OnPresentation,
                            weak_ptr_factory_.GetWeakPtr()));
  } else {
    PostRenderFrameTask(gl_surface_->SwapBuffers(base::BindRepeating(
        &SkiaRenderer::OnPresentation, weak_ptr_factory_.GetWeakPtr())));
  }
}

void SkiaRenderer::PostRenderFrameTask(gfx::SwapResult result) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindRepeating(&SkiaRenderer::RenderFrame,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void SkiaRenderer::Draw(SkCanvas* canvas, float fraction) {
  TRACE_EVENT0("ozone", "SkiaRenderer::Draw");
  // Clear background
  canvas->clear(SkColorSetARGB(255, 255 * fraction, 255 * (1 - fraction), 0));

  SkPaint paint;
  paint.setColor(SK_ColorRED);

  // Draw a rectangle with red paint
  SkRect rect = SkRect::MakeXYWH(10, 10, 128, 128);
  canvas->drawRect(rect, paint);

  // Set up a linear gradient and draw a circle
  {
    SkPoint linearPoints[] = {{0, 0}, {300, 300}};
    SkColor linearColors[] = {SK_ColorGREEN, SK_ColorBLACK};
    paint.setShader(SkGradientShader::MakeLinear(
        linearPoints, linearColors, nullptr, 2, SkShader::kMirror_TileMode));
    paint.setAntiAlias(true);

    canvas->drawCircle(200, 200, 64, paint);

    // Detach shader
    paint.setShader(nullptr);
  }

  // Draw a message with a nice black paint
  paint.setSubpixelText(true);
  paint.setColor(SK_ColorBLACK);
  paint.setTextSize(32);

  canvas->save();
  static const char message[] = "Hello Ozone";
  static const char message_ddl[] = "Hello Ozone + DDL";

  // Translate and rotate
  canvas->translate(300, 300);
  rotation_angle_ += 0.2f;
  if (rotation_angle_ > 360) {
    rotation_angle_ -= 360;
  }
  canvas->rotate(rotation_angle_);

  const char* text = use_ddl_ ? message_ddl : message;
  // Draw the text
  canvas->drawText(text, strlen(text), 0, 0, paint);

  canvas->restore();
}

void SkiaRenderer::StartDDLRenderThreadIfNecessary(SkSurface* sk_surface) {
  DCHECK(use_ddl_);
  if (ddl_render_thread_)
    return;
  DCHECK(!surface_charaterization_.isValid());
  DCHECK(sk_surface);

  if (!sk_surface->characterize(&surface_charaterization_))
    LOG(FATAL) << "Failed to cheracterize the skia surface!";
  ddl_render_thread_ =
      std::make_unique<base::DelegateSimpleThread>(this, "DDLRenderThread");
  ddl_render_thread_->Start();
}

void SkiaRenderer::StopDDLRenderThread() {
  if (!ddl_render_thread_)
    return;
  {
    base::AutoLock auto_lock_(lock_);
    surface_charaterization_ = SkSurfaceCharacterization();
    condition_variable_.Signal();
  }
  ddl_render_thread_->Join();
  ddl_render_thread_ = nullptr;
  while (!ddls_.empty())
    ddls_.pop();
}

std::unique_ptr<SkDeferredDisplayList> SkiaRenderer::GetDDL() {
  base::AutoLock auto_lock_(lock_);
  DCHECK(surface_charaterization_.isValid());
  // Wait until DDL is generated by DDL render thread.
  while (ddls_.empty())
    condition_variable_.Wait();
  auto ddl = std::move(ddls_.front());
  ddls_.pop();
  condition_variable_.Signal();
  return ddl;
}

void SkiaRenderer::Run() {
  base::AutoLock auto_lock(lock_);
  while (true) {
    // Wait until ddls_ is consumed or surface_charaterization_ is reset.
    constexpr size_t kMaxPendingDDLS = 4;
    while (surface_charaterization_.isValid() &&
           ddls_.size() == kMaxPendingDDLS)
      condition_variable_.Wait();
    if (!surface_charaterization_.isValid())
      break;
    DCHECK_LT(ddls_.size(), kMaxPendingDDLS);
    SkDeferredDisplayListRecorder recorder(surface_charaterization_);
    std::unique_ptr<SkDeferredDisplayList> ddl;
    {
      base::AutoUnlock auto_unlock(lock_);
      Draw(recorder.getCanvas(), NextFraction());
      ddl = recorder.detach();
    }
    ddls_.push(std::move(ddl));
    condition_variable_.Signal();
  }
}

void SkiaRenderer::OnPresentation(const gfx::PresentationFeedback& feedback) {
  DCHECK(gl_surface_->SupportsPresentationCallback());
  LOG_IF(ERROR, feedback.timestamp.is_null()) << "Last frame is discarded!";
}

}  // namespace ui
