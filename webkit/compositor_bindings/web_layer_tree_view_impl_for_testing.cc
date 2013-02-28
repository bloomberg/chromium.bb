// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/compositor_bindings/web_layer_tree_view_impl_for_testing.h"

#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "cc/context_provider.h"
#include "cc/fake_web_graphics_context_3d.h"
#include "cc/input_handler.h"
#include "cc/layer.h"
#include "cc/layer_tree_host.h"
#include "cc/output_surface.h"
#include "cc/switches.h"
#include "cc/thread.h"
#include "cc/thread_impl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/Platform.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebInputHandler.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayerTreeView.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayerTreeViewClient.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRenderingStats.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSharedGraphicsContext3D.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "webkit/compositor_bindings/web_compositor_support_impl.h"
#include "webkit/compositor_bindings/web_compositor_support_software_output_device.h"
#include "webkit/compositor_bindings/web_layer_impl.h"
#include "webkit/compositor_bindings/web_rendering_stats_impl.h"
#include "webkit/compositor_bindings/web_to_ccinput_handler_adapter.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

namespace WebKit {
WebLayerTreeViewImplForTesting::WebLayerTreeViewImplForTesting(
    RenderingType type,
    webkit_support::DRTLayerTreeViewClient* client)
    : type_(type),
      drt_client_(client),
      client_(NULL) {}

WebLayerTreeViewImplForTesting::WebLayerTreeViewImplForTesting(
    RenderingType type,
    WebKit::WebLayerTreeViewClient* client)
    : type_(type),
      drt_client_(NULL),
      client_(client) {}

WebLayerTreeViewImplForTesting::~WebLayerTreeViewImplForTesting() {}

bool WebLayerTreeViewImplForTesting::initialize(
    scoped_ptr<cc::Thread> compositor_thread) {
  cc::LayerTreeSettings settings;
  // Accelerated animations are disabled for layout tests, but enabled for unit
  // tests.
  settings.acceleratedAnimationEnabled = type_ == FAKE_CONTEXT;
  layer_tree_host_ =
      cc::LayerTreeHost::create(this, settings, compositor_thread.Pass());
  if (!layer_tree_host_.get())
    return false;
  return true;
}

void WebLayerTreeViewImplForTesting::setSurfaceReady() {
  layer_tree_host_->setSurfaceReady();
}

void WebLayerTreeViewImplForTesting::setRootLayer(const WebLayer& root) {
  layer_tree_host_->setRootLayer(static_cast<const WebLayerImpl*>(&root)
                                     ->layer());
}

void WebLayerTreeViewImplForTesting::clearRootLayer() {
  layer_tree_host_->setRootLayer(scoped_refptr<cc::Layer>());
}

void WebLayerTreeViewImplForTesting::setViewportSize(
    const WebSize& layout_viewport_size,
    const WebSize& device_viewport_size) {
  layer_tree_host_->setViewportSize(layout_viewport_size, device_viewport_size);
}

WebSize WebLayerTreeViewImplForTesting::layoutViewportSize() const {
  return layer_tree_host_->layoutViewportSize();
}

WebSize WebLayerTreeViewImplForTesting::deviceViewportSize() const {
  return layer_tree_host_->deviceViewportSize();
}

void WebLayerTreeViewImplForTesting::setDeviceScaleFactor(
    float device_scale_factor) {
  layer_tree_host_->setDeviceScaleFactor(device_scale_factor);
}

float WebLayerTreeViewImplForTesting::deviceScaleFactor() const {
  return layer_tree_host_->deviceScaleFactor();
}

void WebLayerTreeViewImplForTesting::setBackgroundColor(WebColor color) {
  layer_tree_host_->setBackgroundColor(color);
}

void WebLayerTreeViewImplForTesting::setHasTransparentBackground(
    bool transparent) {
  layer_tree_host_->setHasTransparentBackground(transparent);
}

void WebLayerTreeViewImplForTesting::setVisible(bool visible) {
  layer_tree_host_->setVisible(visible);
}

void WebLayerTreeViewImplForTesting::setPageScaleFactorAndLimits(
    float page_scale_factor,
    float minimum,
    float maximum) {
  layer_tree_host_->setPageScaleFactorAndLimits(
      page_scale_factor, minimum, maximum);
}

void WebLayerTreeViewImplForTesting::startPageScaleAnimation(
    const WebPoint& scroll,
    bool use_anchor,
    float new_page_scale,
    double duration_sec) {}

void WebLayerTreeViewImplForTesting::setNeedsAnimate() {
  layer_tree_host_->setNeedsAnimate();
}

void WebLayerTreeViewImplForTesting::setNeedsRedraw() {
  layer_tree_host_->setNeedsRedraw();
}

bool WebLayerTreeViewImplForTesting::commitRequested() const {
  return layer_tree_host_->commitRequested();
}

void WebLayerTreeViewImplForTesting::composite() {
  layer_tree_host_->composite();
}

void WebLayerTreeViewImplForTesting::updateAnimations(
    double frame_begin_timeSeconds) {
  base::TimeTicks frame_begin_time = base::TimeTicks::FromInternalValue(
      frame_begin_timeSeconds * base::Time::kMicrosecondsPerMillisecond);
  layer_tree_host_->updateAnimations(frame_begin_time);
}

void WebLayerTreeViewImplForTesting::didStopFlinging() {}

bool WebLayerTreeViewImplForTesting::compositeAndReadback(void* pixels,
                                                          const WebRect& rect) {
  return layer_tree_host_->compositeAndReadback(pixels, rect);
}

void WebLayerTreeViewImplForTesting::finishAllRendering() {
  layer_tree_host_->finishAllRendering();
}

void WebLayerTreeViewImplForTesting::setDeferCommits(bool defer_commits) {
  layer_tree_host_->setDeferCommits(defer_commits);
}

void WebLayerTreeViewImplForTesting::renderingStats(WebRenderingStats&) const {}

void WebLayerTreeViewImplForTesting::willBeginFrame() {}

void WebLayerTreeViewImplForTesting::didBeginFrame() {}

void WebLayerTreeViewImplForTesting::animate(
    double monotonic_frame_begin_time) {
}

void WebLayerTreeViewImplForTesting::layout() {
  if (drt_client_)
    drt_client_->Layout();
  if (client_)
    client_->layout();
}

void WebLayerTreeViewImplForTesting::applyScrollAndScale(
    gfx::Vector2d scroll_delta,
    float page_scale) {}

scoped_ptr<cc::OutputSurface>
WebLayerTreeViewImplForTesting::createOutputSurface() {
  scoped_ptr<cc::OutputSurface> surface;
  switch (type_) {
    case FAKE_CONTEXT: {
      scoped_ptr<WebGraphicsContext3D> context3d(
          new cc::FakeWebGraphicsContext3D);
      surface.reset(new cc::OutputSurface(context3d.Pass()));
      break;
    }
    case SOFTWARE_CONTEXT: {
      using webkit::WebCompositorSupportSoftwareOutputDevice;
      scoped_ptr<WebCompositorSupportSoftwareOutputDevice> software_device =
          make_scoped_ptr(new WebCompositorSupportSoftwareOutputDevice);
      surface.reset(new cc::OutputSurface(
          software_device.PassAs<cc::SoftwareOutputDevice>()));
      break;
    }
    case MESA_CONTEXT: {
      scoped_ptr<WebGraphicsContext3D> context3d(
          WebKit::Platform::current()->createOffscreenGraphicsContext3D(
              WebGraphicsContext3D::Attributes()));
      surface.reset(new cc::OutputSurface(context3d.Pass()));
      break;
    }
  }
  return surface.Pass();
}

void WebLayerTreeViewImplForTesting::didRecreateOutputSurface(bool success) {}

scoped_ptr<cc::InputHandler>
WebLayerTreeViewImplForTesting::createInputHandler() {
  return scoped_ptr<cc::InputHandler>();
}

void WebLayerTreeViewImplForTesting::willCommit() {}

void WebLayerTreeViewImplForTesting::didCommit() {}

void WebLayerTreeViewImplForTesting::didCommitAndDrawFrame() {}

void WebLayerTreeViewImplForTesting::didCompleteSwapBuffers() {}

void WebLayerTreeViewImplForTesting::scheduleComposite() {
  if (drt_client_)
    drt_client_->ScheduleComposite();
  if (client_)
    client_->scheduleComposite();
}

class WebLayerTreeViewImplForTesting::MainThreadContextProvider :
    public cc::ContextProvider {
 public:
  virtual bool InitializeOnMainThread() OVERRIDE { return true; }
  virtual bool BindToCurrentThread() OVERRIDE { return true; }

  virtual WebKit::WebGraphicsContext3D* Context3d() OVERRIDE {
    return WebSharedGraphicsContext3D::mainThreadContext();
  }
  virtual class GrContext* GrContext() OVERRIDE {
    return WebSharedGraphicsContext3D::mainThreadGrContext();
  }

  virtual void VerifyContexts() OVERRIDE {}

 protected:
  virtual ~MainThreadContextProvider() {}
};

scoped_refptr<cc::ContextProvider>
WebLayerTreeViewImplForTesting::OffscreenContextProviderForMainThread() {
  if (!contexts_main_thread_)
    contexts_main_thread_ = new MainThreadContextProvider;
  return contexts_main_thread_;
}

class WebLayerTreeViewImplForTesting::CompositorThreadContextProvider :
    public cc::ContextProvider {
 public:
  CompositorThreadContextProvider() : destroyed_(false) {}

  virtual bool InitializeOnMainThread() OVERRIDE {
    return WebSharedGraphicsContext3D::createCompositorThreadContext();
  }
  virtual bool BindToCurrentThread() OVERRIDE {
    return Context3d()->makeContextCurrent();
  }

  virtual WebKit::WebGraphicsContext3D* Context3d() OVERRIDE {
    return WebSharedGraphicsContext3D::compositorThreadContext();
  }
  virtual class GrContext* GrContext() OVERRIDE {
    return WebSharedGraphicsContext3D::compositorThreadGrContext();
  }

  virtual void VerifyContexts() OVERRIDE {
    if (Context3d() && !Context3d()->isContextLost())
      return;
    base::AutoLock lock(destroyed_lock_);
    destroyed_ = true;
  }

  bool DestroyedOnMainThread() {
    base::AutoLock lock(destroyed_lock_);
    return destroyed_;
  }

 protected:
  virtual ~CompositorThreadContextProvider() {}

 private:
  base::Lock destroyed_lock_;
  bool destroyed_;
};

scoped_refptr<cc::ContextProvider>
WebLayerTreeViewImplForTesting::OffscreenContextProviderForCompositorThread() {
  if (!contexts_compositor_thread_ ||
      contexts_compositor_thread_->DestroyedOnMainThread())
    contexts_compositor_thread_ = new CompositorThreadContextProvider;
  return contexts_compositor_thread_;
}

}  // namespace WebKit
