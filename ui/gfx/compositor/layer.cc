// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/layer.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebContentLayer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebExternalTextureLayer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebFloatRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "ui/base/animation/animation.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/compositor/compositor_switches.h"
#include "ui/gfx/compositor/layer_animator.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/point3.h"

#include "ui/gfx/compositor/compositor_cc.h"

namespace {

const float EPSILON = 1e-3f;

bool IsApproximateMultilpleOf(float value, float base) {
  float remainder = fmod(fabs(value), base);
  return remainder < EPSILON || base - remainder < EPSILON;
}

const ui::Layer* GetRoot(const ui::Layer* layer) {
  return layer->parent() ? GetRoot(layer->parent()) : layer;
}

}  // namespace

namespace ui {

Layer::Layer()
    : type_(LAYER_HAS_TEXTURE),
      compositor_(NULL),
      parent_(NULL),
      visible_(true),
      fills_bounds_opaquely_(true),
      layer_updated_externally_(false),
      opacity_(1.0f),
      delegate_(NULL) {
  CreateWebLayer();
}

Layer::Layer(LayerType type)
    : type_(type),
      compositor_(NULL),
      parent_(NULL),
      visible_(true),
      fills_bounds_opaquely_(true),
      layer_updated_externally_(false),
      opacity_(1.0f),
      delegate_(NULL) {
  CreateWebLayer();
}

Layer::~Layer() {
  if (compositor_)
    compositor_->SetRootLayer(NULL);
  if (parent_)
    parent_->Remove(this);
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->parent_ = NULL;
  web_layer_.removeFromParent();
}

Compositor* Layer::GetCompositor() {
  return GetRoot(this)->compositor_;
}

void Layer::SetCompositor(Compositor* compositor) {
  // This function must only be called to set the compositor on the root layer,
  // or to reset it.
  DCHECK(!compositor || !compositor_);
  DCHECK(!compositor || compositor->root_layer() == this);
  DCHECK(!parent_);
  compositor_ = compositor;
}

void Layer::Add(Layer* child) {
  DCHECK(!child->compositor_);
  if (child->parent_)
    child->parent_->Remove(child);
  child->parent_ = this;
  children_.push_back(child);
  web_layer_.addChild(child->web_layer_);
}

void Layer::Remove(Layer* child) {
  std::vector<Layer*>::iterator i =
      std::find(children_.begin(), children_.end(), child);
  DCHECK(i != children_.end());
  children_.erase(i);
  child->parent_ = NULL;
  child->web_layer_.removeFromParent();
}

void Layer::StackAtTop(Layer* child) {
  if (children_.size() <= 1 || child == children_.back())
    return;  // Already in front.
  StackAbove(child, children_.back());
}

void Layer::StackAbove(Layer* child, Layer* other) {
  StackRelativeTo(child, other, true);
}

void Layer::StackAtBottom(Layer* child) {
  if (children_.size() <= 1 || child == children_.front())
    return;  // Already on bottom.
  StackBelow(child, children_.front());
}

void Layer::StackBelow(Layer* child, Layer* other) {
  StackRelativeTo(child, other, false);
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

  visible_ = visible;
  // TODO(piman): Expose a visibility flag on WebLayer.
  web_layer_.setOpacity(visible_ ? opacity_ : 0.f);
}

bool Layer::IsDrawn() const {
  const Layer* layer = this;
  while (layer && layer->visible_)
    layer = layer->parent_;
  return layer == NULL;
}

bool Layer::ShouldDraw() const {
  return type_ == LAYER_HAS_TEXTURE && GetCombinedOpacity() > 0.0f;
}

// static
void Layer::ConvertPointToLayer(const Layer* source,
                                const Layer* target,
                                gfx::Point* point) {
  if (source == target)
    return;

  const Layer* root_layer = GetRoot(source);
  CHECK_EQ(root_layer, GetRoot(target));

  if (source != root_layer)
    source->ConvertPointForAncestor(root_layer, point);
  if (target != root_layer)
    target->ConvertPointFromAncestor(root_layer, point);
}

void Layer::SetFillsBoundsOpaquely(bool fills_bounds_opaquely) {
  if (fills_bounds_opaquely_ == fills_bounds_opaquely)
    return;

  fills_bounds_opaquely_ = fills_bounds_opaquely;

  web_layer_.setOpaque(fills_bounds_opaquely);
  RecomputeDebugBorderColor();
}

void Layer::SetExternalTexture(ui::Texture* texture) {
  layer_updated_externally_ = !!texture;
  texture_ = texture;
  if (web_layer_is_accelerated_ != layer_updated_externally_) {
    // Switch to a different type of layer.
    web_layer_.removeAllChildren();
    WebKit::WebLayer new_layer;
    if (layer_updated_externally_)
      new_layer = WebKit::WebExternalTextureLayer::create();
    else
      new_layer = WebKit::WebContentLayer::create(this);
    if (parent_) {
      DCHECK(!parent_->web_layer_.isNull());
      parent_->web_layer_.replaceChild(web_layer_, new_layer);
    }
    web_layer_ = new_layer;
    web_layer_is_accelerated_ = layer_updated_externally_;
    for (size_t i = 0; i < children_.size(); ++i) {
      DCHECK(!children_[i]->web_layer_.isNull());
      web_layer_.addChild(children_[i]->web_layer_);
    }
    web_layer_.setAnchorPoint(WebKit::WebFloatPoint(0.f, 0.f));
    web_layer_.setOpaque(fills_bounds_opaquely_);
    web_layer_.setOpacity(visible_ ? opacity_ : 0.f);
    web_layer_.setDebugBorderWidth(show_debug_borders_ ? 2 : 0);
    RecomputeTransform();
    RecomputeDebugBorderColor();
  }
  if (texture) {
    TextureCC* texture_cc = static_cast<TextureCC*>(texture);
    texture_cc->Update();
    WebKit::WebExternalTextureLayer texture_layer =
        web_layer_.to<WebKit::WebExternalTextureLayer>();
    texture_layer.setFlipped(texture_cc->flipped());
  }
  RecomputeDrawsContentAndUVRect();
}

void Layer::SetCanvas(const SkCanvas& canvas, const gfx::Point& origin) {
  NOTREACHED();
}

void Layer::SchedulePaint(const gfx::Rect& invalid_rect) {
  WebKit::WebFloatRect web_rect(
      invalid_rect.x(),
      invalid_rect.y(),
      invalid_rect.width(),
      invalid_rect.height());
  if (!web_layer_is_accelerated_)
    web_layer_.to<WebKit::WebContentLayer>().invalidateRect(web_rect);
  else
    web_layer_.to<WebKit::WebExternalTextureLayer>().invalidateRect(web_rect);
}

void Layer::ScheduleDraw() {
  Compositor* compositor = GetCompositor();
  if (compositor)
    compositor->ScheduleDraw();
}

void Layer::paintContents(WebKit::WebCanvas* web_canvas,
                          const WebKit::WebRect& clip) {
  TRACE_EVENT0("ui", "Layer::paintContents");
  gfx::CanvasSkia canvas(web_canvas);
  if (delegate_)
    delegate_->OnPaintLayer(&canvas);
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

void Layer::StackRelativeTo(Layer* child, Layer* other, bool above) {
  DCHECK_NE(child, other);
  DCHECK_EQ(this, child->parent());
  DCHECK_EQ(this, other->parent());

  const size_t child_i =
      std::find(children_.begin(), children_.end(), child) - children_.begin();
  const size_t other_i =
      std::find(children_.begin(), children_.end(), other) - children_.begin();
  if ((above && child_i == other_i + 1) || (!above && child_i + 1 == other_i))
    return;

  const size_t dest_i =
      above ?
      (child_i < other_i ? other_i : other_i + 1) :
      (child_i < other_i ? other_i - 1 : other_i);
  children_.erase(children_.begin() + child_i);
  children_.insert(children_.begin() + dest_i, child);

  child->web_layer_.removeFromParent();
  web_layer_.insertChild(child->web_layer_, dest_i);
}

void Layer::GetLayerProperties(const ui::Transform& parent_transform,
                               std::vector<LayerProperties>* traversal) {
  if (!visible_ || opacity_ != 1.0f)
    return;

  ui::Transform current_transform;
  if (transform().HasChange())
    current_transform.ConcatTransform(transform());
  current_transform.ConcatTranslate(
      static_cast<float>(bounds().x()),
      static_cast<float>(bounds().y()));
  current_transform.ConcatTransform(parent_transform);

  if (fills_bounds_opaquely_ && type_ == LAYER_HAS_TEXTURE) {
    LayerProperties properties;
    properties.layer = this;
    properties.transform_relative_to_root = current_transform;
    traversal->push_back(properties);
  }

  for (size_t i = 0; i < children_.size(); i++)
    children_[i]->GetLayerProperties(current_transform, traversal);
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

  RecomputeTransform();
  RecomputeDrawsContentAndUVRect();
}

void Layer::SetTransformImmediately(const ui::Transform& transform) {
  transform_ = transform;

  RecomputeTransform();
}

void Layer::SetOpacityImmediately(float opacity) {
  bool schedule_draw = (opacity != opacity_ && IsDrawn());
  opacity_ = opacity;

  if (visible_)
    web_layer_.setOpacity(opacity);
  RecomputeDebugBorderColor();
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

void Layer::CreateWebLayer() {
  web_layer_ = WebKit::WebContentLayer::create(this);
  web_layer_.setAnchorPoint(WebKit::WebFloatPoint(0.f, 0.f));
  web_layer_.setOpaque(true);
  web_layer_is_accelerated_ = false;
  show_debug_borders_ = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUIShowLayerBorders);
  web_layer_.setDebugBorderWidth(show_debug_borders_ ? 2 : 0);
  RecomputeDrawsContentAndUVRect();
  RecomputeDebugBorderColor();
}

void Layer::RecomputeTransform() {
  ui::Transform transform = transform_;
  transform.ConcatTranslate(bounds_.x(), bounds_.y());
  web_layer_.setTransform(transform.matrix());
}

void Layer::RecomputeDrawsContentAndUVRect() {
  DCHECK(!web_layer_.isNull());
  bool should_draw = type_ == LAYER_HAS_TEXTURE;
  if (!web_layer_is_accelerated_) {
    web_layer_.to<WebKit::WebContentLayer>().setDrawsContent(should_draw);
    web_layer_.setBounds(bounds_.size());
  } else {
    DCHECK(texture_);
    TextureCC* texture_cc = static_cast<TextureCC*>(texture_.get());
    unsigned int texture_id = texture_cc->texture_id();
    WebKit::WebExternalTextureLayer texture_layer =
        web_layer_.to<WebKit::WebExternalTextureLayer>();
    texture_layer.setTextureId(should_draw ? texture_id : 0);
    gfx::Size size(std::min(bounds_.width(), texture_cc->size().width()),
                   std::min(bounds_.height(), texture_cc->size().height()));
    WebKit::WebFloatRect rect(
        0,
        0,
        static_cast<float>(size.width())/texture_cc->size().width(),
        static_cast<float>(size.height())/texture_cc->size().height());
    texture_layer.setUVRect(rect);
    web_layer_.setBounds(size);
  }
}

void Layer::RecomputeDebugBorderColor() {
  if (!show_debug_borders_)
    return;
  unsigned int color = 0xFF000000;
  color |= web_layer_is_accelerated_ ? 0x0000FF00 : 0x00FF0000;
  bool opaque = fills_bounds_opaquely_ && (GetCombinedOpacity() == 1.f);
  if (!opaque)
    color |= 0xFF;
  web_layer_.setDebugBorderColor(color);
}

}  // namespace ui
