// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/compositor_cc.h"

#include "base/command_line.h"
#include "third_party/skia/include/images/SkImageEncoder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFloatPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSize.h"
#include "ui/gfx/compositor/compositor_switches.h"
#include "ui/gfx/compositor/test_web_graphics_context_3d.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_surface.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/scoped_make_current.h"
#include "webkit/glue/webthread_impl.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_impl.h"

namespace {
webkit_glue::WebThreadImpl* g_compositor_thread = NULL;
}  // anonymous namespace

namespace ui {

SharedResourcesCC::SharedResourcesCC() : initialized_(false) {
}


SharedResourcesCC::~SharedResourcesCC() {
}

// static
SharedResourcesCC* SharedResourcesCC::GetInstance() {
  // We use LeakySingletonTraits so that we don't race with
  // the tear down of the gl_bindings.
  SharedResourcesCC* instance = Singleton<SharedResourcesCC,
      LeakySingletonTraits<SharedResourcesCC> >::get();
  if (instance->Initialize()) {
    return instance;
  } else {
    instance->Destroy();
    return NULL;
  }
}

bool SharedResourcesCC::Initialize() {
  if (initialized_)
    return true;

  {
    // The following line of code exists soley to disable IO restrictions
    // on this thread long enough to perform the GL bindings.
    // TODO(wjmaclean) Remove this when GL initialisation cleaned up.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    if (!gfx::GLSurface::InitializeOneOff() ||
        gfx::GetGLImplementation() == gfx::kGLImplementationNone) {
      LOG(ERROR) << "Could not load the GL bindings";
      return false;
    }
  }

  surface_ = gfx::GLSurface::CreateOffscreenGLSurface(false, gfx::Size(1, 1));
  if (!surface_.get()) {
    LOG(ERROR) << "Unable to create offscreen GL surface.";
    return false;
  }

  context_ = gfx::GLContext::CreateGLContext(
      NULL, surface_.get(), gfx::PreferIntegratedGpu);
  if (!context_.get()) {
    LOG(ERROR) << "Unable to create GL context.";
    return false;
  }

  initialized_ = true;
  return true;
}

void SharedResourcesCC::Destroy() {
  context_ = NULL;
  surface_ = NULL;

  initialized_ = false;
}

gfx::ScopedMakeCurrent* SharedResourcesCC::GetScopedMakeCurrent() {
  DCHECK(initialized_);
  if (initialized_)
    return new gfx::ScopedMakeCurrent(context_.get(), surface_.get());
  else
    return NULL;
}

void* SharedResourcesCC::GetDisplay() {
  return surface_->GetDisplay();
}

gfx::GLShareGroup* SharedResourcesCC::GetShareGroup() {
  DCHECK(initialized_);
  return context_->share_group();
}

TextureCC::TextureCC()
    : texture_id_(0),
      flipped_(false) {
}

void TextureCC::SetCanvas(const SkCanvas& canvas,
                          const gfx::Point& origin,
                          const gfx::Size& overall_size) {
  NOTREACHED();
}

void TextureCC::Draw(const ui::TextureDrawParams& params,
                     const gfx::Rect& clip_bounds_in_texture) {
  NOTREACHED();
}

// static
bool CompositorCC::test_context_enabled_ = false;

CompositorCC::CompositorCC(CompositorDelegate* delegate,
                           gfx::AcceleratedWidget widget,
                           const gfx::Size& size)
    : Compositor(delegate, size),
      widget_(widget),
      root_web_layer_(WebKit::WebLayer::create(this)) {
  WebKit::WebLayerTreeView::Settings settings;
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  settings.showFPSCounter =
     command_line->HasSwitch(switches::kUIShowFPSCounter);
  settings.showPlatformLayerTree =
      command_line->HasSwitch(switches::kUIShowLayerTree);
  settings.enableCompositorThread = !!g_compositor_thread;
  host_ = WebKit::WebLayerTreeView::create(this, root_web_layer_, settings);
  root_web_layer_.setAnchorPoint(WebKit::WebFloatPoint(0.f, 0.f));
  OnWidgetSizeChanged();
}

CompositorCC::~CompositorCC() {
}

void CompositorCC::Initialize(bool use_thread) {
  if (use_thread)
    g_compositor_thread = new webkit_glue::WebThreadImpl("Browser Compositor");
#ifdef WEBCOMPOSITOR_HAS_INITIALIZE
  WebKit::WebCompositor::initialize(g_compositor_thread);
#else
  if (use_thread)
    WebKit::WebCompositor::setThread(g_compositor_thread);
#endif
}

void CompositorCC::Terminate() {
#ifdef WEBCOMPOSITOR_HAS_INITIALIZE
  WebKit::WebCompositor::shutdown();
#endif
  DCHECK(g_compositor_thread);
  delete g_compositor_thread;
  g_compositor_thread = NULL;
}

// static
void CompositorCC::EnableTestContextIfNecessary() {
  // TODO: only do this if command line param not set.
  test_context_enabled_ = true;
}

Texture* CompositorCC::CreateTexture() {
  NOTREACHED();
  return NULL;
}

void CompositorCC::Blur(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

void CompositorCC::ScheduleDraw() {
  if (g_compositor_thread)
    host_.composite();
  else
    Compositor::ScheduleDraw();
}

void CompositorCC::OnNotifyStart(bool clear) {
}

void CompositorCC::OnNotifyEnd() {
}

void CompositorCC::OnWidgetSizeChanged() {
  host_.setViewportSize(size());
  root_web_layer_.setBounds(size());
}

void CompositorCC::OnRootLayerChanged() {
  root_web_layer_.removeAllChildren();
  if (root_layer())
    root_web_layer_.addChild(root_layer()->web_layer());
}

void CompositorCC::DrawTree() {
  host_.composite();
}

bool CompositorCC::ReadPixels(SkBitmap* bitmap, const gfx::Rect& bounds) {
  if (bounds.right() > size().width() || bounds.bottom() > size().height())
    return false;
  // Convert to OpenGL coordinates.
  gfx::Point new_origin(bounds.x(),
                        size().height() - bounds.height() - bounds.y());

  bitmap->setConfig(SkBitmap::kARGB_8888_Config,
                    bounds.width(), bounds.height());
  bitmap->allocPixels();
  SkAutoLockPixels lock_image(*bitmap);
  unsigned char* pixels = static_cast<unsigned char*>(bitmap->getPixels());
  if (host_.compositeAndReadback(pixels,
                                 gfx::Rect(new_origin, bounds.size()))) {
    SwizzleRGBAToBGRAAndFlip(pixels, bounds.size());
    return true;
  }
  return false;
}

void CompositorCC::animateAndLayout(double frameBeginTime) {
}

void CompositorCC::applyScrollAndScale(const WebKit::WebSize& scrollDelta,
                                       float scaleFactor) {
}

void CompositorCC::applyScrollDelta(const WebKit::WebSize&) {
}

WebKit::WebGraphicsContext3D* CompositorCC::createContext3D() {
  WebKit::WebGraphicsContext3D* context;
  if (test_context_enabled_) {
    context = new TestWebGraphicsContext3D();
  } else {
    gfx::GLShareGroup* share_group =
        SharedResourcesCC::GetInstance()->GetShareGroup();
    context = new webkit::gpu::WebGraphicsContext3DInProcessImpl(
        widget_, share_group);
  }
  WebKit::WebGraphicsContext3D::Attributes attrs;
  context->initialize(attrs, 0, true);

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kDisableUIVsync)) {
    context->makeContextCurrent();
    gfx::GLContext* gl_context = gfx::GLContext::GetCurrent();
    gl_context->SetSwapInterval(1);
    gl_context->ReleaseCurrent(NULL);
  }

  return context;
}

void CompositorCC::didRebindGraphicsContext(bool success) {
}

void CompositorCC::scheduleComposite() {
  ScheduleDraw();
}

void CompositorCC::notifyNeedsComposite() {
  ScheduleDraw();
}

Compositor* Compositor::Create(CompositorDelegate* owner,
                               gfx::AcceleratedWidget widget,
                               const gfx::Size& size) {
  return new CompositorCC(owner, widget, size);
}

}  // namespace ui
