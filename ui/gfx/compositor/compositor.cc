// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/compositor.h"

#include "base/command_line.h"
#include "third_party/skia/include/images/SkImageEncoder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "ui/gfx/compositor/compositor_observer.h"
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

const double kDefaultRefreshRate = 60.0;
const double kTestRefreshRate = 100.0;

webkit_glue::WebThreadImpl* g_compositor_thread = NULL;

bool test_compositor_enabled = false;

}  // anonymous namespace

namespace ui {

SharedResources::SharedResources() : initialized_(false) {
}


SharedResources::~SharedResources() {
}

// static
SharedResources* SharedResources::GetInstance() {
  // We use LeakySingletonTraits so that we don't race with
  // the tear down of the gl_bindings.
  SharedResources* instance = Singleton<SharedResources,
      LeakySingletonTraits<SharedResources> >::get();
  if (instance->Initialize()) {
    return instance;
  } else {
    instance->Destroy();
    return NULL;
  }
}

bool SharedResources::Initialize() {
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

void SharedResources::Destroy() {
  context_ = NULL;
  surface_ = NULL;

  initialized_ = false;
}

gfx::ScopedMakeCurrent* SharedResources::GetScopedMakeCurrent() {
  DCHECK(initialized_);
  if (initialized_)
    return new gfx::ScopedMakeCurrent(context_.get(), surface_.get());
  else
    return NULL;
}

gfx::GLShareGroup* SharedResources::GetShareGroup() {
  DCHECK(initialized_);
  return context_->share_group();
}

Texture::Texture(bool flipped, const gfx::Size& size)
    : texture_id_(0),
      flipped_(flipped),
      size_(size) {
}

Texture::~Texture() {
}

Compositor::Compositor(CompositorDelegate* delegate,
                       gfx::AcceleratedWidget widget,
                       const gfx::Size& size)
    : delegate_(delegate),
      size_(size),
      root_layer_(NULL),
      widget_(widget),
      root_web_layer_(WebKit::WebLayer::create()) {
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
#if defined(PER_TILE_PAINTING)
  settings.perTilePainting =
    command_line->HasSwitch(switches::kUIEnablePerTilePainting);
#endif

  host_ = WebKit::WebLayerTreeView::create(this, root_web_layer_, settings);
  root_web_layer_.setAnchorPoint(WebKit::WebFloatPoint(0.f, 0.f));
  WidgetSizeChanged(size_);
}

Compositor::~Compositor() {
  // There's a cycle between |root_web_layer_| and |host_|, which results in
  // leaking and/or crashing. Explicitly set the root layer to NULL so the cycle
  // is broken.
  host_.setRootLayer(NULL);
  if (root_layer_)
    root_layer_->SetCompositor(NULL);
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
  } else {
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
  if (!g_compositor_thread)
    NotifyEnd();
}

void Compositor::ScheduleFullDraw() {
  host_.setNeedsRedraw();
}

bool Compositor::ReadPixels(SkBitmap* bitmap, const gfx::Rect& bounds) {
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

void Compositor::WidgetSizeChanged(const gfx::Size& size) {
  if (size.IsEmpty())
    return;
  size_ = size;
  host_.setViewportSize(size_);
  root_web_layer_.setBounds(size_);
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


void Compositor::updateAnimations(double frameBeginTime) {
}

void Compositor::layout() {
  if (root_layer_)
    root_layer_->SendDamagedRect();
}

void Compositor::applyScrollAndScale(const WebKit::WebSize& scrollDelta,
                                     float scaleFactor) {
}

WebKit::WebGraphicsContext3D* Compositor::createContext3D() {
  WebKit::WebGraphicsContext3D* context;
  if (test_compositor_enabled) {
    // Use context that results in no rendering to the screen.
    TestWebGraphicsContext3D* test_context = new TestWebGraphicsContext3D();
    test_context->Initialize();
    context = test_context;
  } else {
    gfx::GLShareGroup* share_group =
        SharedResources::GetInstance()->GetShareGroup();
    WebKit::WebGraphicsContext3D::Attributes attrs;
    context = webkit::gpu::WebGraphicsContext3DInProcessImpl::CreateForWindow(
        attrs, widget_, share_group);
  }

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kDisableUIVsync)) {
    context->makeContextCurrent();
    gfx::GLContext* gl_context = gfx::GLContext::GetCurrent();
    gl_context->SetSwapInterval(1);
    gl_context->ReleaseCurrent(NULL);
  }

  return context;
}

void Compositor::didCompleteSwapBuffers() {
  NotifyEnd();
}

void Compositor::didRebindGraphicsContext(bool success) {
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

}  // namespace ui
