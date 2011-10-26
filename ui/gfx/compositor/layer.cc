// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/layer.h"

#include <algorithm>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebExternalTextureLayer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContentLayer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFloatPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFloatRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSize.h"
#include "ui/base/animation/animation.h"
#if defined(USE_WEBKIT_COMPOSITOR)
#include "ui/gfx/compositor/compositor_cc.h"
#endif
#include "ui/gfx/compositor/layer_animation_manager.h"
#include "ui/gfx/canvas_skia.h"
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
      recompute_hole_(false),
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
      recompute_hole_(false),
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

  SetNeedsToRecomputeHole();
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

  SetNeedsToRecomputeHole();

  child->DropTextures();
}

void Layer::MoveToFront(Layer* child) {
  std::vector<Layer*>::iterator i =
      std::find(children_.begin(), children_.end(), child);
  DCHECK(i != children_.end());
  children_.erase(i);
  children_.push_back(child);
#if defined(USE_WEBKIT_COMPOSITOR)
  child->web_layer_.removeFromParent();
  web_layer_.addChild(child->web_layer_);
#endif
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
      animator_.reset(new LayerAnimationManager(this));
    animation->Start();
    animator_->SetAnimation(animation);
  } else {
    animator_.reset();
  }
}

void Layer::SetTransform(const ui::Transform& transform) {
  StopAnimatingIfNecessary(LayerAnimationManager::TRANSFORM);
  if (animator_.get() && animator_->IsRunning()) {
    animator_->AnimateTransform(transform);
    return;
  }
  SetTransformImmediately(transform);
}

void Layer::SetBounds(const gfx::Rect& bounds) {
  StopAnimatingIfNecessary(LayerAnimationManager::LOCATION);
  if (animator_.get() && animator_->IsRunning() &&
      bounds.size() == bounds_.size()) {
    animator_->AnimateToPoint(bounds.origin());
    return;
  }
  SetBoundsImmediately(bounds);
}

gfx::Rect Layer::GetTargetBounds() const {
  if (animator_.get() && animator_->IsRunning())
    return gfx::Rect(animator_->GetTargetPoint(), bounds_.size());
  return bounds_;
}

void Layer::SetOpacity(float opacity) {
  StopAnimatingIfNecessary(LayerAnimationManager::OPACITY);
  if (animator_.get() && animator_->IsRunning()) {
    animator_->AnimateOpacity(opacity);
    return;
  }
  SetOpacityImmediately(opacity);
}

void Layer::SetVisible(bool visible) {
  if (visible_ == visible)
    return;

  bool was_drawn = IsDrawn();
  visible_ = visible;
  bool is_drawn = IsDrawn();
  if (was_drawn == is_drawn)
    return;

  if (!is_drawn)
    DropTextures();
  SetNeedsToRecomputeHole();
#if defined(USE_WEBKIT_COMPOSITOR)
  // TODO(piman): Expose a visibility flag on WebLayer.
  web_layer_.setOpacity(visible_ ? opacity_ : 0.f);
#endif
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

  SetNeedsToRecomputeHole();
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
#if defined(USE_WEBKIT_COMPOSITOR)
  NOTREACHED();
#else
  DCHECK(GetCompositor());

  if (recompute_hole_ && !parent_)
    RecomputeHole();

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
  scoped_ptr<gfx::Canvas> canvas(gfx::Canvas::CreateCanvas(
      draw_rect.width(), draw_rect.height(), false));
  canvas->TranslateInt(-draw_rect.x(), -draw_rect.y());
  delegate_->OnPaintLayer(canvas.get());
  SetCanvas(*canvas->GetSkCanvas(), draw_rect.origin());
#endif
}

void Layer::SetNeedsToRecomputeHole() {
  Layer* root_layer = this;
  while (root_layer->parent_)
    root_layer = root_layer->parent_;

  root_layer->recompute_hole_ = true;
}

void Layer::ClearHoleRects() {
  hole_rect_ = gfx::Rect();

  for (size_t i = 0; i < children_.size(); i++)
    children_[i]->ClearHoleRects();
}

void Layer::GetLayerProperties(const ui::Transform& parent_transform,
                               std::vector<LayerProperties>* traversal) {
  if (!visible_ || opacity_ != 1.0f)
    return;

  ui::Transform current_transform;
  current_transform.ConcatTransform(parent_transform);
  if (transform().HasChange())
    current_transform.ConcatTransform(transform());
  current_transform.ConcatTranslate(
      static_cast<float>(bounds().x()),
      static_cast<float>(bounds().y()));

  if (fills_bounds_opaquely_ && type_ == LAYER_HAS_TEXTURE) {
    LayerProperties properties;
    properties.layer = this;
    properties.transform_relative_to_root = current_transform;
    traversal->push_back(properties);
  }

  for (size_t i = 0; i < children_.size(); i++)
    children_[i]->GetLayerProperties(current_transform, traversal);
}

void Layer::RecomputeHole() {
  std::vector<LayerProperties> traversal;
  ui::Transform transform;

  ClearHoleRects();
  GetLayerProperties(transform, &traversal);

  for (size_t i = 0; i < traversal.size(); i++) {
    Layer* layer = traversal[i].layer;
    gfx::Rect bounds = gfx::Rect(layer->bounds().size());

    // Iterate through layers which are after traversal[i] in draw order
    // and find the largest candidate hole.
    for (size_t j = i + 1; j < traversal.size(); j++) {
      gfx::Rect candidate_hole = gfx::Rect(traversal[j].layer->bounds().size());

      // Compute transform to go from bounds of layer |j| to local bounds of
      // layer |i|.
      ui::Transform candidate_hole_transform;
      ui::Transform inverted;

      candidate_hole_transform.ConcatTransform(
          traversal[j].transform_relative_to_root);

      if (!traversal[i].transform_relative_to_root.GetInverse(&inverted))
        continue;

      candidate_hole_transform.ConcatTransform(inverted);

      // cannot punch a hole if the relative transform between the two layers
      // is not multiple of 90.
      float degrees;
      gfx::Point p;
      if (!InterpolatedTransform::FactorTRS(candidate_hole_transform, &p,
          &degrees, NULL) || !IsApproximateMultilpleOf(degrees, 90.0f))
        continue;

      candidate_hole_transform.TransformRect(&candidate_hole);
      candidate_hole = candidate_hole.Intersect(bounds);

      if (candidate_hole.size().GetArea() > layer->hole_rect().size().GetArea())
        layer->set_hole_rect(candidate_hole);
    }
    // Free up texture memory if the hole fills bounds of layer.
    if (!layer->ShouldDraw() && !layer_updated_externally())
      layer->DropTexture();

#if defined(USE_WEBKIT_COMPOSITOR)
    layer->RecomputeDrawsContent();
#endif
  }

  recompute_hole_ = false;
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

void Layer::DropTexture() {
  if (!layer_updated_externally_)
    texture_ = NULL;
}

void Layer::DropTextures() {
  DropTexture();
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
    LayerAnimationManager::AnimationProperty property) {
  if (!animator_.get() || !animator_->IsRunning() ||
      !animator_->got_initial_tick()) {
    return;
  }

  if (property != LayerAnimationManager::LOCATION &&
      animator_->IsAnimating(LayerAnimationManager::LOCATION)) {
    SetBoundsImmediately(
        gfx::Rect(animator_->GetTargetPoint(), bounds_.size()));
  }
  if (property != LayerAnimationManager::OPACITY &&
      animator_->IsAnimating(LayerAnimationManager::OPACITY)) {
    SetOpacityImmediately(animator_->GetTargetOpacity());
  }
  if (property != LayerAnimationManager::TRANSFORM &&
      animator_->IsAnimating(LayerAnimationManager::TRANSFORM)) {
    SetTransformImmediately(animator_->GetTargetTransform());
  }
  animator_.reset();
}

void Layer::SetBoundsImmediately(const gfx::Rect& bounds) {
  bounds_ = bounds;

  SetNeedsToRecomputeHole();
#if defined(USE_WEBKIT_COMPOSITOR)
  web_layer_.setBounds(bounds.size());
  RecomputeTransform();
  RecomputeDrawsContent();
#endif
}

void Layer::SetTransformImmediately(const ui::Transform& transform) {
  transform_ = transform;

  SetNeedsToRecomputeHole();
#if defined(USE_WEBKIT_COMPOSITOR)
  RecomputeTransform();
#endif
}

void Layer::SetOpacityImmediately(float opacity) {
  opacity_ = opacity;
  SetNeedsToRecomputeHole();
#if defined(USE_WEBKIT_COMPOSITOR)
  if (visible_)
    web_layer_.setOpacity(opacity);
#endif
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
