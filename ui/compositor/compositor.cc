// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/threading/thread_restrictions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/images/SkImageEncoder.h"
#include "third_party/WebKit/Source/Platform/chromium/public/Platform.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCompositorSupport.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFloatPoint.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCompositorOutputSurface.h"
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

#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#endif

namespace {

const double kDefaultRefreshRate = 60.0;
const double kTestRefreshRate = 100.0;

webkit_glue::WebThreadImpl* g_compositor_thread = NULL;

bool test_compositor_enabled = false;

ui::ContextFactory* g_context_factory = NULL;

}  // namespace

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

WebKit::WebGraphicsContext3D* DefaultContextFactory::CreateOffscreenContext() {
  return CreateContextCommon(NULL, true);
}

void DefaultContextFactory::RemoveCompositor(Compositor* compositor) {
}

WebKit::WebGraphicsContext3D* DefaultContextFactory::CreateContextCommon(
    Compositor* compositor,
    bool offscreen) {
  DCHECK(offscreen || compositor);
  WebKit::WebGraphicsContext3D::Attributes attrs;
  attrs.depth = false;
  attrs.stencil = false;
  attrs.antialias = false;
  attrs.shareResources = true;
  WebKit::WebGraphicsContext3D* context =
      offscreen ?
      webkit::gpu::WebGraphicsContext3DInProcessImpl::CreateForWebView(
          attrs, false) :
      webkit::gpu::WebGraphicsContext3DInProcessImpl::CreateForWindow(
          attrs, compositor->widget(), share_group_.get());
  if (!context)
    return NULL;

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!offscreen) {
    context->makeContextCurrent();
    gfx::GLContext* gl_context = gfx::GLContext::GetCurrent();
    bool vsync = !command_line->HasSwitch(switches::kDisableUIVsync);
    gl_context->SetSwapInterval(vsync ? 1 : 0);
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
      swap_posted_(false),
      device_scale_factor_(0.0f),
      last_started_frame_(0),
      last_ended_frame_(0),
      disable_schedule_composite_(false) {
  WebKit::WebCompositorSupport* compositor_support =
      WebKit::Platform::current()->compositorSupport();
  root_web_layer_.reset(compositor_support->createLayer());
  WebKit::WebLayerTreeView::Settings settings;
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  settings.showFPSCounter =
      command_line->HasSwitch(switches::kUIShowFPSCounter);
  settings.showPlatformLayerTree =
      command_line->HasSwitch(switches::kUIShowLayerTree);
  settings.refreshRate =
      test_compositor_enabled ? kTestRefreshRate : kDefaultRefreshRate;

  root_web_layer_->setAnchorPoint(WebKit::WebFloatPoint(0.f, 0.f));
  host_.reset(compositor_support->createLayerTreeView(this, *root_web_layer_,
                                                      settings));
  host_->setSurfaceReady();
}

Compositor::~Compositor() {
  // Don't call |CompositorDelegate::ScheduleDraw| from this point.
  delegate_ = NULL;
  if (root_layer_)
    root_layer_->SetCompositor(NULL);

  // Stop all outstanding draws before telling the ContextFactory to tear
  // down any contexts that the |host_| may rely upon.
  host_.reset();

  if (!test_compositor_enabled)
    ContextFactory::GetInstance()->RemoveCompositor(this);
}

void Compositor::Initialize(bool use_thread) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  WebKit::WebCompositorSupport* compositor_support =
      WebKit::Platform::current()->compositorSupport();
  // These settings must be applied before we initialize the compositor.
  compositor_support->setPartialSwapEnabled(
      command_line->HasSwitch(switches::kUIEnablePartialSwap));
  compositor_support->setPerTilePaintingEnabled(
      command_line->HasSwitch(switches::kUIEnablePerTilePainting));
  if (use_thread)
    g_compositor_thread = new webkit_glue::WebThreadImpl("Browser Compositor");
  compositor_support->initialize(g_compositor_thread);
}

void Compositor::Terminate() {
  WebKit::Platform::current()->compositorSupport()->shutdown();
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
    host_->composite();
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
  root_web_layer_->removeAllChildren();
  if (root_layer_)
    root_web_layer_->addChild(root_layer_->web_layer());
}

void Compositor::Draw(bool force_clear) {
  if (!root_layer_)
    return;

  last_started_frame_++;
  if (!g_compositor_thread)
    FOR_EACH_OBSERVER(CompositorObserver,
                      observer_list_,
                      OnCompositingWillStart(this));

  // TODO(nduca): Temporary while compositor calls
  // compositeImmediately() directly.
  layout();
  host_->composite();
  if (!g_compositor_thread && !swap_posted_)
    NotifyEnd();
}

void Compositor::ScheduleFullDraw() {
  host_->setNeedsRedraw();
}

bool Compositor::ReadPixels(SkBitmap* bitmap,
                            const gfx::Rect& bounds_in_pixel) {
  if (bounds_in_pixel.right() > size().width() ||
      bounds_in_pixel.bottom() > size().height())
    return false;
  bitmap->setConfig(SkBitmap::kARGB_8888_Config,
                    bounds_in_pixel.width(), bounds_in_pixel.height());
  bitmap->allocPixels();
  SkAutoLockPixels lock_image(*bitmap);
  unsigned char* pixels = static_cast<unsigned char*>(bitmap->getPixels());
  return host_->compositeAndReadback(pixels, bounds_in_pixel);
}

void Compositor::SetScaleAndSize(float scale, const gfx::Size& size_in_pixel) {
  DCHECK_GT(scale, 0);
  if (size_in_pixel.IsEmpty() || scale <= 0)
    return;
  size_ = size_in_pixel;
  host_->setViewportSize(size_in_pixel);
  root_web_layer_->setBounds(size_in_pixel);

  if (device_scale_factor_ != scale) {
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

bool Compositor::IsThreaded() const {
  return g_compositor_thread != NULL;
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
  FOR_EACH_OBSERVER(CompositorObserver,
                    observer_list_,
                    OnCompositingAborted(this));
}

void Compositor::updateAnimations(double frameBeginTime) {
}

void Compositor::layout() {
  // We're sending damage that will be addressed during this composite
  // cycle, so we don't need to schedule another composite to address it.
  disable_schedule_composite_ = true;
  if (root_layer_)
    root_layer_->SendDamagedRects();
  disable_schedule_composite_ = false;
}

void Compositor::applyScrollAndScale(const WebKit::WebSize& scrollDelta,
                                     float scaleFactor) {
}

// Adapts a pure WebGraphicsContext3D into a WebCompositorOutputSurface.
class WebGraphicsContextToOutputSurfaceAdapter :
    public WebKit::WebCompositorOutputSurface {
public:
    explicit WebGraphicsContextToOutputSurfaceAdapter(
        WebKit::WebGraphicsContext3D* context)
        : m_context3D(context)
        , m_client(0)
    {
    }

    virtual bool bindToClient(
        WebKit::WebCompositorOutputSurfaceClient* client) OVERRIDE
    {
        DCHECK(client);
        if (!m_context3D->makeContextCurrent())
            return false;
        m_client = client;
        return true;
    }

    virtual const Capabilities& capabilities() const OVERRIDE
    {
        return m_capabilities;
    }

    virtual WebKit::WebGraphicsContext3D* context3D() const OVERRIDE
    {
        return m_context3D.get();
    }

    virtual void sendFrameToParentCompositor(
        const WebKit::WebCompositorFrame&) OVERRIDE
    {
    }

private:
    scoped_ptr<WebKit::WebGraphicsContext3D> m_context3D;
    Capabilities m_capabilities;
    WebKit::WebCompositorOutputSurfaceClient* m_client;
};

WebKit::WebCompositorOutputSurface* Compositor::createOutputSurface() {
  if (test_compositor_enabled) {
    ui::TestWebGraphicsContext3D* test_context =
      new ui::TestWebGraphicsContext3D();
    test_context->Initialize();
    return new WebGraphicsContextToOutputSurfaceAdapter(test_context);
  } else {
    return new WebGraphicsContextToOutputSurfaceAdapter(
        ContextFactory::GetInstance()->CreateContext(this));
  }
}

void Compositor::didRecreateOutputSurface(bool success) {
}

// Called once per draw in single-threaded compositor mode and potentially
// many times between draws in the multi-threaded compositor mode.
void Compositor::didCommit() {
  FOR_EACH_OBSERVER(CompositorObserver,
                    observer_list_,
                    OnCompositingDidCommit(this));
}

void Compositor::didCommitAndDrawFrame() {
  // TODO(backer): Plumb through an earlier impl side will start.
  if (g_compositor_thread)
    FOR_EACH_OBSERVER(CompositorObserver,
                      observer_list_,
                      OnCompositingWillStart(this));

  FOR_EACH_OBSERVER(CompositorObserver,
                    observer_list_,
                    OnCompositingStarted(this));
}

void Compositor::didCompleteSwapBuffers() {
  NotifyEnd();
}

void Compositor::scheduleComposite() {
  if (!disable_schedule_composite_)
    ScheduleDraw();
}

void Compositor::NotifyEnd() {
  last_ended_frame_++;
  FOR_EACH_OBSERVER(CompositorObserver,
                    observer_list_,
                    OnCompositingEnded(this));
}

COMPOSITOR_EXPORT void SetupTestCompositor() {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableTestCompositor)) {
    test_compositor_enabled = true;
  }
#if defined(OS_CHROMEOS)
  // If the test is running on the chromeos envrionment (such as
  // device or vm bots), use the real compositor.
  if (base::chromeos::IsRunningOnChromeOS())
    test_compositor_enabled = false;
#endif
}

COMPOSITOR_EXPORT void DisableTestCompositor() {
  test_compositor_enabled = false;
}

COMPOSITOR_EXPORT bool IsTestCompositorEnabled() {
  return test_compositor_enabled;
}

}  // namespace ui
