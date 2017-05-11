/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#ifndef StyleRareNonInheritedData_h
#define StyleRareNonInheritedData_h

#include <memory>
#include "core/CoreExport.h"
#include "core/css/StyleColor.h"
#include "core/style/ClipPathOperation.h"
#include "core/style/ComputedStyleConstants.h"
#include "core/style/CounterDirectives.h"
#include "core/style/DataPersistent.h"
#include "core/style/DataRef.h"
#include "core/style/FillLayer.h"
#include "core/style/LineClampValue.h"
#include "core/style/NinePieceImage.h"
#include "core/style/OutlineValue.h"
#include "core/style/ShapeValue.h"
#include "core/style/StyleContentAlignmentData.h"
#include "core/style/StyleScrollSnapData.h"
#include "core/style/StyleSelfAlignmentData.h"
#include "platform/LengthPoint.h"
#include "platform/graphics/TouchAction.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/Vector.h"

namespace blink {

class ContentData;
class CSSAnimationData;
class CSSTransitionData;
class ShadowList;
class StyleDeprecatedFlexibleBoxData;
class StyleFilterData;
class StyleFlexibleBoxData;
class StyleGridData;
class StyleGridItemData;
class StyleMultiColData;
class StyleNonInheritedVariables;
class StyleReflection;
class StyleTransformData;
class StyleWillChangeData;

// Page size type.
// StyleRareNonInheritedData::page_size_ is meaningful only when
// StyleRareNonInheritedData::page_size_type_ is PAGE_SIZE_RESOLVED.
enum PageSizeType {
  PAGE_SIZE_AUTO,            // size: auto
  PAGE_SIZE_AUTO_LANDSCAPE,  // size: landscape
  PAGE_SIZE_AUTO_PORTRAIT,   // size: portrait
  PAGE_SIZE_RESOLVED         // Size is fully resolved.
};

// This struct is for rarely used non-inherited CSS3, CSS2, and WebKit-specific
// properties.  By grouping them together, we save space, and only allocate this
// object when someone actually uses one of these properties.
// TODO(sashab): Move this into a private class on ComputedStyle, and remove
// all methods on it, merging them into copy/creation methods on ComputedStyle
// instead. Keep the allocation logic, only allocating a new object if needed.
class CORE_EXPORT StyleRareNonInheritedData
    : public RefCounted<StyleRareNonInheritedData> {
 public:
  static PassRefPtr<StyleRareNonInheritedData> Create() {
    return AdoptRef(new StyleRareNonInheritedData);
  }
  PassRefPtr<StyleRareNonInheritedData> Copy() const {
    return AdoptRef(new StyleRareNonInheritedData(*this));
  }
  ~StyleRareNonInheritedData();

  bool operator==(const StyleRareNonInheritedData&) const;
  bool operator!=(const StyleRareNonInheritedData& o) const {
    return !(*this == o);
  }

  bool ContentDataEquivalent(const StyleRareNonInheritedData&) const;
  bool CounterDataEquivalent(const StyleRareNonInheritedData&) const;
  bool ShadowDataEquivalent(const StyleRareNonInheritedData&) const;
  bool ReflectionDataEquivalent(const StyleRareNonInheritedData&) const;
  bool AnimationDataEquivalent(const StyleRareNonInheritedData&) const;
  bool TransitionDataEquivalent(const StyleRareNonInheritedData&) const;
  bool ShapeOutsideDataEquivalent(const StyleRareNonInheritedData&) const;
  bool ClipPathDataEquivalent(const StyleRareNonInheritedData&) const;
  bool HasFilters() const;
  bool HasBackdropFilters() const;
  bool HasOpacity() const { return opacity < 1; }

  float opacity;  // Whether or not we're transparent.

  float perspective_;
  float shape_image_threshold_;

  int order_;

  LengthPoint perspective_origin_;
  LengthPoint object_position_;

  LineClampValue line_clamp;  // An Apple extension.
  DraggableRegionMode draggable_region_mode_;

  DataRef<StyleDeprecatedFlexibleBoxData>
      deprecated_flexible_box_;  // Flexible box properties
  DataRef<StyleFlexibleBoxData> flexible_box_;
  DataRef<StyleMultiColData> multi_col_;  //  CSS3 multicol properties
  DataRef<StyleTransformData>
      transform_;  // Transform properties (rotate, scale, skew, etc.)
  DataRef<StyleWillChangeData> will_change_;  // CSS Will Change

  DataPersistent<StyleFilterData>
      filter_;  // Filter operations (url, sepia, blur, etc.)
  DataPersistent<StyleFilterData>
      backdrop_filter_;  // Backdrop filter operations (url, sepia, blur, etc.)

  DataRef<StyleGridData> grid_;
  DataRef<StyleGridItemData> grid_item_;
  DataRef<StyleScrollSnapData> scroll_snap_;

  Persistent<ContentData> content_;
  std::unique_ptr<CounterDirectiveMap> counter_directives_;
  std::unique_ptr<CSSAnimationData> animations_;
  std::unique_ptr<CSSTransitionData> transitions_;

  RefPtr<ShadowList> box_shadow_;

  RefPtr<StyleReflection> box_reflect_;

  Persistent<ShapeValue> shape_outside_;
  RefPtr<ClipPathOperation> clip_path_;

  FillLayer mask_;
  NinePieceImage mask_box_image_;

  FloatSize page_size_;
  Length shape_margin_;

  OutlineValue outline_;

  StyleColor text_decoration_color_;
  StyleColor visited_link_text_decoration_color_;
  StyleColor visited_link_background_color_;
  StyleColor visited_link_outline_color_;
  StyleColor visited_link_border_left_color_;
  StyleColor visited_link_border_right_color_;
  StyleColor visited_link_border_top_color_;
  StyleColor visited_link_border_bottom_color_;

  Vector<String> callback_selectors_;

  std::unique_ptr<Vector<Persistent<StyleImage>>> paint_images_;

  std::unique_ptr<StyleNonInheritedVariables> variables_;

  StyleContentAlignmentData align_content_;
  StyleSelfAlignmentData align_items_;
  StyleSelfAlignmentData align_self_;
  StyleContentAlignmentData justify_content_;
  StyleSelfAlignmentData justify_items_;
  StyleSelfAlignmentData justify_self_;

  unsigned page_size_type_ : 2;       // PageSizeType
  unsigned transform_style3d_ : 1;    // ETransformStyle3D
  unsigned backface_visibility_ : 1;  // EBackfaceVisibility

  unsigned user_drag : 2;      // EUserDrag
  unsigned text_overflow : 1;  // Whether or not lines that spill out should be
                               // truncated with "..."
  unsigned margin_before_collapse : 2;  // EMarginCollapse
  unsigned margin_after_collapse : 2;   // EMarginCollapse
  unsigned appearance_ : 6;             // EAppearance

  unsigned text_decoration_style_ : 3;  // TextDecorationStyle

  unsigned has_current_opacity_animation_ : 1;
  unsigned has_current_transform_animation_ : 1;
  unsigned has_current_filter_animation_ : 1;
  unsigned has_current_backdrop_filter_animation_ : 1;
  unsigned running_opacity_animation_on_compositor_ : 1;
  unsigned running_transform_animation_on_compositor_ : 1;
  unsigned running_filter_animation_on_compositor_ : 1;
  unsigned running_backdrop_filter_animation_on_compositor_ : 1;

  unsigned is_stacking_context_ : 1;

  unsigned effective_blend_mode_ : 5;  // EBlendMode

  unsigned touch_action_ : kTouchActionBits;  // TouchAction

  unsigned object_fit_ : 3;  // ObjectFit

  unsigned isolation_ : 1;  // Isolation

  unsigned contain_ : 4;  // Containment

  // ScrollBehavior. 'scroll-behavior' has 2 accepted values, but ScrollBehavior
  // has a third value (that can only be specified using CSSOM scroll APIs) so 2
  // bits are needed.
  unsigned scroll_behavior_ : 2;

  unsigned scroll_snap_type_ : 2;  // ScrollSnapType

  // Plugins require accelerated compositing for reasons external to blink.
  // In which case, we need to update the ComputedStyle on the
  // LayoutEmbeddedObject, so store this bit so that the style actually changes
  // when the plugin becomes composited.
  unsigned requires_accelerated_compositing_for_external_reasons_ : 1;

  // Whether the transform (if it exists) is stored in the element's inline
  // style.
  unsigned has_inline_transform_ : 1;
  unsigned resize_ : 2;  // EResize
  unsigned has_compositor_proxy_ : 1;

  // Style adjustment for appearance is disabled when certain properties are
  // set.
  unsigned has_author_background_ : 1;  // Whether there is a author-defined
                                        // background.
  unsigned has_author_border_ : 1;  // Whether there is a author-defined border.

 private:
  StyleRareNonInheritedData();
  StyleRareNonInheritedData(const StyleRareNonInheritedData&);
};

}  // namespace blink

#endif  // StyleRareNonInheritedData_h
