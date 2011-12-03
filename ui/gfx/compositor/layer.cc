// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/layer.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContentLayer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebExternalTextureLayer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebFloatRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "ui/base/animation/animation.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/compositor/compositor_switches.h"
#include "ui/gfx/compositor/layer_animator.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/point3.h"

#if defined(USE_WEBKIT_COMPOSITOR)
#include "ui/gfx/compositor/compositor_cc.h"
#endif

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
      recompute_hole_(false),
      layer_updated_externally_(false),
      opacity_(1.0f),
      delegate_(NULL) {
#if defined(USE_WEBKIT_COMPOSITOR)
  CreateWebLayer();
#endif
}

Layer::Layer(LayerType type)
    : type_(type),
      compositor_(NULL),
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
  if (compositor_)
    compositor_->SetRootLayer(NULL);
  if (parent_)
    parent_->Remove(this);
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->parent_ = NULL;
#if defined(USE_WEBKIT_COMPOSITOR)
  web_layer_.removeFromParent();
#endif
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

void Layer::StackAtTop(Layer* child) {
  if (children_.size() <= 1 || child == children_.back())
    return;  // Already in front.
  StackAbove(child, children_.back());

  SetNeedsToRecomputeHole();
}

void Layer::StackAbove(Layer* child, Layer* other) {
  DCHECK_NE(child, other);
  DCHECK_EQ(this, child->parent());
  DCHECK_EQ(this, other->parent());

  const size_t child_i =
      std::find(children_.begin(), children_.end(), child) - children_.begin();
  const size_t other_i =
      std::find(children_.begin(), children_.end(), other) - children_.begin();
  if (child_i == other_i + 1)
    return;

  const size_t dest_i = child_i < other_i ? other_i : other_i + 1;
  children_.erase(children_.begin() + child_i);
  children_.insert(children_.begin() + dest_i, child);

  SetNeedsToRecomputeHole();

#if defined(USE_WEBKIT_COMPOSITOR)
  child->web_layer_.removeFromParent();
  web_layer_.insertChild(child->web_layer_, dest_i);
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
  SetNeedsToRecomputeHole();
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

  SetNeedsToRecomputeHole();
#if defined(USE_WEBKIT_COMPOSITOR)
  web_layer_.setOpaque(fills_bounds_opaquely);
  RecomputeDebugBorderColor();
#endif
}

void Layer::SetExternalTexture(ui::Texture* texture) {
  layer_updated_externally_ = !!texture;
  texture_ = texture;
#if defined(USE_WEBKIT_COMPOSITOR)
  if (web_layer_is_accelerated_ != layer_updated_externally_) {
    // Switch to a different type of layer.
    web_layer_.removeAllChildren();
    WebKit::WebLayer new_layer;
    if (layer_updated_externally_)
      new_layer = WebKit::WebExternalTextureLayer::create(this);
    else
      new_layer = WebKit::WebContentLayer::create(this, this);
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

      if (candidate_hole.size().GetArea() >
          layer->hole_rect().size().GetArea()) {
        layer->set_hole_rect(candidate_hole);
      }
    }
    // Free up texture memory if the hole fills bounds of layer.
    if (!layer->ShouldDraw() && !layer_updated_externally())
      layer->DropTexture();

#if defined(USE_WEBKIT_COMPOSITOR)
  RecomputeDrawsContentAndUVRect();
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

  SetNeedsToRecomputeHole();
#if defined(USE_WEBKIT_COMPOSITOR)
  RecomputeTransform();
  RecomputeDrawsContentAndUVRect();
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
  bool schedule_draw = (opacity != opacity_ && IsDrawn());
  bool was_opaque = GetCombinedOpacity() == 1.0f;
  opacity_ = opacity;
  bool is_opaque = GetCombinedOpacity() == 1.0f;

  // If our opacity has changed we need to recompute our hole, our parent's hole
  // and the holes of all our descendants.
  if (was_opaque != is_opaque)
    SetNeedsToRecomputeHole();
#if defined(USE_WEBKIT_COMPOSITOR)
  if (visible_)
    web_layer_.setOpacity(opacity);
  RecomputeDebugBorderColor();
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
  bool should_draw = type_ == LAYER_HAS_TEXTURE &&
      !hole_rect_.Contains(gfx::Rect(gfx::Point(0, 0), bounds_.size()));
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
#endif

}  // namespace ui
