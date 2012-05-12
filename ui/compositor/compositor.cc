// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor.h"

#include "base/command_line.h"
#include "base/threading/thread_restrictions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/images/SkImageEncoder.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/test_web_graphics_context_3d.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "webkit/glue/webthread_impl.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_impl.h"

namespace {

const double kDefaultRefreshRate = 60.0;
const double kTestRefreshRate = 100.0;

webkit_glue::WebThreadImpl* g_compositor_thread = NULL;

bool test_compositor_enabled = false;

ui::ContextFactory* g_context_factory = NULL;

}  // anonymous namespace

namespace ui {

// static
ContextFactory* ContextFactory::GetInstance() {
  // We leak the shared resources so that we don't race with
  // the tear down of the gl_bindings.
  if (!g_context_factory) {
    DVLOG(1) << "Using DefaultSharedResource";
    scoped_ptr<DefaultContextFactory> instance(
        new DefaultContextFactory());
    if (instance->Initialize())
      g_context_factory = instance.release();
  }
  return g_context_factory;
}

// static
void ContextFactory::SetInstance(ContextFactory* instance) {
  g_context_factory = instance;
}

DefaultContextFactory::DefaultContextFactory() {
}

DefaultContextFactory::~DefaultContextFactory() {
}

bool DefaultContextFactory::Initialize() {
  // The following line of code exists soley to disable IO restrictions
  // on this thread long enough to perform the GL bindings.
  // TODO(wjmaclean) Remove this when GL initialisation cleaned up.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  if (!gfx::GLSurface::InitializeOneOff() ||
      gfx::GetGLImplementation() == gfx::kGLImplementationNone) {
    LOG(ERROR) << "Could not load the GL bindings";
    return false;
  }
  return true;
}

WebKit::WebGraphicsContext3D* DefaultContextFactory::CreateContext(
    Compositor* compositor) {
  return CreateContextCommon(compositor, false);
}

WebKit::WebGraphicsContext3D* DefaultContextFactory::CreateOffscreenContext(
    Compositor* compositor) {
  return CreateContextCommon(compositor, true);
}

void DefaultContextFactory::RemoveCompositor(Compositor* compositor) {
}

WebKit::WebGraphicsContext3D* DefaultContextFactory::CreateContextCommon(
    Compositor* compositor,
    bool offscreen) {
  WebKit::WebGraphicsContext3D::Attributes attrs;
  attrs.shareResources = true;
  WebKit::WebGraphicsContext3D* context =
      offscreen ?
      webkit::gpu::WebGraphicsContext3DInProcessImpl::CreateForWebView(
          attrs, false) :
      webkit::gpu::WebGraphicsContext3DInProcessImpl::CreateForWindow(
          attrs, compositor->widget(), share_group_.get());
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kDisableUIVsync)) {
    context->makeContextCurrent();
    gfx::GLContext* gl_context = gfx::GLContext::GetCurrent();
    gl_context->SetSwapInterval(1);
    gl_context->ReleaseCurrent(NULL);
  }
  return context;
}

Texture::Texture(bool flipped, const gfx::Size& size)
    : texture_id_(0),
      flipped_(flipped),
      size_(size) {
}

Texture::~Texture() {
}

Compositor::Compositor(CompositorDelegate* delegate,
                       gfx::AcceleratedWidget widget)
    : delegate_(delegate),
      root_layer_(NULL),
      widget_(widget),
      root_web_layer_(WebKit::WebLayer::create()),
      swap_posted_(false),
      device_scale_factor_(0.0f) {
  WebKit::WebLayerTreeView::Settings settings;
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  settings.showFPSCounter =
      command_line->HasSwitch(switches::kUIShowFPSCounter);
  settings.showPlatformLayerTree =
      command_line->HasSwitch(switches::kUIShowLayerTree);
  settings.refreshRate = test_compositor_enabled ?
      kTestRefreshRate : kDefaultRefreshRate;
  settings.partialSwapEnabled =
      command_line->HasSwitch(switches::kUIEnablePartialSwap);
  settings.perTilePainting =
    command_line->HasSwitch(switches::kUIEnablePerTilePainting);

  host_.initialize(this, root_web_layer_, settings);
  root_web_layer_.setAnchorPoint(WebKit::WebFloatPoint(0.f, 0.f));
}

Compositor::~Compositor() {
  // Don't call |CompositorDelegate::ScheduleDraw| from this point.
  delegate_ = NULL;
  // There's a cycle between |root_web_layer_| and |host_|, which results in
  // leaking and/or crashing. Explicitly set the root layer to NULL so the cycle
  // is broken.
  host_.setRootLayer(NULL);
  if (root_layer_)
    root_layer_->SetCompositor(NULL);
  if (!test_compositor_enabled)
    ContextFactory::GetInstance()->RemoveCompositor(this);
}

void Compositor::Initialize(bool use_thread) {
  if (use_thread)
    g_compositor_thread = new webkit_glue::WebThreadImpl("Browser Compositor");
  WebKit::WebCompositor::initialize(g_compositor_thread);
}

void Compositor::Terminate() {
  WebKit::WebCompositor::shutdown();
  if (g_compositor_thread) {
    delete g_compositor_thread;
    g_compositor_thread = NULL;
  }
}

void Compositor::ScheduleDraw() {
  if (g_compositor_thread) {
    // TODO(nduca): Temporary while compositor calls
    // compositeImmediately() directly.
    layout();
    host_.composite();
  } else if (delegate_) {
    delegate_->ScheduleDraw();
  }
}

void Compositor::SetRootLayer(Layer* root_layer) {
  if (root_layer_ == root_layer)
    return;
  if (root_layer_)
    root_layer_->SetCompositor(NULL);
  root_layer_ = root_layer;
  if (root_layer_ && !root_layer_->GetCompositor())
    root_layer_->SetCompositor(this);
  root_web_layer_.removeAllChildren();
  if (root_layer_)
    root_web_layer_.addChild(root_layer_->web_layer());
}

void Compositor::Draw(bool force_clear) {
  if (!root_layer_)
    return;

  // TODO(nduca): Temporary while compositor calls
  // compositeImmediately() directly.
  layout();
  host_.composite();
  if (!g_compositor_thread && !swap_posted_)
    NotifyEnd();
}

void Compositor::ScheduleFullDraw() {
  host_.setNeedsRedraw();
}

bool Compositor::ReadPixels(SkBitmap* bitmap,
                            const gfx::Rect& bounds_in_pixel) {
  if (bounds_in_pixel.right() > size().width() ||
      bounds_in_pixel.bottom() > size().height())
    return false;
  // Convert to OpenGL coordinates.
  gfx::Point new_origin(
      bounds_in_pixel.x(),
      size().height() - bounds_in_pixel.height() - bounds_in_pixel.y());

  bitmap->setConfig(SkBitmap::kARGB_8888_Config,
                    bounds_in_pixel.width(), bounds_in_pixel.height());
  bitmap->allocPixels();
  SkAutoLockPixels lock_image(*bitmap);
  unsigned char* pixels = static_cast<unsigned char*>(bitmap->getPixels());
  if (host_.compositeAndReadback(
          pixels, gfx::Rect(new_origin, bounds_in_pixel.size()))) {
    SwizzleRGBAToBGRAAndFlip(pixels, bounds_in_pixel.size());
    return true;
  }
  return false;
}

void Compositor::SetScaleAndSize(float scale, const gfx::Size& size_in_pixel) {
  DCHECK(scale > 0);
  if (size_in_pixel.IsEmpty() || scale <= 0)
    return;
  size_ = size_in_pixel;
  host_.setViewportSize(size_in_pixel);
  root_web_layer_.setBounds(size_in_pixel);

  if (device_scale_factor_ != scale && IsDIPEnabled()) {
    device_scale_factor_ = scale;
    if (root_layer_)
      root_layer_->OnDeviceScaleFactorChanged(scale);
  }
}

void Compositor::AddObserver(CompositorObserver* observer) {
  observer_list_.AddObserver(observer);
}

void Compositor::RemoveObserver(CompositorObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

bool Compositor::HasObserver(CompositorObserver* observer) {
  return observer_list_.HasObserver(observer);
}

void Compositor::OnSwapBuffersPosted() {
  swap_posted_ = true;
}

void Compositor::OnSwapBuffersComplete() {
  DCHECK(swap_posted_);
  swap_posted_ = false;
  NotifyEnd();
}

void Compositor::OnSwapBuffersAborted() {
  if (swap_posted_) {
    swap_posted_ = false;
    NotifyEnd();
  }
}

void Compositor::updateAnimations(double frameBeginTime) {
}

void Compositor::layout() {
  if (root_layer_)
    root_layer_->SendDamagedRects();
}

void Compositor::applyScrollAndScale(const WebKit::WebSize& scrollDelta,
                                     float scaleFactor) {
}

WebKit::WebGraphicsContext3D* Compositor::createContext3D() {
  if (test_compositor_enabled) {
    ui::TestWebGraphicsContext3D* test_context =
      new ui::TestWebGraphicsContext3D();
   test_context->Initialize();
   return test_context;
  } else {
    return ContextFactory::GetInstance()->CreateContext(this);
  }
}

void Compositor::didRebindGraphicsContext(bool success) {
}

void Compositor::didCommitAndDrawFrame() {
  FOR_EACH_OBSERVER(CompositorObserver,
                    observer_list_,
                    OnCompositingStarted(this));
}

void Compositor::didCompleteSwapBuffers() {
  NotifyEnd();
}

void Compositor::scheduleComposite() {
  ScheduleDraw();
}

void Compositor::SwizzleRGBAToBGRAAndFlip(unsigned char* pixels,
                                          const gfx::Size& image_size) {
  // Swizzle from RGBA to BGRA
  size_t bitmap_size = 4 * image_size.width() * image_size.height();
  for(size_t i = 0; i < bitmap_size; i += 4)
    std::swap(pixels[i], pixels[i + 2]);

  // Vertical flip to transform from GL co-ords
  size_t row_size = 4 * image_size.width();
  scoped_array<unsigned char> tmp_row(new unsigned char[row_size]);
  for(int row = 0; row < image_size.height() / 2; row++) {
    memcpy(tmp_row.get(),
           &pixels[row * row_size],
           row_size);
    memcpy(&pixels[row * row_size],
           &pixels[bitmap_size - (row + 1) * row_size],
           row_size);
    memcpy(&pixels[bitmap_size - (row + 1) * row_size],
           tmp_row.get(),
           row_size);
  }
}

void Compositor::NotifyEnd() {
  FOR_EACH_OBSERVER(CompositorObserver,
                    observer_list_,
                    OnCompositingEnded(this));
}

COMPOSITOR_EXPORT void SetupTestCompositor() {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableTestCompositor)) {
    test_compositor_enabled = true;
  }
}

COMPOSITOR_EXPORT void DisableTestCompositor() {
  test_compositor_enabled = false;
}

COMPOSITOR_EXPORT bool IsTestCompositorEnabled() {
  return test_compositor_enabled;
}

}  // namespace ui
