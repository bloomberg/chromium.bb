// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_CLIENT_ANDROID_H_
#define CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_CLIENT_ANDROID_H_

#include "base/macros.h"
#include "content/common/content_export.h"
#include "ui/android/delegated_frame_host_android.h"

namespace content {

class RenderWidgetHostViewAndroid;

class CONTENT_EXPORT DelegatedFrameHostClientAndroid
    : public ui::DelegatedFrameHostAndroid::Client {
 public:
  explicit DelegatedFrameHostClientAndroid(
      RenderWidgetHostViewAndroid* render_widget_host_view);
  ~DelegatedFrameHostClientAndroid() override;

 private:
  // DelegatedFrameHostAndroid::Client implementation.
  void SetBeginFrameSource(viz::BeginFrameSource* begin_frame_source) override;
  void DidPresentCompositorFrames(
      const base::flat_map<uint32_t, gfx::PresentationFeedback>& feedbacks)
      override;
  void DidReceiveCompositorFrameAck(
      const std::vector<viz::ReturnedResource>& resources) override;
  void ReclaimResources(
      const std::vector<viz::ReturnedResource>& resources) override;
  void OnFrameTokenChanged(uint32_t frame_token) override;
  void WasEvicted() override;

  RenderWidgetHostViewAndroid* render_widget_host_view_;

  DISALLOW_COPY_AND_ASSIGN(DelegatedFrameHostClientAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DELEGATED_FRAME_HOST_CLIENT_ANDROID_H_
