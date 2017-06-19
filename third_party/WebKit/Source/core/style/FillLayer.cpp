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

#include "core/style/FillLayer.h"

#include "core/layout/LayoutObject.h"
#include "core/style/DataEquivalency.h"

namespace blink {

struct SameSizeAsFillLayer {
  FillLayer* next_;

  Persistent<StyleImage> image_;

  Length x_position_;
  Length y_position_;

  LengthSize size_length_;

  unsigned bitfields1_;
  unsigned bitfields2_;
};

static_assert(sizeof(FillLayer) == sizeof(SameSizeAsFillLayer),
              "FillLayer should stay small");

FillLayer::FillLayer(EFillLayerType type, bool use_initial_values)
    : next_(0),
      image_(FillLayer::InitialFillImage(type)),
      x_position_(FillLayer::InitialFillXPosition(type)),
      y_position_(FillLayer::InitialFillYPosition(type)),
      size_length_(FillLayer::InitialFillSizeLength(type)),
      attachment_(FillLayer::InitialFillAttachment(type)),
      clip_(FillLayer::InitialFillClip(type)),
      origin_(FillLayer::InitialFillOrigin(type)),
      repeat_x_(FillLayer::InitialFillRepeatX(type)),
      repeat_y_(FillLayer::InitialFillRepeatY(type)),
      composite_(FillLayer::InitialFillComposite(type)),
      size_type_(use_initial_values ? FillLayer::InitialFillSizeType(type)
                                    : kSizeNone),
      blend_mode_(static_cast<unsigned>(FillLayer::InitialFillBlendMode(type))),
      mask_source_type_(FillLayer::InitialFillMaskSourceType(type)),
      background_x_origin_(kLeftEdge),
      background_y_origin_(kTopEdge),
      image_set_(use_initial_values),
      attachment_set_(use_initial_values),
      clip_set_(use_initial_values),
      origin_set_(use_initial_values),
      repeat_x_set_(use_initial_values),
      repeat_y_set_(use_initial_values),
      x_pos_set_(use_initial_values),
      y_pos_set_(use_initial_values),
      background_x_origin_set_(false),
      background_y_origin_set_(false),
      composite_set_(use_initial_values || type == kMaskFillLayer),
      blend_mode_set_(use_initial_values),
      mask_source_type_set_(use_initial_values),
      type_(type),
      this_or_next_layers_clip_max_(0),
      this_or_next_layers_use_content_box_(0),
      this_or_next_layers_have_local_attachment_(0),
      cached_properties_computed_(false) {}

FillLayer::FillLayer(const FillLayer& o)
    : next_(o.next_ ? new FillLayer(*o.next_) : 0),
      image_(o.image_),
      x_position_(o.x_position_),
      y_position_(o.y_position_),
      size_length_(o.size_length_),
      attachment_(o.attachment_),
      clip_(o.clip_),
      origin_(o.origin_),
      repeat_x_(o.repeat_x_),
      repeat_y_(o.repeat_y_),
      composite_(o.composite_),
      size_type_(o.size_type_),
      blend_mode_(o.blend_mode_),
      mask_source_type_(o.mask_source_type_),
      background_x_origin_(o.background_x_origin_),
      background_y_origin_(o.background_y_origin_),
      image_set_(o.image_set_),
      attachment_set_(o.attachment_set_),
      clip_set_(o.clip_set_),
      origin_set_(o.origin_set_),
      repeat_x_set_(o.repeat_x_set_),
      repeat_y_set_(o.repeat_y_set_),
      x_pos_set_(o.x_pos_set_),
      y_pos_set_(o.y_pos_set_),
      background_x_origin_set_(o.background_x_origin_set_),
      background_y_origin_set_(o.background_y_origin_set_),
      composite_set_(o.composite_set_),
      blend_mode_set_(o.blend_mode_set_),
      mask_source_type_set_(o.mask_source_type_set_),
      type_(o.type_),
      this_or_next_layers_clip_max_(0),
      this_or_next_layers_use_content_box_(0),
      this_or_next_layers_have_local_attachment_(0),
      cached_properties_computed_(false) {}

FillLayer::~FillLayer() {
  delete next_;
}

FillLayer& FillLayer::operator=(const FillLayer& o) {
  if (next_ != o.next_) {
    delete next_;
    next_ = o.next_ ? new FillLayer(*o.next_) : 0;
  }

  image_ = o.image_;
  x_position_ = o.x_position_;
  y_position_ = o.y_position_;
  background_x_origin_ = o.background_x_origin_;
  background_y_origin_ = o.background_y_origin_;
  background_x_origin_set_ = o.background_x_origin_set_;
  background_y_origin_set_ = o.background_y_origin_set_;
  size_length_ = o.size_length_;
  attachment_ = o.attachment_;
  clip_ = o.clip_;
  composite_ = o.composite_;
  blend_mode_ = o.blend_mode_;
  origin_ = o.origin_;
  repeat_x_ = o.repeat_x_;
  repeat_y_ = o.repeat_y_;
  size_type_ = o.size_type_;
  mask_source_type_ = o.mask_source_type_;

  image_set_ = o.image_set_;
  attachment_set_ = o.attachment_set_;
  clip_set_ = o.clip_set_;
  composite_set_ = o.composite_set_;
  blend_mode_set_ = o.blend_mode_set_;
  origin_set_ = o.origin_set_;
  repeat_x_set_ = o.repeat_x_set_;
  repeat_y_set_ = o.repeat_y_set_;
  x_pos_set_ = o.x_pos_set_;
  y_pos_set_ = o.y_pos_set_;
  mask_source_type_set_ = o.mask_source_type_set_;

  type_ = o.type_;

  cached_properties_computed_ = false;

  return *this;
}

bool FillLayer::operator==(const FillLayer& o) const {
  // We do not check the "isSet" booleans for each property, since those are
  // only used during initial construction to propagate patterns into layers.
  // All layer comparisons happen after values have all been filled in anyway.
  return DataEquivalent(image_, o.image_) && x_position_ == o.x_position_ &&
         y_position_ == o.y_position_ &&
         background_x_origin_ == o.background_x_origin_ &&
         background_y_origin_ == o.background_y_origin_ &&
         attachment_ == o.attachment_ && clip_ == o.clip_ &&
         composite_ == o.composite_ && blend_mode_ == o.blend_mode_ &&
         origin_ == o.origin_ && repeat_x_ == o.repeat_x_ &&
         repeat_y_ == o.repeat_y_ && size_type_ == o.size_type_ &&
         mask_source_type_ == o.mask_source_type_ &&
         size_length_ == o.size_length_ && type_ == o.type_ &&
         ((next_ && o.next_) ? *next_ == *o.next_ : next_ == o.next_);
}

void FillLayer::FillUnsetProperties() {
  FillLayer* curr;
  for (curr = this; curr && curr->IsXPositionSet(); curr = curr->Next()) {
  }
  if (curr && curr != this) {
    // We need to fill in the remaining values with the pattern specified.
    for (FillLayer* pattern = this; curr; curr = curr->Next()) {
      curr->x_position_ = pattern->x_position_;
      if (pattern->IsBackgroundXOriginSet())
        curr->background_x_origin_ = pattern->background_x_origin_;
      if (pattern->IsBackgroundYOriginSet())
        curr->background_y_origin_ = pattern->background_y_origin_;
      pattern = pattern->Next();
      if (pattern == curr || !pattern)
        pattern = this;
    }
  }

  for (curr = this; curr && curr->IsYPositionSet(); curr = curr->Next()) {
  }
  if (curr && curr != this) {
    // We need to fill in the remaining values with the pattern specified.
    for (FillLayer* pattern = this; curr; curr = curr->Next()) {
      curr->y_position_ = pattern->y_position_;
      if (pattern->IsBackgroundXOriginSet())
        curr->background_x_origin_ = pattern->background_x_origin_;
      if (pattern->IsBackgroundYOriginSet())
        curr->background_y_origin_ = pattern->background_y_origin_;
      pattern = pattern->Next();
      if (pattern == curr || !pattern)
        pattern = this;
    }
  }

  for (curr = this; curr && curr->IsAttachmentSet(); curr = curr->Next()) {
  }
  if (curr && curr != this) {
    // We need to fill in the remaining values with the pattern specified.
    for (FillLayer* pattern = this; curr; curr = curr->Next()) {
      curr->attachment_ = pattern->attachment_;
      pattern = pattern->Next();
      if (pattern == curr || !pattern)
        pattern = this;
    }
  }

  for (curr = this; curr && curr->IsClipSet(); curr = curr->Next()) {
  }
  if (curr && curr != this) {
    // We need to fill in the remaining values with the pattern specified.
    for (FillLayer* pattern = this; curr; curr = curr->Next()) {
      curr->clip_ = pattern->clip_;
      pattern = pattern->Next();
      if (pattern == curr || !pattern)
        pattern = this;
    }
  }

  for (curr = this; curr && curr->IsCompositeSet(); curr = curr->Next()) {
  }
  if (curr && curr != this) {
    // We need to fill in the remaining values with the pattern specified.
    for (FillLayer* pattern = this; curr; curr = curr->Next()) {
      curr->composite_ = pattern->composite_;
      pattern = pattern->Next();
      if (pattern == curr || !pattern)
        pattern = this;
    }
  }

  for (curr = this; curr && curr->IsBlendModeSet(); curr = curr->Next()) {
  }
  if (curr && curr != this) {
    // We need to fill in the remaining values with the pattern specified.
    for (FillLayer* pattern = this; curr; curr = curr->Next()) {
      curr->blend_mode_ = pattern->blend_mode_;
      pattern = pattern->Next();
      if (pattern == curr || !pattern)
        pattern = this;
    }
  }

  for (curr = this; curr && curr->IsOriginSet(); curr = curr->Next()) {
  }
  if (curr && curr != this) {
    // We need to fill in the remaining values with the pattern specified.
    for (FillLayer* pattern = this; curr; curr = curr->Next()) {
      curr->origin_ = pattern->origin_;
      pattern = pattern->Next();
      if (pattern == curr || !pattern)
        pattern = this;
    }
  }

  for (curr = this; curr && curr->IsRepeatXSet(); curr = curr->Next()) {
  }
  if (curr && curr != this) {
    // We need to fill in the remaining values with the pattern specified.
    for (FillLayer* pattern = this; curr; curr = curr->Next()) {
      curr->repeat_x_ = pattern->repeat_x_;
      pattern = pattern->Next();
      if (pattern == curr || !pattern)
        pattern = this;
    }
  }

  for (curr = this; curr && curr->IsRepeatYSet(); curr = curr->Next()) {
  }
  if (curr && curr != this) {
    // We need to fill in the remaining values with the pattern specified.
    for (FillLayer* pattern = this; curr; curr = curr->Next()) {
      curr->repeat_y_ = pattern->repeat_y_;
      pattern = pattern->Next();
      if (pattern == curr || !pattern)
        pattern = this;
    }
  }

  for (curr = this; curr && curr->IsSizeSet(); curr = curr->Next()) {
  }
  if (curr && curr != this) {
    // We need to fill in the remaining values with the pattern specified.
    for (FillLayer* pattern = this; curr; curr = curr->Next()) {
      curr->size_type_ = pattern->size_type_;
      curr->size_length_ = pattern->size_length_;
      pattern = pattern->Next();
      if (pattern == curr || !pattern)
        pattern = this;
    }
  }
}

void FillLayer::CullEmptyLayers() {
  FillLayer* next;
  for (FillLayer* p = this; p; p = next) {
    next = p->next_;
    if (next && !next->IsImageSet()) {
      delete next;
      p->next_ = 0;
      break;
    }
  }
}

void FillLayer::ComputeCachedPropertiesIfNeeded() const {
  if (cached_properties_computed_)
    return;
  this_or_next_layers_clip_max_ = Clip();
  this_or_next_layers_use_content_box_ =
      Clip() == kContentFillBox || Origin() == kContentFillBox;
  this_or_next_layers_have_local_attachment_ =
      Attachment() == kLocalBackgroundAttachment;
  cached_properties_computed_ = true;

  if (next_) {
    next_->ComputeCachedPropertiesIfNeeded();
    this_or_next_layers_clip_max_ = EnclosingFillBox(
        ThisOrNextLayersClipMax(), next_->ThisOrNextLayersClipMax());
    this_or_next_layers_use_content_box_ |=
        next_->this_or_next_layers_use_content_box_;
    this_or_next_layers_have_local_attachment_ |=
        next_->this_or_next_layers_have_local_attachment_;
  }
}

bool FillLayer::ClipOccludesNextLayers() const {
  return Clip() == ThisOrNextLayersClipMax();
}

bool FillLayer::ContainsImage(StyleImage* s) const {
  if (!s)
    return false;
  if (image_ && *s == *image_)
    return true;
  if (next_)
    return next_->ContainsImage(s);
  return false;
}

bool FillLayer::ImagesAreLoaded() const {
  const FillLayer* curr;
  for (curr = this; curr; curr = curr->Next()) {
    if (curr->image_ && !curr->image_->IsLoaded())
      return false;
  }

  return true;
}

bool FillLayer::ImageIsOpaque(const Document& document,
                              const ComputedStyle& style) const {
  // Returns true if we have an image that will cover the content below it when
  // m_composite == CompositeSourceOver && m_blendMode == WebBlendModeNormal.
  // Otherwise false.
  return image_->KnownToBeOpaque(document, style) &&
         !image_->ImageSize(document, style.EffectiveZoom(), LayoutSize())
              .IsEmpty();
}

bool FillLayer::ImageTilesLayer() const {
  // Returns true if an image will be tiled such that it covers any sized
  // rectangle.
  // TODO(schenney) We could relax the repeat mode requirement if we also knew
  // the rect we had to fill, and the portion of the image we need to use, and
  // know that the latter covers the former.
  return (repeat_x_ == kRepeatFill || repeat_x_ == kRoundFill) &&
         (repeat_y_ == kRepeatFill || repeat_y_ == kRoundFill);
}

bool FillLayer::ImageOccludesNextLayers(const Document& document,
                                        const ComputedStyle& style) const {
  // We can't cover without an image, regardless of other parameters
  if (!image_ || !image_->CanRender())
    return false;

  switch (composite_) {
    case kCompositeClear:
    case kCompositeCopy:
      return ImageTilesLayer();
    case kCompositeSourceOver:
      return (blend_mode_ == static_cast<unsigned>(WebBlendMode::kNormal)) &&
             ImageTilesLayer() && ImageIsOpaque(document, style);
    default: {}
  }

  return false;
}

static inline bool LayerImagesIdentical(const FillLayer& layer1,
                                        const FillLayer& layer2) {
  // We just care about pointer equivalency.
  return layer1.GetImage() == layer2.GetImage();
}

bool FillLayer::ImagesIdentical(const FillLayer* layer1,
                                const FillLayer* layer2) {
  for (; layer1 && layer2; layer1 = layer1->Next(), layer2 = layer2->Next()) {
    if (!LayerImagesIdentical(*layer1, *layer2))
      return false;
  }

  return !layer1 && !layer2;
}

}  // namespace blink
