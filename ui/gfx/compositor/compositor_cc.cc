// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/compositor_cc.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFloatPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSize.h"
#include "ui/gfx/compositor/layer.h"
#include "webkit/glue/webthread_impl.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_impl.h"

namespace {
webkit_glue::WebThreadImpl* g_compositor_thread = NULL;
}  // anonymous namespace

namespace ui {

TextureCC::TextureCC() {
}

void TextureCC::SetCanvas(const SkCanvas& canvas,
                          const gfx::Point& origin,
                          const gfx::Size& overall_size) {
}

void TextureCC::Draw(const ui::TextureDrawParams& params,
                     const gfx::Rect& clip_bounds_in_texture) {
}

CompositorCC::CompositorCC(CompositorDelegate* delegate,
                           gfx::AcceleratedWidget widget,
                           const gfx::Size& size)
    : Compositor(delegate, size),
      widget_(widget),
      root_web_layer_(WebKit::WebLayer::create(this)) {
  WebKit::WebLayerTreeView::Settings settings;
  settings.enableCompositorThread = !!g_compositor_thread;
  host_ = WebKit::WebLayerTreeView::create(this, root_web_layer_, settings);
  root_web_layer_.setAnchorPoint(WebKit::WebFloatPoint(0.f, 0.f));
  OnWidgetSizeChanged();
}

CompositorCC::~CompositorCC() {
}

void CompositorCC::InitializeThread() {
  g_compositor_thread = new webkit_glue::WebThreadImpl("Browser Compositor");
  WebKit::WebCompositor::setThread(g_compositor_thread);
}

void CompositorCC::TerminateThread() {
  DCHECK(g_compositor_thread);
  delete g_compositor_thread;
  g_compositor_thread = NULL;
}

Texture* CompositorCC::CreateTexture() {
  return new TextureCC();
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
  root_web_layer_.addChild(root_layer()->web_layer());
}

void CompositorCC::DrawTree() {
  host_.composite();
}

void CompositorCC::animateAndLayout(double frameBeginTime) {
}

void CompositorCC::applyScrollDelta(const WebKit::WebSize&) {
}

WebKit::WebGraphicsContext3D* CompositorCC::createContext3D() {
  WebKit::WebGraphicsContext3D* context =
      new webkit::gpu::WebGraphicsContext3DInProcessImpl(widget_, NULL);
  WebKit::WebGraphicsContext3D::Attributes attrs;
  context->initialize(attrs, 0, true);
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
