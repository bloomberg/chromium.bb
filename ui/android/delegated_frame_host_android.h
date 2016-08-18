// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_DELEGATED_FRAME_HOST_ANDROID_H_
#define UI_ANDROID_DELEGATED_FRAME_HOST_ANDROID_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cc/output/copy_output_request.h"
#include "cc/resources/returned_resource.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "ui/android/ui_android_export.h"
#include "ui/gfx/selection_bound.h"

namespace cc {

class CompositorFrame;
class CopyOutputResult;
class Layer;
class SurfaceId;
class SurfaceManager;
class SurfaceLayer;
class SurfaceIdAllocator;
enum class SurfaceDrawStatus;

}  // namespace cc

namespace ui {
class ViewAndroid;
class WindowAndroidCompositor;

class UI_ANDROID_EXPORT DelegatedFrameHostAndroid
    : public cc::SurfaceFactoryClient {
 public:
  using ReturnResourcesCallback =
      base::Callback<void(const cc::ReturnedResourceArray&)>;

  DelegatedFrameHostAndroid(ViewAndroid* view,
                            SkColor background_color,
                            ReturnResourcesCallback return_resources_callback);

  ~DelegatedFrameHostAndroid() override;

  void SubmitCompositorFrame(cc::CompositorFrame frame,
                             cc::SurfaceFactory::DrawCallback draw_callback);

  void DestroyDelegatedContent();

  bool HasDelegatedContent() const;

  uint32_t GetSurfaceClientId() const;

  // Should only be called when the host has a content layer.
  void RequestCopyOfSurface(
      WindowAndroidCompositor* compositor,
      const gfx::Rect& src_subrect_in_pixel,
      cc::CopyOutputRequest::CopyOutputRequestCallback result_callback);

  void OutputSurfaceChanged();

  void UpdateBackgroundColor(SkColor color);

  void SetContentsOpaque(bool opaque);

  void UpdateContainerSizeinDIP(const gfx::Size& size_in_dip);

 private:
  // cc::SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;
  void SetBeginFrameSource(cc::BeginFrameSource* begin_frame_source) override;

  void UpdateBackgroundLayer();

  ViewAndroid* view_;

  cc::SurfaceManager* surface_manager_;
  std::unique_ptr<cc::SurfaceIdAllocator> surface_id_allocator_;
  ReturnResourcesCallback return_resources_callback_;

  std::unique_ptr<cc::SurfaceFactory> surface_factory_;

  struct FrameData {
    FrameData();
    ~FrameData();

    cc::SurfaceId surface_id;
    gfx::Size surface_size;
    float top_controls_height;
    float top_controls_shown_ratio;
    float bottom_controls_height;
    float bottom_controls_shown_ratio;
    cc::Selection<gfx::SelectionBound> viewport_selection;
  };
  std::unique_ptr<FrameData> current_frame_;

  scoped_refptr<cc::SurfaceLayer> content_layer_;

  scoped_refptr<cc::Layer> background_layer_;

  gfx::Size container_size_in_dip_;

  DISALLOW_COPY_AND_ASSIGN(DelegatedFrameHostAndroid);
};

}  // namespace ui

#endif  // UI_ANDROID_DELEGATED_FRAME_HOST_ANDROID_H_
