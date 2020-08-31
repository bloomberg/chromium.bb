// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BROWSER_CONTROLS_CONTAINER_VIEW_H_
#define WEBLAYER_BROWSER_BROWSER_CONTROLS_CONTAINER_VIEW_H_

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents_observer.h"

namespace cc {
class UIResourceLayer;
}  // namespace cc

namespace content {
class WebContents;
}

namespace weblayer {

class ContentViewRenderView;

// Native side of BrowserControlsContainerView. Responsible for creating and
// positioning the cc::Layer that contains an image of the contents of the
// top-control.
class BrowserControlsContainerView : public content::WebContentsObserver {
 public:
  BrowserControlsContainerView(const base::android::JavaParamRef<jobject>&
                                   java_browser_controls_container_view,
                               ContentViewRenderView* content_view_render_view,
                               bool is_top);
  ~BrowserControlsContainerView() override;

  // Height needed to display the control.
  int GetControlsHeight();

  // Returns the amount of vertical space to take away from the contents.
  int GetContentHeightDelta();

  bool IsFullyVisible() const;

  // Creates |controls_layer_|.
  void CreateControlsLayer(JNIEnv* env,
                           int id);

  // Deletes |this|.
  void DeleteBrowserControlsContainerView(JNIEnv* env);

  // Deletes |controls_layer_|.
  void DeleteControlsLayer(JNIEnv* env);

  // Sets the offsets of the controls and content. See
  // BrowserControlsContainerView's javadoc for details on this.
  void SetTopControlsOffset(JNIEnv* env,
                            int controls_offset_y,
                            int content_offset_y);
  void SetBottomControlsOffset(JNIEnv* env, int controls_offset_y);

  // Sets the size of |controls_layer_|.
  void SetControlsSize(JNIEnv* env,
                       int width,
                       int height);

  // Triggers updating the resource (bitmap) shown in |controls_layer_|.
  void UpdateControlsResource(JNIEnv* env);

  void SetWebContents(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& web_contents);

 private:
  // WebContentsObserver:
  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override;

  base::android::ScopedJavaGlobalRef<jobject>
      java_browser_controls_container_view_;
  ContentViewRenderView* content_view_render_view_;
  const bool is_top_;
  int controls_resource_id_ = -1;

  // Layer containing showing the image for the controls. This is a sibling of
  // the WebContents layer.
  scoped_refptr<cc::UIResourceLayer> controls_layer_;

  DISALLOW_COPY_AND_ASSIGN(BrowserControlsContainerView);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BROWSER_CONTROLS_CONTAINER_VIEW_H_
