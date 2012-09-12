// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/Platform/chromium/public/Platform.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCompositorSupport.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebContentLayer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebExternalTextureLayer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFilterOperation.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFilterOperations.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFloatPoint.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFloatRect.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSolidColorLayer.h"
#include "ui/base/animation/animation.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/display.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/point3.h"

namespace {

const float EPSILON = 1e-3f;

bool IsApproximateMultipleOf(float value, float base) {
  float remainder = fmod(fabs(value), base);
  return remainder < EPSILON || base - remainder < EPSILON;
}

const ui::Layer* GetRoot(const ui::Layer* layer) {
  return layer->parent() ? GetRoot(layer->parent()) : layer;
}

}  // namespace

namespace ui {

Layer::Layer()
    : type_(LAYER_TEXTURED),
      compositor_(NULL),
      parent_(NULL),
      visible_(true),
      force_render_surface_(false),
      fills_bounds_opaquely_(true),
      layer_updated_externally_(false),
      opacity_(1.0f),
      background_blur_radius_(0),
      layer_saturation_(0.0f),
      layer_brightness_(0.0f),
      layer_grayscale_(0.0f),
      layer_inverted_(false),
      layer_mask_(NULL),
      layer_mask_back_link_(NULL),
      delegate_(NULL),
      web_layer_(NULL),
      scale_content_(true),
      device_scale_factor_(1.0f) {
  CreateWebLayer();
}

Layer::Layer(LayerType type)
    : type_(type),
      compositor_(NULL),
      parent_(NULL),
      visible_(true),
      force_render_surface_(false),
      fills_bounds_opaquely_(true),
      layer_updated_externally_(false),
      opacity_(1.0f),
      background_blur_radius_(0),
      layer_saturation_(0.0f),
      layer_brightness_(0.0f),
      layer_grayscale_(0.0f),
      layer_inverted_(false),
      layer_mask_(NULL),
      layer_mask_back_link_(NULL),
      delegate_(NULL),
      scale_content_(true),
      device_scale_factor_(1.0f) {
  CreateWebLayer();
}

Layer::~Layer() {
  // Destroying the animator may cause observers to use the layer (and
  // indirectly the WebLayer). Destroy the animator first so that the WebLayer
  // is still around.
  if (animator_)
    animator_->SetDelegate(NULL);
  animator_ = NULL;
  if (compositor_)
    compositor_->SetRootLayer(NULL);
  if (parent_)
    parent_->Remove(this);
  if (layer_mask_)
    SetMaskLayer(NULL);
  if (layer_mask_back_link_)
    layer_mask_back_link_->SetMaskLayer(NULL);
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->parent_ = NULL;
  web_layer_->removeFromParent();
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
  if (compositor)
    OnDeviceScaleFactorChanged(compositor->device_scale_factor());
}

void Layer::Add(Layer* child) {
  DCHECK(!child->compositor_);
  if (child->parent_)
    child->parent_->Remove(child);
  child->parent_ = this;
  children_.push_back(child);
  web_layer_->addChild(child->web_layer_);
  child->OnDeviceScaleFactorChanged(device_scale_factor_);
}

void Layer::Remove(Layer* child) {
  std::vector<Layer*>::iterator i =
      std::find(children_.begin(), children_.end(), child);
  DCHECK(i != children_.end());
  children_.erase(i);
  child->parent_ = NULL;
  child->web_layer_->removeFromParent();
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
  animator_ = animator;
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
  if (animator_.get() && animator_->IsAnimatingProperty(
      LayerAnimationElement::TRANSFORM)) {
    return animator_->GetTargetTransform();
  }
  return transform_;
}

void Layer::SetBounds(const gfx::Rect& bounds) {
  GetAnimator()->SetBounds(bounds);
}

gfx::Rect Layer::GetTargetBounds() const {
  if (animator_.get() && animator_->IsAnimatingProperty(
      LayerAnimationElement::BOUNDS)) {
    return animator_->GetTargetBounds();
  }
  return bounds_;
}

void Layer::SetMasksToBounds(bool masks_to_bounds) {
  web_layer_->setMasksToBounds(masks_to_bounds);
}

bool Layer::GetMasksToBounds() const {
  return web_layer_->masksToBounds();
}

void Layer::SetOpacity(float opacity) {
  GetAnimator()->SetOpacity(opacity);
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

void Layer::SetBackgroundBlur(int blur_radius) {
  background_blur_radius_ = blur_radius;

  WebKit::WebFilterOperations filters;
  if (background_blur_radius_) {
    filters.append(WebKit::WebFilterOperation::createBlurFilter(
        background_blur_radius_));
  }
  web_layer_->setBackgroundFilters(filters);
}

void Layer::SetLayerSaturation(float saturation) {
  layer_saturation_ = saturation;
  SetLayerFilters();
}

void Layer::SetLayerBrightness(float brightness) {
  GetAnimator()->SetBrightness(brightness);
}

float Layer::GetTargetBrightness() const {
  if (animator_.get() && animator_->IsAnimatingProperty(
      LayerAnimationElement::BRIGHTNESS)) {
    return animator_->GetTargetBrightness();
  }
  return layer_brightness();
}

void Layer::SetLayerGrayscale(float grayscale) {
  GetAnimator()->SetGrayscale(grayscale);
}

float Layer::GetTargetGrayscale() const {
  if (animator_.get() && animator_->IsAnimatingProperty(
      LayerAnimationElement::GRAYSCALE)) {
    return animator_->GetTargetGrayscale();
  }
  return layer_grayscale();
}

void Layer::SetLayerInverted(bool inverted) {
  layer_inverted_ = inverted;
  SetLayerFilters();
}

void Layer::SetMaskLayer(Layer* layer_mask) {
  // The provided mask should not have a layer mask itself.
  DCHECK(!layer_mask ||
         (!layer_mask->layer_mask_layer() &&
          layer_mask->children().empty() &&
          !layer_mask->layer_mask_back_link_));
  DCHECK(!layer_mask_back_link_);
  if (layer_mask_ == layer_mask)
    return;
  // We need to de-reference the currently linked object so that no problem
  // arises if the mask layer gets deleted before this object.
  if (layer_mask_)
    layer_mask_->layer_mask_back_link_ = NULL;
  layer_mask_ = layer_mask;
  web_layer_->setMaskLayer(
      layer_mask ? layer_mask->web_layer() : NULL);
  // We need to reference the linked object so that it can properly break the
  // link to us when it gets deleted.
  if (layer_mask)
    layer_mask->layer_mask_back_link_ = this;
}

void Layer::SetLayerFilters() {
  WebKit::WebFilterOperations filters;
  if (layer_saturation_) {
    filters.append(WebKit::WebFilterOperation::createSaturateFilter(
        layer_saturation_));
  }
  if (layer_grayscale_) {
    filters.append(WebKit::WebFilterOperation::createGrayscaleFilter(
        layer_grayscale_));
  }
  if (layer_inverted_)
    filters.append(WebKit::WebFilterOperation::createInvertFilter(1.0));
  // Brightness goes last, because the resulting colors neeed clamping, which
  // cause further color matrix filters to be applied separately. In this order,
  // they all can be combined in a single pass.
  if (layer_brightness_) {
    filters.append(WebKit::WebFilterOperation::createBrightnessFilter(
        layer_brightness_));
  }

  web_layer_->setFilters(filters);
}

float Layer::GetTargetOpacity() const {
  if (animator_.get() && animator_->IsAnimatingProperty(
      LayerAnimationElement::OPACITY))
    return animator_->GetTargetOpacity();
  return opacity_;
}

void Layer::SetVisible(bool visible) {
  GetAnimator()->SetVisibility(visible);
}

bool Layer::GetTargetVisibility() const {
  if (animator_.get() && animator_->IsAnimatingProperty(
      LayerAnimationElement::VISIBILITY))
    return animator_->GetTargetVisibility();
  return visible_;
}

bool Layer::IsDrawn() const {
  const Layer* layer = this;
  while (layer && layer->visible_)
    layer = layer->parent_;
  return layer == NULL;
}

bool Layer::ShouldDraw() const {
  return type_ != LAYER_NOT_DRAWN && GetCombinedOpacity() > 0.0f;
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

  web_layer_->setOpaque(fills_bounds_opaquely);
  RecomputeDebugBorderColor();
}

void Layer::SetExternalTexture(Texture* texture) {
  DCHECK_EQ(type_, LAYER_TEXTURED);
  layer_updated_externally_ = !!texture;
  texture_ = texture;
  if (web_layer_is_accelerated_ != layer_updated_externally_) {
    // Switch to a different type of layer.
    web_layer_->removeAllChildren();
    scoped_ptr<WebKit::WebContentLayer> old_content_layer(
        content_layer_.release());
    scoped_ptr<WebKit::WebSolidColorLayer> old_solid_layer(
        solid_color_layer_.release());
    scoped_ptr<WebKit::WebExternalTextureLayer> old_texture_layer(
        texture_layer_.release());
    WebKit::WebLayer* new_layer = NULL;
    WebKit::WebCompositorSupport* compositor_support =
        WebKit::Platform::current()->compositorSupport();
    if (layer_updated_externally_) {
      texture_layer_.reset(
          compositor_support->createExternalTextureLayer(this));
      texture_layer_->setFlipped(texture_->flipped());
      new_layer = texture_layer_->layer();
    } else {
      old_texture_layer->willModifyTexture();
      content_layer_.reset(compositor_support->createContentLayer(this));
      new_layer = content_layer_->layer();
    }
    if (parent_) {
      DCHECK(parent_->web_layer_);
      parent_->web_layer_->replaceChild(web_layer_, new_layer);
    }
    web_layer_= new_layer;
    web_layer_is_accelerated_ = layer_updated_externally_;
    for (size_t i = 0; i < children_.size(); ++i) {
      DCHECK(children_[i]->web_layer_);
      web_layer_->addChild(children_[i]->web_layer_);
    }
    web_layer_->setAnchorPoint(WebKit::WebFloatPoint(0.f, 0.f));
    web_layer_->setOpaque(fills_bounds_opaquely_);
    web_layer_->setOpacity(visible_ ? opacity_ : 0.f);
    web_layer_->setDebugBorderWidth(show_debug_borders_ ? 2 : 0);
    web_layer_->setForceRenderSurface(force_render_surface_);
    RecomputeTransform();
    RecomputeDebugBorderColor();
  }
  RecomputeDrawsContentAndUVRect();
}

void Layer::SetColor(SkColor color) {
  DCHECK_EQ(type_, LAYER_SOLID_COLOR);
  // WebColor is equivalent to SkColor, per WebColor.h.
  solid_color_layer_->setBackgroundColor(static_cast<WebKit::WebColor>(color));
  SetFillsBoundsOpaquely(SkColorGetA(color) == 0xFF);
}

bool Layer::SchedulePaint(const gfx::Rect& invalid_rect) {
  if (type_ == LAYER_SOLID_COLOR || (!delegate_ && !texture_))
    return false;

  damaged_region_.op(invalid_rect.x(),
                     invalid_rect.y(),
                     invalid_rect.right(),
                     invalid_rect.bottom(),
                     SkRegion::kUnion_Op);
  ScheduleDraw();
  return true;
}

void Layer::ScheduleDraw() {
  Compositor* compositor = GetCompositor();
  if (compositor)
    compositor->ScheduleDraw();
}

void Layer::SendDamagedRects() {
  if ((delegate_ || texture_) && !damaged_region_.isEmpty()) {
    for (SkRegion::Iterator iter(damaged_region_);
         !iter.done(); iter.next()) {
      const SkIRect& sk_damaged = iter.rect();
      gfx::Rect damaged(
          sk_damaged.x(),
          sk_damaged.y(),
          sk_damaged.width(),
          sk_damaged.height());

      if (scale_content_ && web_layer_is_accelerated_) {
        damaged.Inset(-1, -1);
        damaged = damaged.Intersect(gfx::Rect(bounds_.size()));
      }

      gfx::Rect damaged_in_pixel = ConvertRectToPixel(this, damaged);
      WebKit::WebFloatRect web_rect(
          damaged_in_pixel.x(),
          damaged_in_pixel.y(),
          damaged_in_pixel.width(),
          damaged_in_pixel.height());
      web_layer_->invalidateRect(web_rect);
    }
    damaged_region_.setEmpty();
  }
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->SendDamagedRects();
}

void Layer::SuppressPaint() {
  if (!delegate_)
    return;
  delegate_ = NULL;
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->SuppressPaint();
}

void Layer::OnDeviceScaleFactorChanged(float device_scale_factor) {
  if (device_scale_factor_ == device_scale_factor)
    return;
  device_scale_factor_ = device_scale_factor;
  RecomputeTransform();
  RecomputeDrawsContentAndUVRect();
  SchedulePaint(gfx::Rect(bounds_.size()));
  if (delegate_)
    delegate_->OnDeviceScaleFactorChanged(device_scale_factor);
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->OnDeviceScaleFactorChanged(device_scale_factor);
}

void Layer::paintContents(WebKit::WebCanvas* web_canvas,
                          const WebKit::WebRect& clip,
                          WebKit::WebFloatRect& opaque) {
  TRACE_EVENT0("ui", "Layer::paintContents");
  scoped_ptr<gfx::Canvas> canvas(gfx::Canvas::CreateCanvasWithoutScaling(
      web_canvas, ui::GetScaleFactorFromScale(device_scale_factor_)));

  if (scale_content_) {
    canvas->Save();
    canvas->sk_canvas()->scale(SkFloatToScalar(device_scale_factor_),
                               SkFloatToScalar(device_scale_factor_));
  }

  if (delegate_)
    delegate_->OnPaintLayer(canvas.get());
  if (scale_content_)
    canvas->Restore();
}

unsigned Layer::prepareTexture(WebKit::WebTextureUpdater& /* updater */) {
  DCHECK(layer_updated_externally_);
  return texture_->texture_id();
}

WebKit::WebGraphicsContext3D* Layer::context() {
  DCHECK(layer_updated_externally_);
  return texture_->HostContext3D();
}

void Layer::SetForceRenderSurface(bool force) {
  if (force_render_surface_ == force)
    return;

  force_render_surface_ = force;
  web_layer_->setForceRenderSurface(force_render_surface_);
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

  child->web_layer_->removeFromParent();
  web_layer_->insertChild(child->web_layer_, dest_i);
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

  base::Closure closure;
  if (delegate_)
    closure = delegate_->PrepareForLayerBoundsChange();
  bool was_move = bounds_.size() == bounds.size();
  bounds_ = bounds;

  RecomputeTransform();
  RecomputeDrawsContentAndUVRect();
  if (!closure.is_null())
    closure.Run();

  if (was_move) {
    // Don't schedule a draw if we're invisible. We'll schedule one
    // automatically when we get visible.
    if (IsDrawn())
      ScheduleDraw();
  } else {
    // Always schedule a paint, even if we're invisible.
    SchedulePaint(gfx::Rect(bounds.size()));
  }
}

void Layer::SetTransformImmediately(const ui::Transform& transform) {
  transform_ = transform;

  RecomputeTransform();
}

void Layer::SetOpacityImmediately(float opacity) {
  bool schedule_draw = (opacity != opacity_ && IsDrawn());
  opacity_ = opacity;

  if (visible_)
    web_layer_->setOpacity(opacity);
  RecomputeDebugBorderColor();
  if (schedule_draw)
    ScheduleDraw();
}

void Layer::SetVisibilityImmediately(bool visible) {
  if (visible_ == visible)
    return;

  visible_ = visible;
  // TODO(piman): Expose a visibility flag on WebLayer.
  web_layer_->setOpacity(visible_ ? opacity_ : 0.f);
}

void Layer::SetBrightnessImmediately(float brightness) {
  layer_brightness_ = brightness;
  SetLayerFilters();
}

void Layer::SetGrayscaleImmediately(float grayscale) {
  layer_grayscale_ = grayscale;
  SetLayerFilters();
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

void Layer::SetVisibilityFromAnimation(bool visibility) {
  SetVisibilityImmediately(visibility);
}

void Layer::SetBrightnessFromAnimation(float brightness) {
  SetBrightnessImmediately(brightness);
}
void Layer::SetGrayscaleFromAnimation(float grayscale) {
  SetGrayscaleImmediately(grayscale);
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

bool Layer::GetVisibilityForAnimation() const {
  return visible();
}

float Layer::GetBrightnessForAnimation() const {
  return layer_brightness();
}

float Layer::GetGrayscaleForAnimation() const {
  return layer_grayscale();
}

void Layer::CreateWebLayer() {
  WebKit::WebCompositorSupport* compositor_support =
      WebKit::Platform::current()->compositorSupport();
  if (type_ == LAYER_SOLID_COLOR) {
    solid_color_layer_.reset(compositor_support->createSolidColorLayer());
    web_layer_ = solid_color_layer_->layer();
  } else {
    content_layer_.reset(compositor_support->createContentLayer(this));
    web_layer_ = content_layer_->layer();
  }
  web_layer_is_accelerated_ = false;
  show_debug_borders_ = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUIShowLayerBorders);
  web_layer_->setAnchorPoint(WebKit::WebFloatPoint(0.f, 0.f));
  web_layer_->setOpaque(true);
  web_layer_->setDebugBorderWidth(show_debug_borders_ ? 2 : 0);
}

void Layer::RecomputeTransform() {
  ui::Transform scale_translate;
  scale_translate.matrix().set3x3(device_scale_factor_, 0, 0,
                                  0, device_scale_factor_, 0,
                                  0, 0, 1);
  // Start with the inverse matrix of above.
  Transform transform;
  transform.matrix().set3x3(1.0f / device_scale_factor_, 0, 0,
                            0, 1.0f / device_scale_factor_, 0,
                            0, 0, 1);
  transform.ConcatTransform(transform_);
  transform.ConcatTranslate(bounds_.x(), bounds_.y());
  transform.ConcatTransform(scale_translate);
  web_layer_->setTransform(transform.matrix());
}

void Layer::RecomputeDrawsContentAndUVRect() {
  DCHECK(web_layer_);
  bool should_draw = type_ != LAYER_NOT_DRAWN;
  if (!web_layer_is_accelerated_) {
    if (type_ != LAYER_SOLID_COLOR) {
      web_layer_->setDrawsContent(should_draw);
    }
    web_layer_->setBounds(ConvertSizeToPixel(this, bounds_.size()));
  } else {
    DCHECK(texture_);

    gfx::Size texture_size;
    if (scale_content_)
      texture_size = texture_->size();
    else
      texture_size = ConvertSizeToDIP(this, texture_->size());

    gfx::Size size(std::min(bounds().width(), texture_size.width()),
                   std::min(bounds().height(), texture_size.height()));
    WebKit::WebFloatRect rect(
        0,
        0,
        static_cast<float>(size.width())/texture_size.width(),
        static_cast<float>(size.height())/texture_size.height());
    texture_layer_->setUVRect(rect);

    gfx::Size size_in_pixel = ConvertSizeToPixel(this, size);
    web_layer_->setBounds(size_in_pixel);
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
  web_layer_->setDebugBorderColor(color);
}

}  // namespace ui
