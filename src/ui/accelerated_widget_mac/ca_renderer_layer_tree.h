// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCELERATED_WIDGET_MAC_CA_RENDERER_LAYER_TREE_H_
#define UI_ACCELERATED_WIDGET_MAC_CA_RENDERER_LAYER_TREE_H_

#include <IOSurface/IOSurface.h>
#include <QuartzCore/QuartzCore.h>

#include <memory>
#include <vector>

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "ui/accelerated_widget_mac/accelerated_widget_mac_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/mac/io_surface.h"
#include "ui/gfx/video_types.h"

@class AVSampleBufferDisplayLayer;

namespace ui {

struct CARendererLayerParams;

enum class CALayerType {
  // A CALayer with contents set to an IOSurface by setContents.
  kDefault,
  // An AVSampleBufferDisplayLayer.
  kVideo,
  // A CAMetalLayer that copies half-float or 10-bit IOSurfaces.
  kHDRCopier,
};

// The CARendererLayerTree will construct a hierarchy of CALayers from a linear
// list provided by the CoreAnimation renderer using the algorithm and structure
// referenced described in
// https://docs.google.com/document/d/1DtSN9zzvCF44_FQPM7ie01UxGHagQ66zfF5L9HnigQY/edit?usp=sharing
class ACCELERATED_WIDGET_MAC_EXPORT CARendererLayerTree {
 public:
  CARendererLayerTree(bool allow_av_sample_buffer_display_layer,
                      bool allow_solid_color_layers);

  CARendererLayerTree(const CARendererLayerTree&) = delete;
  CARendererLayerTree& operator=(const CARendererLayerTree&) = delete;

  // This will remove all CALayers from this tree from their superlayer.
  ~CARendererLayerTree();

  // Append the description of a new CALayer to the tree. This will not
  // create any new CALayers until CommitScheduledCALayers is called. This
  // cannot be called anymore after CommitScheduledCALayers has been called.
  bool ScheduleCALayer(const CARendererLayerParams& params);

  // Create a CALayer tree for the scheduled layers, and set |superlayer| to
  // have only this tree as its sublayers. If |old_tree| is non-null, then try
  // to re-use the CALayers of |old_tree| as much as possible. |old_tree| will
  // be destroyed at the end of the function, and any CALayers in it which were
  // not re-used by |this| will be removed from the CALayer hierarchy.
  void CommitScheduledCALayers(CALayer* superlayer,
                               std::unique_ptr<CARendererLayerTree> old_tree,
                               const gfx::Size& pixel_size,
                               float scale_factor);

  // Returns the contents used for a given solid color.
  id ContentsForSolidColorForTesting(unsigned int color);

  // If there exists only a single content layer, return the IOSurface of that
  // layer.
  IOSurfaceRef GetContentIOSurface() const;

 private:
  class SolidColorContents;
  class RootLayer;
  class ClipAndSortingLayer;
  class TransformLayer;
  class ContentLayer;
  friend class ContentLayer;

  class RootLayer {
   public:
    RootLayer();

    RootLayer(const RootLayer&) = delete;
    RootLayer& operator=(const RootLayer&) = delete;

    // This will remove |ca_layer| from its superlayer, if |ca_layer| is
    // non-nil.
    ~RootLayer();

    // Append a new content layer, without modifying the actual CALayer
    // structure.
    bool AddContentLayer(CARendererLayerTree* tree,
                         const CARendererLayerParams& params);

    // Downgrade all downgradeable AVSampleBufferDisplayLayers to be normal
    // CALayers.
    // https://crbug.com/923427, https://crbug.com/1143477
    void DowngradeAVLayersToCALayers();

    // Allocate CALayers for this layer and its children, and set their
    // properties appropriately. Re-use the CALayers from |old_layer| if
    // possible. If re-using a CALayer from |old_layer|, reset its |ca_layer|
    // to nil, so that its destructor will not remove an active CALayer.
    void CommitToCA(CALayer* superlayer,
                    RootLayer* old_layer,
                    const gfx::Size& pixel_size,
                    float scale_factor);

    // Return true if the CALayer tree is just a video layer on a black or
    // transparent background, false otherwise.
    bool WantsFullcreenLowPowerBackdrop() const;

    std::vector<ClipAndSortingLayer> clip_and_sorting_layers_;
    base::scoped_nsobject<CALayer> ca_layer_;
  };
  class ClipAndSortingLayer {
   public:
    ClipAndSortingLayer(bool is_clipped,
                        gfx::Rect clip_rect,
                        gfx::RRectF rounded_corner_bounds,
                        unsigned sorting_context_id,
                        bool is_singleton_sorting_context);
    ClipAndSortingLayer(ClipAndSortingLayer&& layer);

    ClipAndSortingLayer(const ClipAndSortingLayer&) = delete;
    ClipAndSortingLayer& operator=(const ClipAndSortingLayer&) = delete;

    // See the behavior of RootLayer for the effects of these functions on the
    // |ca_layer| member and |old_layer| argument.
    ~ClipAndSortingLayer();
    void AddContentLayer(CARendererLayerTree* tree,
                         const CARendererLayerParams& params);
    void CommitToCA(CALayer* superlayer,
                    ClipAndSortingLayer* old_layer,
                    float scale_factor);

    std::vector<TransformLayer> transform_layers_;
    bool is_clipped_ = false;
    gfx::Rect clip_rect_;
    gfx::RRectF rounded_corner_bounds_;
    unsigned sorting_context_id_ = 0;
    bool is_singleton_sorting_context_ = false;
    base::scoped_nsobject<CALayer> clipping_ca_layer_;
    base::scoped_nsobject<CALayer> rounded_corner_ca_layer_;
  };
  class TransformLayer {
   public:
    TransformLayer(const gfx::Transform& transform);
    TransformLayer(TransformLayer&& layer);

    TransformLayer(const TransformLayer&) = delete;
    TransformLayer& operator=(const TransformLayer&) = delete;

    // See the behavior of RootLayer for the effects of these functions on the
    // |ca_layer| member and |old_layer| argument.
    ~TransformLayer();
    void AddContentLayer(CARendererLayerTree* tree,
                         const CARendererLayerParams& params);
    void CommitToCA(CALayer* superlayer,
                    TransformLayer* old_layer,
                    float scale_factor);

    gfx::Transform transform_;
    std::vector<ContentLayer> content_layers_;
    base::scoped_nsobject<CALayer> ca_layer_;
  };
  class ContentLayer {
   public:
    ContentLayer(CARendererLayerTree* tree,
                 base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
                 base::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer,
                 const gfx::RectF& contents_rect,
                 const gfx::Rect& rect,
                 unsigned background_color,
                 const gfx::ColorSpace& color_space,
                 unsigned edge_aa_mask,
                 float opacity,
                 unsigned filter,
                 gfx::ProtectedVideoType protected_video_type);
    ContentLayer(ContentLayer&& layer);

    ContentLayer(const ContentLayer&) = delete;
    ContentLayer& operator=(const ContentLayer&) = delete;

    // See the behavior of RootLayer for the effects of these functions on the
    // |ca_layer| member and |old_layer| argument.
    ~ContentLayer();
    void CommitToCA(CALayer* parent,
                    ContentLayer* old_layer,
                    float scale_factor);

    // Ensure that the IOSurface be marked as in-use as soon as it is received.
    // When they are committed to the window server, that will also increment
    // their use count.
    const gfx::ScopedInUseIOSurface io_surface_;
    const base::ScopedCFTypeRef<CVPixelBufferRef> cv_pixel_buffer_;
    scoped_refptr<SolidColorContents> solid_color_contents_;
    gfx::RectF contents_rect_;
    gfx::RectF rect_;
    unsigned background_color_ = 0;
    // The color space of |io_surface|. Used for HDR tonemapping.
    gfx::ColorSpace io_surface_color_space_;
    // Note that the CoreAnimation edge antialiasing mask is not the same as
    // the edge antialiasing mask passed to the constructor.
    CAEdgeAntialiasingMask ca_edge_aa_mask_ = 0;
    float opacity_ = 1;
    NSString* const ca_filter_ = nil;

    CALayerType type_ = CALayerType::kDefault;

    // If |type| is CALayerType::kVideo and |video_type_can_downgrade| then
    // |type| can be downgraded to kDefault. This can be set to false for
    // HDR video (that cannot be displayed by a regular CALayer) or for
    // protected content (see https://crbug.com/1026703).
    bool video_type_can_downgrade_ = true;

    gfx::ProtectedVideoType protected_video_type_ =
        gfx::ProtectedVideoType::kClear;

    base::scoped_nsobject<CALayer> ca_layer_;

    // If this layer's contents can be represented as an
    // AVSampleBufferDisplayLayer, then |ca_layer| will point to |av_layer|.
    base::scoped_nsobject<AVSampleBufferDisplayLayer> av_layer_;

    // Layer used to colorize content when it updates, if borders are
    // enabled.
    base::scoped_nsobject<CALayer> update_indicator_layer_;
  };

  RootLayer root_layer_;
  float scale_factor_ = 1;
  bool has_committed_ = false;
  const bool allow_av_sample_buffer_display_layer_ = true;
  const bool allow_solid_color_layers_ = true;
};

}  // namespace ui

#endif  // UI_ACCELERATED_WIDGET_MAC_CA_RENDERER_LAYER_TREE_H_
