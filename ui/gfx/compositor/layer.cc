// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/layer.h"

#include <algorithm>

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContentLayer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebExternalTextureLayer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFloatPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFloatRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSize.h"
#include "ui/base/animation/animation.h"
#if defined(USE_WEBKIT_COMPOSITOR)
#include "ui/gfx/compositor/compositor_cc.h"
#endif
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/compositor/layer_animator.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/point3.h"

namespace {

const float EPSILON = 1e-3f;

bool IsApproximateMultilpleOf(float value, float base) {
  float remainder = fmod(fabs(value), base);
  return remainder < EPSILON || base - remainder < EPSILON;
}

}  // namespace

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
#if defined(USE_WEBKIT_COMPOSITOR)
  CreateWebLayer();
#endif
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
#if defined(USE_WEBKIT_COMPOSITOR)
  CreateWebLayer();
#endif
}

Layer::~Layer() {
  if (parent_)
    parent_->Remove(this);
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->parent_ = NULL;
#if defined(USE_WEBKIT_COMPOSITOR)
  web_layer_.removeFromParent();
#endif
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
#if defined(USE_WEBKIT_COMPOSITOR)
  web_layer_.addChild(child->web_layer_);
#endif

  RecomputeHole();
}

void Layer::Remove(Layer* child) {
  std::vector<Layer*>::iterator i =
      std::find(children_.begin(), children_.end(), child);
  DCHECK(i != children_.end());
  children_.erase(i);
  child->parent_ = NULL;
#if defined(USE_WEBKIT_COMPOSITOR)
  child->web_layer_.removeFromParent();
#endif

  RecomputeHole();

  child->DropTextures();
}

void Layer::MoveToFront(Layer* child) {
  if (children_.size() <= 1 || child == children_.back())
    return;  // Already in front.
  MoveAbove(child, children_.back());
}

void Layer::MoveAbove(Layer* child, Layer* other) {
  DCHECK_NE(child, other);
  DCHECK_EQ(this, child->parent());
  DCHECK_EQ(this, other->parent());
  size_t child_i =
      std::find(children_.begin(), children_.end(), child) - children_.begin();
  size_t other_i =
      std::find(children_.begin(), children_.end(), other) - children_.begin();
  if (child_i > other_i)
    return;  // Already in front of |other|.

  // Reorder children.
  children_.erase(children_.begin() + child_i);
  children_.insert(children_.begin() + other_i, child);

#if defined(USE_WEBKIT_COMPOSITOR)
  child->web_layer_.removeFromParent();
  web_layer_.insertChild(child->web_layer_, other_i);
#endif
}

bool Layer::Contains(const Layer* other) const {
  for (const Layer* parent = other; parent; parent = parent->parent()) {
    if (parent == this)
      return true;
  }
  return false;
}

void Layer::SetAnimator(LayerAnimator* animator) {
  if (animator)
    animator->SetDelegate(this);
  animator_.reset(animator);
}

LayerAnimator* Layer::GetAnimator() {
  if (!animator_.get())
    SetAnimator(LayerAnimator::CreateDefaultAnimator());
  return animator_.get();
}

void Layer::SetTransform(const ui::Transform& transform) {
  GetAnimator()->SetTransform(transform);
}

Transform Layer::GetTargetTransform() const {
  if (animator_.get() && animator_->is_animating())
    return animator_->GetTargetTransform();
  return transform_;
}

void Layer::SetBounds(const gfx::Rect& bounds) {
  GetAnimator()->SetBounds(bounds);
}

gfx::Rect Layer::GetTargetBounds() const {
  if (animator_.get() && animator_->is_animating())
    return animator_->GetTargetBounds();
  return bounds_;
}

void Layer::SetOpacity(float opacity) {
  GetAnimator()->SetOpacity(opacity);
}

float Layer::GetTargetOpacity() const {
  if (animator_.get() && animator_->is_animating())
    return animator_->GetTargetOpacity();
  return opacity_;
}

void Layer::SetVisible(bool visible) {
  if (visible_ == visible)
    return;

  bool was_drawn = IsDrawn();
  visible_ = visible;
#if defined(USE_WEBKIT_COMPOSITOR)
  // TODO(piman): Expose a visibility flag on WebLayer.
  web_layer_.setOpacity(visible_ ? opacity_ : 0.f);
#endif
  bool is_drawn = IsDrawn();
  if (was_drawn == is_drawn)
    return;

  if (!is_drawn)
    DropTextures();
  if (parent_)
    parent_->RecomputeHole();
}

bool Layer::IsDrawn() const {
  const Layer* layer = this;
  while (layer && layer->visible_)
    layer = layer->parent_;
  return layer == NULL;
}

bool Layer::ShouldDraw() const {
  return type_ == LAYER_HAS_TEXTURE && GetCombinedOpacity() > 0.0f &&
      !hole_rect_.Contains(gfx::Rect(gfx::Point(0, 0), bounds_.size()));
}

// static
void Layer::ConvertPointToLayer(const Layer* source,
                                const Layer* target,
                                gfx::Point* point) {
  if (source == target)
    return;

  const Layer* root_layer = source->compositor()->root_layer();
  CHECK_EQ(root_layer, target->compositor()->root_layer());

  if (source != root_layer)
    source->ConvertPointForAncestor(root_layer, point);
  if (target != root_layer)
    target->ConvertPointFromAncestor(root_layer, point);
}

void Layer::SetFillsBoundsOpaquely(bool fills_bounds_opaquely) {
  if (fills_bounds_opaquely_ == fills_bounds_opaquely)
    return;

  fills_bounds_opaquely_ = fills_bounds_opaquely;

  if (parent())
    parent()->RecomputeHole();
#if defined(USE_WEBKIT_COMPOSITOR)
  web_layer_.setOpaque(fills_bounds_opaquely);
#endif
}

void Layer::SetExternalTexture(ui::Texture* texture) {
  DCHECK(texture);
  layer_updated_externally_ = true;
  texture_ = texture;
#if defined(USE_WEBKIT_COMPOSITOR)
  if (!web_layer_is_accelerated_) {
    web_layer_.removeAllChildren();
    WebKit::WebLayer new_layer =
      WebKit::WebExternalTextureLayer::create(this);
    if (parent_) {
      DCHECK(!parent_->web_layer_.isNull());
      parent_->web_layer_.replaceChild(web_layer_, new_layer);
    }
    web_layer_ = new_layer;
    web_layer_is_accelerated_ = true;
    for (size_t i = 0; i < children_.size(); ++i) {
      DCHECK(!children_[i]->web_layer_.isNull());
      web_layer_.addChild(children_[i]->web_layer_);
    }
    web_layer_.setAnchorPoint(WebKit::WebFloatPoint(0.f, 0.f));
    web_layer_.setOpaque(fills_bounds_opaquely_);
    web_layer_.setOpacity(visible_ ? opacity_ : 0.f);
    web_layer_.setBounds(bounds_.size());
    RecomputeTransform();
  }
  TextureCC* texture_cc = static_cast<TextureCC*>(texture);
  texture_cc->Update();
  web_layer_.to<WebKit::WebExternalTextureLayer>().setFlipped(
      texture_cc->flipped());
  RecomputeDrawsContent();
#endif
}

void Layer::SetCanvas(const SkCanvas& canvas, const gfx::Point& origin) {
#if defined(USE_WEBKIT_COMPOSITOR)
  NOTREACHED();
#else
  DCHECK_EQ(type_, LAYER_HAS_TEXTURE);

  if (!texture_.get())
    texture_ = GetCompositor()->CreateTexture();

  texture_->SetCanvas(canvas, origin, bounds_.size());
  invalid_rect_ = gfx::Rect();
#endif
}

void Layer::SchedulePaint(const gfx::Rect& invalid_rect) {
#if defined(USE_WEBKIT_COMPOSITOR)
  WebKit::WebFloatRect web_rect(invalid_rect.x(),
      invalid_rect.y(),
      invalid_rect.width(),
      invalid_rect.height());
  if (!web_layer_is_accelerated_)
    web_layer_.to<WebKit::WebContentLayer>().invalidateRect(web_rect);
#else
  invalid_rect_ = invalid_rect_.Union(invalid_rect);
  ScheduleDraw();
#endif
}

void Layer::ScheduleDraw() {
  Compositor* compositor = GetCompositor();
  if (compositor)
    compositor->ScheduleDraw();
}

void Layer::Draw() {
  TRACE_EVENT0("ui", "Layer::Draw");
#if defined(USE_WEBKIT_COMPOSITOR)
  NOTREACHED();
#else
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
#endif
}

void Layer::DrawTree() {
#if defined(USE_WEBKIT_COMPOSITOR)
  NOTREACHED();
#else
  if (!visible_)
    return;

  Draw();
  for (size_t i = 0; i < children_.size(); ++i)
    children_.at(i)->DrawTree();
#endif
}

void Layer::notifyNeedsComposite() {
#if defined(USE_WEBKIT_COMPOSITOR)
  ScheduleDraw();
#else
  NOTREACHED();
#endif
}

void Layer::paintContents(WebKit::WebCanvas* web_canvas,
                          const WebKit::WebRect& clip) {
  TRACE_EVENT0("ui", "Layer::paintContents");
#if defined(USE_WEBKIT_COMPOSITOR)
  gfx::CanvasSkia canvas(web_canvas);
  delegate_->OnPaintLayer(&canvas);
#else
  NOTREACHED();
#endif
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
  TRACE_EVENT0("ui", "Layer::UpdateLayerCanvas");
#if defined(USE_WEBKIT_COMPOSITOR)
  NOTREACHED();
#else
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
  scoped_ptr<gfx::Canvas> canvas(gfx::Canvas::CreateCanvas(draw_rect.size(),
                                                           false));
  canvas->Translate(gfx::Point().Subtract(draw_rect.origin()));
  delegate_->OnPaintLayer(canvas.get());
  SetCanvas(*canvas->GetSkCanvas(), draw_rect.origin());
#endif
}

void Layer::RecomputeHole() {
  if (type_ == LAYER_HAS_NO_TEXTURE)
    return;

  // Reset to default.
  hole_rect_ = gfx::Rect();

  // Find the largest hole
  for (size_t i = 0; i < children_.size(); ++i) {
    // Ignore non-opaque and hidden children.
    if (!children_[i]->IsCompletelyOpaque() || !children_[i]->visible_)
      continue;

    // Ignore children that aren't rotated by multiples of 90 degrees.
    float degrees;
    if (!InterpolatedTransform::FactorTRS(children_[i]->transform(),
                                          NULL, &degrees, NULL) ||
        !IsApproximateMultilpleOf(degrees, 90.0f))
      continue;

    // The reason why we don't just take the bounds and apply the transform is
    // that the bounds encodes a position, too, so the effective transformation
    // matrix is actually different that the one reported. As well, the bounds
    // will not necessarily be at the origin.
    gfx::Rect candidate_hole(children_[i]->bounds_.size());
    ui::Transform transform = children_[i]->transform();
    transform.ConcatTranslate(static_cast<float>(children_[i]->bounds_.x()),
                              static_cast<float>(children_[i]->bounds_.y()));
    transform.TransformRect(&candidate_hole);

    // This layer might not contain the child (e.g., a portion of the child may
    // be offscreen). Only the portion of the child that overlaps this layer is
    // of any importance, so take the intersection.
    candidate_hole = gfx::Rect(bounds().size()).Intersect(candidate_hole);

    // Ensure we have the largest hole.
    if (candidate_hole.size().GetArea() > hole_rect_.size().GetArea())
      hole_rect_ = candidate_hole;
  }

  // Free up texture memory if the hole fills bounds of layer.
  if (!ShouldDraw() && !layer_updated_externally_)
    texture_ = NULL;

#if defined(USE_WEBKIT_COMPOSITOR)
  RecomputeDrawsContent();
#endif
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

void Layer::SetBoundsImmediately(const gfx::Rect& bounds) {
  if (bounds == bounds_)
    return;

  bool was_move = bounds_.size() == bounds.size();
  bounds_ = bounds;
  if (IsDrawn()) {
    if (was_move)
      ScheduleDraw();
    else
      SchedulePaint(gfx::Rect(bounds.size()));
  }

  if (parent())
    parent()->RecomputeHole();
#if defined(USE_WEBKIT_COMPOSITOR)
  web_layer_.setBounds(bounds.size());
  RecomputeTransform();
  RecomputeDrawsContent();
#endif
}

void Layer::SetTransformImmediately(const ui::Transform& transform) {
  transform_ = transform;

  if (parent())
    parent()->RecomputeHole();
#if defined(USE_WEBKIT_COMPOSITOR)
  RecomputeTransform();
#endif
}

void Layer::SetOpacityImmediately(float opacity) {
  bool schedule_draw = (opacity != opacity_ && IsDrawn());
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
#if defined(USE_WEBKIT_COMPOSITOR)
  if (visible_)
    web_layer_.setOpacity(opacity);
#endif
  if (schedule_draw)
    ScheduleDraw();
}

void Layer::SetBoundsFromAnimation(const gfx::Rect& bounds) {
  SetBoundsImmediately(bounds);
}

void Layer::SetTransformFromAnimation(const Transform& transform) {
  SetTransformImmediately(transform);
}

void Layer::SetOpacityFromAnimation(float opacity) {
  SetOpacityImmediately(opacity);
}

void Layer::ScheduleDrawForAnimation() {
  ScheduleDraw();
}

const gfx::Rect& Layer::GetBoundsForAnimation() const {
  return bounds();
}

const Transform& Layer::GetTransformForAnimation() const {
  return transform();
}

float Layer::GetOpacityForAnimation() const {
  return opacity();
}

#if defined(USE_WEBKIT_COMPOSITOR)
void Layer::CreateWebLayer() {
  web_layer_ = WebKit::WebContentLayer::create(this, this);
  web_layer_.setAnchorPoint(WebKit::WebFloatPoint(0.f, 0.f));
  web_layer_.setOpaque(true);
  web_layer_is_accelerated_ = false;
  RecomputeDrawsContent();
}

void Layer::RecomputeTransform() {
  ui::Transform transform = transform_;
  transform.ConcatTranslate(bounds_.x(), bounds_.y());
  web_layer_.setTransform(transform.matrix());
}

void Layer::RecomputeDrawsContent() {
  DCHECK(!web_layer_.isNull());
  bool should_draw = type_ == LAYER_HAS_TEXTURE &&
      !hole_rect_.Contains(gfx::Rect(gfx::Point(0, 0), bounds_.size()));
  if (!web_layer_is_accelerated_) {
    web_layer_.to<WebKit::WebContentLayer>().setDrawsContent(should_draw);
  } else {
    DCHECK(texture_);
#if defined(USE_WEBKIT_COMPOSITOR)
    unsigned int texture_id =
        static_cast<TextureCC*>(texture_.get())->texture_id();
#else
    unsigned int texture_id = 0;
#endif
    web_layer_.to<WebKit::WebExternalTextureLayer>().setTextureId(
        should_draw ? texture_id : 0);
  }
}
#endif

}  // namespace ui
