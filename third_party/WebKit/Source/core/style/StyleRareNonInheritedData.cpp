/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "core/style/StyleRareNonInheritedData.h"

#include "core/animation/css/CSSAnimationData.h"
#include "core/animation/css/CSSTransitionData.h"
#include "core/style/ComputedStyle.h"
#include "core/style/ContentData.h"
#include "core/style/DataEquivalency.h"
#include "core/style/MemberCopy.h"
#include "core/style/ShadowList.h"
#include "core/style/StyleFilterData.h"
#include "core/style/StyleNonInheritedVariables.h"
#include "core/style/StyleTransformData.h"

namespace blink {

class SameSizeStyleRareNonInheritedData
    : public RefCounted<StyleRareNonInheritedData> {
 public:
  float floats[3];
  int integers;

  LengthPoint length_points[2];
  LineClampValue line_clamps;
  DraggableRegionMode draggable_regions;

  void* data_refs[8];
  DataPersistent<void*> data_persistents[2];
  void* own_ptrs[4];
  Persistent<void*> persistent_handles[2];
  void* ref_ptrs[3];
  void* unique_ptrs[1];

  FillLayer fill_layers;
  NinePieceImage nine_pieces;
  FloatSize float_size;
  Length lengths;
  OutlineValue outline;

  StyleColor style_colors[8];

  Vector<String> callback_selectors_;

  StyleContentAlignmentData content_alignment_data[2];
  StyleSelfAlignmentData self_alignment_data[4];

  unsigned bit_fields_[2];
};

static_assert(sizeof(StyleRareNonInheritedData) ==
                  sizeof(SameSizeStyleRareNonInheritedData),
              "StyleRareNonInheritedData_should_stay_small");

StyleRareNonInheritedData::StyleRareNonInheritedData()
    : opacity_(ComputedStyle::InitialOpacity()),
      perspective_(ComputedStyle::InitialPerspective()),
      shape_image_threshold_(ComputedStyle::InitialShapeImageThreshold()),
      order_(ComputedStyle::InitialOrder()),
      perspective_origin_(ComputedStyle::InitialPerspectiveOrigin()),
      object_position_(ComputedStyle::InitialObjectPosition()),
      line_clamp_(ComputedStyle::InitialLineClamp()),
      draggable_region_mode_(static_cast<unsigned>(kDraggableRegionNone)),
      shape_outside_(ComputedStyle::InitialShapeOutside()),
      clip_path_(ComputedStyle::InitialClipPath()),
      mask_(kMaskFillLayer, true),
      mask_box_image_(NinePieceImage::MaskDefaults()),
      page_size_(),
      shape_margin_(ComputedStyle::InitialShapeMargin()),
      text_decoration_color_(StyleColor::CurrentColor()),
      visited_link_text_decoration_color_(StyleColor::CurrentColor()),
      visited_link_background_color_(ComputedStyle::InitialBackgroundColor()),
      visited_link_outline_color_(StyleColor::CurrentColor()),
      visited_link_border_left_color_(StyleColor::CurrentColor()),
      visited_link_border_right_color_(StyleColor::CurrentColor()),
      visited_link_border_top_color_(StyleColor::CurrentColor()),
      visited_link_border_bottom_color_(StyleColor::CurrentColor()),
      non_inherited_variables_(ComputedStyle::InitialNonInheritedVariables()),
      align_content_(ComputedStyle::InitialContentAlignment()),
      align_items_(ComputedStyle::InitialDefaultAlignment()),
      align_self_(ComputedStyle::InitialSelfAlignment()),
      justify_content_(ComputedStyle::InitialContentAlignment()),
      justify_items_(ComputedStyle::InitialSelfAlignment()),
      justify_self_(ComputedStyle::InitialSelfAlignment()),
      page_size_type_(static_cast<unsigned>(PageSizeType::kAuto)),
      transform_style_3d_(ComputedStyle::InitialTransformStyle3D()),
      backface_visibility_(ComputedStyle::InitialBackfaceVisibility()),
      user_drag_(ComputedStyle::InitialUserDrag()),
      text_overflow_(ComputedStyle::InitialTextOverflow()),
      margin_before_collapse_(kMarginCollapseCollapse),
      margin_after_collapse_(kMarginCollapseCollapse),
      appearance_(ComputedStyle::InitialAppearance()),
      text_decoration_style_(ComputedStyle::InitialTextDecorationStyle()),
      has_current_opacity_animation_(false),
      has_current_transform_animation_(false),
      has_current_filter_animation_(false),
      has_current_backdrop_filter_animation_(false),
      running_opacity_animation_on_compositor_(false),
      running_transform_animation_on_compositor_(false),
      running_filter_animation_on_compositor_(false),
      running_backdrop_filter_animation_on_compositor_(false),
      is_stacking_context_(false),
      effective_blend_mode_(ComputedStyle::InitialBlendMode()),
      touch_action_(ComputedStyle::InitialTouchAction()),
      object_fit_(ComputedStyle::InitialObjectFit()),
      isolation_(ComputedStyle::InitialIsolation()),
      contain_(ComputedStyle::InitialContain()),
      scroll_behavior_(ComputedStyle::InitialScrollBehavior()),
      scroll_snap_type_(ComputedStyle::InitialScrollSnapType()),
      requires_accelerated_compositing_for_external_reasons_(false),
      has_inline_transform_(false),
      resize_(ComputedStyle::InitialResize()),
      has_compositor_proxy_(false),
      has_author_background_(false),
      has_author_border_(false) {}

StyleRareNonInheritedData::StyleRareNonInheritedData(
    const StyleRareNonInheritedData& o)
    : RefCounted<StyleRareNonInheritedData>(),
      opacity_(o.opacity_),
      perspective_(o.perspective_),
      shape_image_threshold_(o.shape_image_threshold_),
      order_(o.order_),
      perspective_origin_(o.perspective_origin_),
      object_position_(o.object_position_),
      line_clamp_(o.line_clamp_),
      draggable_region_mode_(o.draggable_region_mode_),
      deprecated_flexible_box_data_(
          MemberCopy(o.deprecated_flexible_box_data_)),
      flexible_box_data_(MemberCopy(o.flexible_box_data_)),
      multi_col_data_(MemberCopy(o.multi_col_data_)),
      transform_data_(MemberCopy(o.transform_data_)),
      will_change_data_(MemberCopy(o.will_change_data_)),
      filter_(MemberCopy(o.filter_)),
      backdrop_filter_(MemberCopy(o.backdrop_filter_)),
      grid_data_(MemberCopy(o.grid_data_)),
      grid_item_data_(MemberCopy(o.grid_item_data_)),
      scroll_snap_data_(MemberCopy(o.scroll_snap_data_)),
      content_(MemberCopy(o.content_)),
      counter_directives_(MemberCopy(o.counter_directives_)),
      animations_(MemberCopy(o.animations_)),
      transitions_(MemberCopy(o.transitions_)),
      box_shadow_(MemberCopy(o.box_shadow_)),
      box_reflect_(MemberCopy(o.box_reflect_)),
      shape_outside_(o.shape_outside_),
      clip_path_(MemberCopy(o.clip_path_)),
      mask_(o.mask_),
      mask_box_image_(o.mask_box_image_),
      page_size_(o.page_size_),
      shape_margin_(o.shape_margin_),
      outline_(o.outline_),
      text_decoration_color_(o.text_decoration_color_),
      visited_link_text_decoration_color_(
          o.visited_link_text_decoration_color_),
      visited_link_background_color_(o.visited_link_background_color_),
      visited_link_outline_color_(o.visited_link_outline_color_),
      visited_link_border_left_color_(o.visited_link_border_left_color_),
      visited_link_border_right_color_(o.visited_link_border_right_color_),
      visited_link_border_top_color_(o.visited_link_border_top_color_),
      visited_link_border_bottom_color_(o.visited_link_border_bottom_color_),
      callback_selectors_(o.callback_selectors_),
      paint_images_(MemberCopy(o.paint_images_)),
      non_inherited_variables_(MemberCopy(o.non_inherited_variables_)),
      align_content_(o.align_content_),
      align_items_(o.align_items_),
      align_self_(o.align_self_),
      justify_content_(o.justify_content_),
      justify_items_(o.justify_items_),
      justify_self_(o.justify_self_),
      page_size_type_(o.page_size_type_),
      transform_style_3d_(o.transform_style_3d_),
      backface_visibility_(o.backface_visibility_),
      user_drag_(o.user_drag_),
      text_overflow_(o.text_overflow_),
      margin_before_collapse_(o.margin_before_collapse_),
      margin_after_collapse_(o.margin_after_collapse_),
      appearance_(o.appearance_),
      text_decoration_style_(o.text_decoration_style_),
      has_current_opacity_animation_(o.has_current_opacity_animation_),
      has_current_transform_animation_(o.has_current_transform_animation_),
      has_current_filter_animation_(o.has_current_filter_animation_),
      has_current_backdrop_filter_animation_(
          o.has_current_backdrop_filter_animation_),
      running_opacity_animation_on_compositor_(
          o.running_opacity_animation_on_compositor_),
      running_transform_animation_on_compositor_(
          o.running_transform_animation_on_compositor_),
      running_filter_animation_on_compositor_(
          o.running_filter_animation_on_compositor_),
      running_backdrop_filter_animation_on_compositor_(
          o.running_backdrop_filter_animation_on_compositor_),
      is_stacking_context_(o.is_stacking_context_),
      effective_blend_mode_(o.effective_blend_mode_),
      touch_action_(o.touch_action_),
      object_fit_(o.object_fit_),
      isolation_(o.isolation_),
      contain_(o.contain_),
      scroll_behavior_(o.scroll_behavior_),
      scroll_snap_type_(o.scroll_snap_type_),
      requires_accelerated_compositing_for_external_reasons_(
          o.requires_accelerated_compositing_for_external_reasons_),
      has_inline_transform_(o.has_inline_transform_),
      resize_(o.resize_),
      has_compositor_proxy_(o.has_compositor_proxy_),
      has_author_background_(o.has_author_background_),
      has_author_border_(o.has_author_border_) {}

StyleRareNonInheritedData::~StyleRareNonInheritedData() {}

bool StyleRareNonInheritedData::operator==(
    const StyleRareNonInheritedData& o) const {
  return opacity_ == o.opacity_ && perspective_ == o.perspective_ &&
         shape_image_threshold_ == o.shape_image_threshold_ &&
         order_ == o.order_ && perspective_origin_ == o.perspective_origin_ &&
         object_position_ == o.object_position_ &&
         line_clamp_ == o.line_clamp_ &&
         draggable_region_mode_ == o.draggable_region_mode_ &&
         DataEquivalent(deprecated_flexible_box_data_,
                        o.deprecated_flexible_box_data_) &&
         DataEquivalent(flexible_box_data_, o.flexible_box_data_) &&
         DataEquivalent(multi_col_data_, o.multi_col_data_) &&
         DataEquivalent(transform_data_, o.transform_data_) &&
         DataEquivalent(will_change_data_, o.will_change_data_) &&
         DataEquivalent(filter_, o.filter_) &&
         DataEquivalent(backdrop_filter_, o.backdrop_filter_) &&
         DataEquivalent(grid_data_, o.grid_data_) &&
         DataEquivalent(grid_item_data_, o.grid_item_data_) &&
         DataEquivalent(scroll_snap_data_, o.scroll_snap_data_) &&
         DataEquivalent(content_, o.content_) &&
         DataEquivalent(counter_directives_, o.counter_directives_) &&
         DataEquivalent(box_shadow_, o.box_shadow_) &&
         DataEquivalent(box_reflect_, o.box_reflect_) &&
         DataEquivalent(animations_, o.animations_) &&
         DataEquivalent(transitions_, o.transitions_) &&
         DataEquivalent(shape_outside_, o.shape_outside_) && mask_ == o.mask_ &&
         mask_box_image_ == o.mask_box_image_ && page_size_ == o.page_size_ &&
         shape_margin_ == o.shape_margin_ && outline_ == o.outline_ &&
         DataEquivalent(clip_path_, o.clip_path_) &&
         text_decoration_color_ == o.text_decoration_color_ &&
         visited_link_text_decoration_color_ ==
             o.visited_link_text_decoration_color_ &&
         visited_link_background_color_ == o.visited_link_background_color_ &&
         visited_link_outline_color_ == o.visited_link_outline_color_ &&
         visited_link_border_left_color_ == o.visited_link_border_left_color_ &&
         visited_link_border_right_color_ ==
             o.visited_link_border_right_color_ &&
         visited_link_border_top_color_ == o.visited_link_border_top_color_ &&
         visited_link_border_bottom_color_ ==
             o.visited_link_border_bottom_color_ &&
         callback_selectors_ == o.callback_selectors_ &&
         DataEquivalent(non_inherited_variables_, o.non_inherited_variables_) &&
         align_content_ == o.align_content_ && align_items_ == o.align_items_ &&
         align_self_ == o.align_self_ &&
         justify_content_ == o.justify_content_ &&
         justify_items_ == o.justify_items_ &&
         justify_self_ == o.justify_self_ &&
         page_size_type_ == o.page_size_type_ &&
         transform_style_3d_ == o.transform_style_3d_ &&
         backface_visibility_ == o.backface_visibility_ &&
         user_drag_ == o.user_drag_ && text_overflow_ == o.text_overflow_ &&
         margin_before_collapse_ == o.margin_before_collapse_ &&
         margin_after_collapse_ == o.margin_after_collapse_ &&
         appearance_ == o.appearance_ &&
         text_decoration_style_ == o.text_decoration_style_ &&
         has_current_opacity_animation_ == o.has_current_opacity_animation_ &&
         has_current_transform_animation_ ==
             o.has_current_transform_animation_ &&
         has_current_filter_animation_ == o.has_current_filter_animation_ &&
         has_current_backdrop_filter_animation_ ==
             o.has_current_backdrop_filter_animation_ &&
         is_stacking_context_ == o.is_stacking_context_ &&
         effective_blend_mode_ == o.effective_blend_mode_ &&
         touch_action_ == o.touch_action_ && object_fit_ == o.object_fit_ &&
         isolation_ == o.isolation_ && contain_ == o.contain_ &&
         scroll_behavior_ == o.scroll_behavior_ &&
         scroll_snap_type_ == o.scroll_snap_type_ &&
         requires_accelerated_compositing_for_external_reasons_ ==
             o.requires_accelerated_compositing_for_external_reasons_ &&
         has_inline_transform_ == o.has_inline_transform_ &&
         resize_ == o.resize_ &&
         has_compositor_proxy_ == o.has_compositor_proxy_ &&
         has_author_background_ == o.has_author_background_ &&
         has_author_border_ == o.has_author_border_;
}

}  // namespace blink
