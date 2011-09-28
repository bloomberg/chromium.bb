// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/layer.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/point3.h"

namespace ui {

Layer::Layer(Compositor* compositor)
    : type_(LAYER_HAS_TEXTURE),
      compositor_(compositor),
      parent_(NULL),
      visible_(true),
      fills_bounds_opaquely_(false),
      layer_updated_externally_(false),
      opacity_(1.0f),
      delegate_(NULL) {
}

Layer::Layer(Compositor* compositor, LayerType type)
    : type_(type),
      compositor_(compositor),
      parent_(NULL),
      visible_(true),
      fills_bounds_opaquely_(false),
      layer_updated_externally_(false),
      opacity_(1.0f),
      delegate_(NULL) {
}

Layer::~Layer() {
  if (parent_)
    parent_->Remove(this);
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->parent_ = NULL;
}

Compositor* Layer::GetCompositor() {
  return compositor_ ? compositor_ : parent_ ? parent_->GetCompositor() : NULL;
}

void Layer::SetCompositor(Compositor* compositor) {
  // This function must only be called once, with a valid compositor, and only
  // for the compositor's root layer.
  DCHECK(!compositor_);
  DCHECK(compositor);
  DCHECK_EQ(compositor->root_layer(), this);
  compositor_ = compositor;
}

void Layer::Add(Layer* child) {
  if (child->parent_)
    child->parent_->Remove(child);
  child->parent_ = this;
  children_.push_back(child);

  if (child->fills_bounds_opaquely())
    RecomputeHole();
}

void Layer::Remove(Layer* child) {
  std::vector<Layer*>::iterator i =
      std::find(children_.begin(), children_.end(), child);
  DCHECK(i != children_.end());
  children_.erase(i);
  child->parent_ = NULL;

  if (child->fills_bounds_opaquely())
    RecomputeHole();

  child->DropTextures();
}

bool Layer::Contains(const Layer* other) const {
  for (const Layer* parent = other; parent; parent = parent->parent()) {
    if (parent == this)
      return true;
  }
  return false;
}

void Layer::SetTransform(const ui::Transform& transform) {
  transform_ = transform;

  if (parent() && fills_bounds_opaquely_)
    parent()->RecomputeHole();
}

void Layer::SetBounds(const gfx::Rect& bounds) {
  bounds_ = bounds;

  if (parent() && fills_bounds_opaquely_)
    parent()->RecomputeHole();
}

void Layer::SetVisible(bool visible) {
  visible_ = visible;
  if (!visible_)
    DropTextures();
}

bool Layer::ShouldDraw() {
  return type_ == LAYER_HAS_TEXTURE && GetCombinedOpacity() > 0.0f &&
      !hole_rect_.Contains(gfx::Rect(gfx::Point(0, 0), bounds_.size()));
}

// static
void Layer::ConvertPointToLayer(const Layer* source,
                                const Layer* target,
                                gfx::Point* point) {
  const Layer* inner = NULL;
  const Layer* outer = NULL;
  if (source->Contains(target)) {
    inner = target;
    outer = source;
    inner->ConvertPointFromAncestor(outer, point);
  } else if (target->Contains(source)) {
    inner = source;
    outer = target;
    inner->ConvertPointForAncestor(outer, point);
  } else {
    NOTREACHED(); // |source| and |target| are in unrelated hierarchies.
  }
}

void Layer::SetFillsBoundsOpaquely(bool fills_bounds_opaquely) {
  if (fills_bounds_opaquely_ == fills_bounds_opaquely)
    return;

  fills_bounds_opaquely_ = fills_bounds_opaquely;

  if (parent())
    parent()->RecomputeHole();
}

void Layer::SetExternalTexture(ui::Texture* texture) {
  DCHECK(texture);
  layer_updated_externally_ = true;
  texture_ = texture;
}

void Layer::SetCanvas(const SkCanvas& canvas, const gfx::Point& origin) {
  DCHECK_EQ(type_, LAYER_HAS_TEXTURE);

  if (!texture_.get())
    texture_ = GetCompositor()->CreateTexture();

  texture_->SetCanvas(canvas, origin, bounds_.size());
  invalid_rect_ = gfx::Rect();
}

void Layer::SchedulePaint(const gfx::Rect& invalid_rect) {
  invalid_rect_ = invalid_rect_.Union(invalid_rect);
  if (GetCompositor())
    GetCompositor()->SchedulePaint();
}

void Layer::Draw() {
  DCHECK(GetCompositor());
  if (!ShouldDraw())
    return;

  UpdateLayerCanvas();

  // Layer drew nothing, no texture was created.
  if (!texture_.get())
    return;

  ui::TextureDrawParams texture_draw_params;
  for (Layer* layer = this; layer; layer = layer->parent_) {
    texture_draw_params.transform.ConcatTransform(layer->transform_);
    texture_draw_params.transform.ConcatTranslate(
        static_cast<float>(layer->bounds_.x()),
        static_cast<float>(layer->bounds_.y()));
  }

  const float combined_opacity = GetCombinedOpacity();

  // Only blend for transparent child layers (and when we're forcing
  // transparency). The root layer will clobber the cleared bg.
  const bool is_root = parent_ == NULL;
  const bool forcing_transparency = combined_opacity < 1.0f;
  const bool is_opaque = fills_bounds_opaquely_ || !has_valid_alpha_channel();
  texture_draw_params.blend = !is_root && (forcing_transparency || !is_opaque);

  texture_draw_params.compositor_size = GetCompositor()->size();
  texture_draw_params.opacity = combined_opacity;
  texture_draw_params.has_valid_alpha_channel = has_valid_alpha_channel();

  hole_rect_ = hole_rect_.Intersect(
      gfx::Rect(0, 0, bounds_.width(), bounds_.height()));
  if (hole_rect_.IsEmpty()) {
    DrawRegion(texture_draw_params,
               gfx::Rect(0, 0, bounds_.width(), bounds_.height()));
  } else {
    // Top (above the hole).
    DrawRegion(texture_draw_params, gfx::Rect(0,
                                              0,
                                              bounds_.width(),
                                              hole_rect_.y()));
    // Left (of the hole).
    DrawRegion(texture_draw_params, gfx::Rect(0,
                                              hole_rect_.y(),
                                              hole_rect_.x(),
                                              hole_rect_.height()));
    // Right (of the hole).
    DrawRegion(texture_draw_params, gfx::Rect(
        hole_rect_.right(),
        hole_rect_.y(),
        bounds_.width() - hole_rect_.right(),
        hole_rect_.height()));

    // Bottom (below the hole).
    DrawRegion(texture_draw_params, gfx::Rect(
        0,
        hole_rect_.bottom(),
        bounds_.width(),
        bounds_.height() - hole_rect_.bottom()));
  }
}

void Layer::DrawTree() {
  if (!visible_)
    return;

  Draw();
  for (size_t i = 0; i < children_.size(); ++i)
    children_.at(i)->DrawTree();
}

void Layer::SetOpacity(float alpha) {
  bool was_opaque = GetCombinedOpacity() == 1.0f;
  opacity_ = alpha;
  bool is_opaque = GetCombinedOpacity() == 1.0f;

  // If our opacity has changed we need to recompute our hole, our parent's hole
  // and the holes of all our descendants.
  if (was_opaque != is_opaque) {
    if (parent_)
      parent_->RecomputeHole();
    std::queue<Layer*> to_process;
    to_process.push(this);
    while (!to_process.empty()) {
      Layer* current = to_process.front();
      to_process.pop();
      current->RecomputeHole();
      for (size_t i = 0; i < current->children_.size(); ++i)
        to_process.push(current->children_.at(i));
    }
  }
}

float Layer::GetCombinedOpacity() const {
  float opacity = opacity_;
  Layer* current = this->parent_;
  while (current) {
    opacity *= current->opacity_;
    current = current->parent_;
  }
  return opacity;
}

void Layer::DrawRegion(const ui::TextureDrawParams& params,
                       const gfx::Rect& region_to_draw) {
  if (!region_to_draw.IsEmpty())
    texture_->Draw(params, region_to_draw);
}

void Layer::UpdateLayerCanvas() {
  // If we have no delegate, that means that whoever constructed the Layer is
  // setting its canvas directly with SetCanvas().
  if (!delegate_ || layer_updated_externally_)
    return;
  gfx::Rect local_bounds = gfx::Rect(gfx::Point(), bounds_.size());
  gfx::Rect draw_rect = texture_.get() ? invalid_rect_.Intersect(local_bounds) :
      local_bounds;
  if (draw_rect.IsEmpty()) {
    invalid_rect_ = gfx::Rect();
    return;
  }
  scoped_ptr<gfx::Canvas> canvas(gfx::Canvas::CreateCanvas(
      draw_rect.width(), draw_rect.height(), false));
  canvas->TranslateInt(-draw_rect.x(), -draw_rect.y());
  delegate_->OnPaintLayer(canvas.get());
  SetCanvas(*canvas->AsCanvasSkia(), draw_rect.origin());
}

void Layer::RecomputeHole() {
  if (type_ == LAYER_HAS_NO_TEXTURE)
    return;

  // If there are no opaque child layers, hole_rect_ should remain empty.
  hole_rect_ = gfx::Rect();

  for (size_t i = 0; i < children_.size(); ++i) {
    if (children_[i]->fills_bounds_opaquely() &&
        children_[i]->GetCombinedOpacity() == 1.0f &&
        !children_[i]->transform().HasChange()) {
      hole_rect_ = children_[i]->bounds();
      break;
    }
  }

  // Free up texture memory if the hole fills bounds of layer
  if (!ShouldDraw() && !layer_updated_externally_)
    texture_ = NULL;
}

void Layer::DropTextures() {
  if (!layer_updated_externally_)
    texture_ = NULL;
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->DropTextures();
}

bool Layer::ConvertPointForAncestor(const Layer* ancestor,
                                    gfx::Point* point) const {
  ui::Transform transform;
  bool result = GetTransformRelativeTo(ancestor, &transform);
  gfx::Point3f p(*point);
  transform.TransformPoint(p);
  *point = p.AsPoint();
  return result;
}

bool Layer::ConvertPointFromAncestor(const Layer* ancestor,
                                     gfx::Point* point) const {
  ui::Transform transform;
  bool result = GetTransformRelativeTo(ancestor, &transform);
  gfx::Point3f p(*point);
  transform.TransformPointReverse(p);
  *point = p.AsPoint();
  return result;
}

bool Layer::GetTransformRelativeTo(const Layer* ancestor,
                                   ui::Transform* transform) const {
  const Layer* p = this;
  for (; p && p != ancestor; p = p->parent()) {
    if (p->transform().HasChange())
      transform->ConcatTransform(p->transform());
    transform->ConcatTranslate(static_cast<float>(p->bounds().x()),
                               static_cast<float>(p->bounds().y()));
  }
  return p == ancestor;
}

}  // namespace ui
