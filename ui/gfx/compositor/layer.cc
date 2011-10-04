// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/layer.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/animation/animation.h"
#include "ui/gfx/compositor/layer_animator.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/point3.h"

namespace ui {

Layer::Layer(Compositor* compositor)
    : type_(LAYER_HAS_TEXTURE),
      compositor_(compositor),
      parent_(NULL),
      visible_(true),
      fills_bounds_opaquely_(true),
      layer_updated_externally_(false),
      opacity_(1.0f),
      delegate_(NULL) {
}

Layer::Layer(Compositor* compositor, LayerType type)
    : type_(type),
      compositor_(compositor),
      parent_(NULL),
      visible_(true),
      fills_bounds_opaquely_(true),
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

  RecomputeHole();
}

void Layer::Remove(Layer* child) {
  std::vector<Layer*>::iterator i =
      std::find(children_.begin(), children_.end(), child);
  DCHECK(i != children_.end());
  children_.erase(i);
  child->parent_ = NULL;

  RecomputeHole();

  child->DropTextures();
}

void Layer::MoveToFront(Layer* child) {
  std::vector<Layer*>::iterator i =
      std::find(children_.begin(), children_.end(), child);
  DCHECK(i != children_.end());
  children_.erase(i);
  children_.push_back(child);
}

bool Layer::Contains(const Layer* other) const {
  for (const Layer* parent = other; parent; parent = parent->parent()) {
    if (parent == this)
      return true;
  }
  return false;
}

void Layer::SetAnimation(Animation* animation) {
  if (animation) {
    if (!animator_.get())
      animator_.reset(new LayerAnimator(this));
    animation->Start();
    animator_->SetAnimation(animation);
  } else {
    animator_.reset();
  }
}

void Layer::SetTransform(const ui::Transform& transform) {
  StopAnimatingIfNecessary(LayerAnimator::TRANSFORM);
  if (animator_.get() && animator_->IsRunning()) {
    animator_->AnimateTransform(transform);
    return;
  }
  SetTransformImmediately(transform);
}

void Layer::SetBounds(const gfx::Rect& bounds) {
  StopAnimatingIfNecessary(LayerAnimator::LOCATION);
  if (animator_.get() && animator_->IsRunning() &&
      bounds.size() == bounds_.size()) {
    animator_->AnimateToPoint(bounds.origin());
    return;
  }
  SetBoundsImmediately(bounds);
}

void Layer::SetOpacity(float opacity) {
  StopAnimatingIfNecessary(LayerAnimator::OPACITY);
  if (animator_.get() && animator_->IsRunning()) {
    animator_->AnimateOpacity(opacity);
    return;
  }
  SetOpacityImmediately(opacity);
}

void Layer::SetVisible(bool visible) {
  visible_ = visible;
  if (!visible_)
    DropTextures();
  if (parent_)
    parent_->RecomputeHole();
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
  ScheduleDraw();
}

void Layer::ScheduleDraw() {
  if (GetCompositor())
    GetCompositor()->ScheduleDraw();
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

  std::vector<gfx::Rect> regions_to_draw;
  PunchHole(gfx::Rect(gfx::Point(), bounds().size()), hole_rect_,
            &regions_to_draw);

  for (size_t i = 0; i < regions_to_draw.size(); ++i) {
    if (!regions_to_draw[i].IsEmpty())
      texture_->Draw(texture_draw_params, regions_to_draw[i]);
  }
}

void Layer::DrawTree() {
  if (!visible_)
    return;

  Draw();
  for (size_t i = 0; i < children_.size(); ++i)
    children_.at(i)->DrawTree();
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

  // Reset to default.
  hole_rect_ = gfx::Rect();

  // 1) We cannot do any hole punching if any child has a transform.
  for (size_t i = 0; i < children_.size(); ++i) {
    if (children_[i]->transform().HasChange() && children_[i]->visible())
      return;
  }

  // 2) Find the largest hole.
  for (size_t i = 0; i < children_.size(); ++i) {
    // Ignore non-opaque and hidden children.
    if (!children_[i]->IsCompletelyOpaque() || !children_[i]->visible())
      continue;

    if (children_[i]->bounds().size().GetArea() > hole_rect_.size().GetArea()) {
      hole_rect_ = children_[i]->bounds();
    }
  }

  // 3) Make sure hole does not intersect with non-opaque children.
  for (size_t i = 0; i < children_.size(); ++i) {
    // Ignore opaque and hidden children.
    if (children_[i]->IsCompletelyOpaque() || !children_[i]->visible())
      continue;

    // Ignore non-intersecting children.
    if (!hole_rect_.Intersects(children_[i]->bounds()))
      continue;

    // Compute surrounding fragments.
    std::vector<gfx::Rect> fragments;
    PunchHole(hole_rect_, children_[i]->bounds(), &fragments);

    // Pick the largest surrounding fragment as the new hole.
    hole_rect_ = fragments[0];
    for (size_t j = 1; j < fragments.size(); ++j) {
      if (fragments[j].size().GetArea() > hole_rect_.size().GetArea()) {
        hole_rect_ = fragments[j];
      }
    }
    DCHECK(!hole_rect_.IsEmpty());
  }

  // 4) Free up texture memory if the hole fills bounds of layer.
  if (!ShouldDraw() && !layer_updated_externally_)
    texture_ = NULL;
}

bool Layer::IsCompletelyOpaque() const {
  return fills_bounds_opaquely() && GetCombinedOpacity() == 1.0f;
}

// static
void Layer::PunchHole(const gfx::Rect& rect,
                      const gfx::Rect& region_to_punch_out,
                      std::vector<gfx::Rect>* sides) {
  gfx::Rect trimmed_rect = rect.Intersect(region_to_punch_out);

  if (trimmed_rect.IsEmpty()) {
    sides->push_back(rect);
    return;
  }

  // Top (above the hole).
  sides->push_back(gfx::Rect(rect.x(),
                             rect.y(),
                             rect.width(),
                             trimmed_rect.y() - rect.y()));

  // Left (of the hole).
  sides->push_back(gfx::Rect(rect.x(),
                             trimmed_rect.y(),
                             trimmed_rect.x() - rect.x(),
                             trimmed_rect.height()));

  // Right (of the hole).
  sides->push_back(gfx::Rect(trimmed_rect.right(),
                             trimmed_rect.y(),
                             rect.right() - trimmed_rect.right(),
                             trimmed_rect.height()));

  // Bottom (below the hole).
  sides->push_back(gfx::Rect(rect.x(),
                             trimmed_rect.bottom(),
                             rect.width(),
                             rect.bottom() - trimmed_rect.bottom()));
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

void Layer::StopAnimatingIfNecessary(
    LayerAnimator::AnimationProperty property) {
  if (!animator_.get() || !animator_->IsRunning() ||
      !animator_->got_initial_tick()) {
    return;
  }

  if (property != LayerAnimator::LOCATION &&
      animator_->IsAnimating(LayerAnimator::LOCATION)) {
    SetBoundsImmediately(
        gfx::Rect(animator_->GetTargetPoint(), bounds_.size()));
  }
  if (property != LayerAnimator::OPACITY &&
      animator_->IsAnimating(LayerAnimator::OPACITY)) {
    SetOpacityImmediately(animator_->GetTargetOpacity());
  }
  if (property != LayerAnimator::TRANSFORM &&
      animator_->IsAnimating(LayerAnimator::TRANSFORM)) {
    SetTransformImmediately(animator_->GetTargetTransform());
  }
  animator_.reset();
}

void Layer::SetBoundsImmediately(const gfx::Rect& bounds) {
  bounds_ = bounds;

  if (parent())
    parent()->RecomputeHole();
}

void Layer::SetTransformImmediately(const ui::Transform& transform) {
  transform_ = transform;

  if (parent())
    parent()->RecomputeHole();
}

void Layer::SetOpacityImmediately(float opacity) {
  bool was_opaque = GetCombinedOpacity() == 1.0f;
  opacity_ = opacity;
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

void Layer::SetBoundsFromAnimator(const gfx::Rect& bounds) {
  SetBoundsImmediately(bounds);
}

void Layer::SetTransformFromAnimator(const Transform& transform) {
  SetTransformImmediately(transform);
}

void Layer::SetOpacityFromAnimator(float opacity) {
  SetOpacityImmediately(opacity);
}

}  // namespace ui
