// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_AW_RENDER_VIEW_EXT_H_
#define ANDROID_WEBVIEW_RENDERER_AW_RENDER_VIEW_EXT_H_

#include "base/timer/timer.h"
#include "content/public/renderer/render_view_observer.h"
#include "ui/gfx/geometry/size.h"

namespace android_webview {

// NOTE: We should not add more things to RenderView and related classes.
//       RenderView is deprecated in content, since it is not compatible
//       with site isolation/out of process iframes.

// Render process side of AwRenderViewHostExt, this provides cross-process
// implementation of miscellaneous WebView functions that we need to poke
// WebKit directly to implement (and that aren't needed in the chrome app).
class AwRenderViewExt : public content::RenderViewObserver {
 public:
  static void RenderViewCreated(content::RenderView* render_view);

 private:
  AwRenderViewExt(content::RenderView* render_view);
  ~AwRenderViewExt() override;

  // RenderViewObserver:
  void DidCommitCompositorFrame() override;
  void DidUpdateMainFrameLayout() override;
  void OnDestruct() override;

  void UpdateContentsSize();

  gfx::Size last_sent_contents_size_;

  // Whether the contents size may have changed and |UpdateContentsSize| needs
  // to be called.
  bool needs_contents_size_update_ = true;

  DISALLOW_COPY_AND_ASSIGN(AwRenderViewExt);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_AW_RENDER_VIEW_EXT_H_
