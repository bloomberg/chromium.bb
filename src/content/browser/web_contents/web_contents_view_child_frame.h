// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_CHILD_FRAME_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_CHILD_FRAME_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/web_contents/web_contents_view.h"

namespace content {

class RenderWidgetHostImpl;
class RenderWidgetHostViewChildFrame;
class WebContentsImpl;
class WebContentsViewDelegate;

class WebContentsViewChildFrame : public WebContentsView,
                                  public RenderViewHostDelegateView {
 public:
  WebContentsViewChildFrame(WebContentsImpl* web_contents,
                            WebContentsViewDelegate* delegate,
                            RenderViewHostDelegateView** delegate_view);

  WebContentsViewChildFrame(const WebContentsViewChildFrame&) = delete;
  WebContentsViewChildFrame& operator=(const WebContentsViewChildFrame&) =
      delete;

  ~WebContentsViewChildFrame() override;

  // WebContentsView implementation --------------------------------------------
  gfx::NativeView GetNativeView() const override;
  gfx::NativeView GetContentNativeView() const override;
  gfx::NativeWindow GetTopLevelNativeWindow() const override;
  gfx::Rect GetContainerBounds() const override;
  void Focus() override;
  void SetInitialFocus() override;
  void StoreFocus() override;
  void RestoreFocus() override;
  void FocusThroughTabTraversal(bool reverse) override;
  DropData* GetDropData() const override;
  gfx::Rect GetViewBounds() const override;
  void CreateView(gfx::NativeView context) override;
  RenderWidgetHostViewBase* CreateViewForWidget(
      RenderWidgetHost* render_widget_host) override;
  RenderWidgetHostViewBase* CreateViewForChildWidget(
      RenderWidgetHost* render_widget_host) override;
  void SetPageTitle(const std::u16string& title) override;
  void RenderViewReady() override;
  void RenderViewHostChanged(RenderViewHost* old_host,
                             RenderViewHost* new_host) override;
  void SetOverscrollControllerEnabled(bool enabled) override;
#if defined(OS_MAC)
  bool CloseTabAfterEventTrackingIfNeeded() override;
#endif
  void OnCapturerCountChanged() override;

  // Backend implementation of RenderViewHostDelegateView.
  void ShowContextMenu(RenderFrameHost& render_frame_host,
                       const ContextMenuParams& params) override;
  void StartDragging(const DropData& drop_data,
                     blink::DragOperationsMask allowed_ops,
                     const gfx::ImageSkia& image,
                     const gfx::Vector2d& image_offset,
                     const blink::mojom::DragEventSourceInfo& event_info,
                     RenderWidgetHostImpl* source_rwh) override;
  void UpdateDragCursor(ui::mojom::DragOperation operation) override;
  void GotFocus(RenderWidgetHostImpl* render_widget_host) override;
  void TakeFocus(bool reverse) override;

  static RenderWidgetHostViewChildFrame*
  CreateRenderWidgetHostViewForInnerFrameTree(
      WebContentsImpl* web_contents,
      RenderWidgetHost* render_widget_host);

 private:
  WebContentsView* GetOuterView();
  const WebContentsView* GetOuterView() const;

  RenderViewHostDelegateView* GetOuterDelegateView();

  // The WebContentsImpl whose contents we display.
  raw_ptr<WebContentsImpl> web_contents_;

  // The delegate ownership is passed to WebContentsView.
  std::unique_ptr<WebContentsViewDelegate> delegate_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_CHILD_FRAME_H_
