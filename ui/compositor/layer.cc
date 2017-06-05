// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/filter_operation.h"
#include "cc/base/filter_operations.h"
#include "cc/layers/nine_patch_layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/surface_layer.h"
#include "cc/layers/texture_layer.h"
#include "cc/output/copy_output_request.h"
#include "cc/resources/transferable_resource.h"
#include "cc/trees/layer_tree_settings.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_observer.h"
#include "ui/compositor/paint_context.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/interpolated_transform.h"

namespace {

const ui::Layer* GetRoot(const ui::Layer* layer) {
  while (layer->parent())
    layer = layer->parent();
  return layer;
}

}  // namespace

namespace ui {

class Layer::LayerMirror : public LayerDelegate, LayerObserver {
 public:
  LayerMirror(Layer* source, Layer* dest)
      : source_(source), dest_(dest) {
    dest->AddObserver(this);
    dest->set_delegate(this);
  }

  ~LayerMirror() override {
    dest_->RemoveObserver(this);
    dest_->set_delegate(nullptr);
  }

  Layer* dest() { return dest_; }

  // LayerDelegate:
  void OnPaintLayer(const PaintContext& context) override {
    if (auto* delegate = source_->delegate())
      delegate->OnPaintLayer(context);
  }
  void OnDelegatedFrameDamage(const gfx::Rect& damage_rect_in_dip) override {}
  void OnDeviceScaleFactorChanged(float device_scale_factor) override {}

  // LayerObserver:
  void LayerDestroyed(Layer* layer) override {
    DCHECK_EQ(dest_, layer);
    source_->OnMirrorDestroyed(this);
  }

 private:
  Layer* const source_;
  Layer* const dest_;

  DISALLOW_COPY_AND_ASSIGN(LayerMirror);
};

Layer::Layer()
    : type_(LAYER_TEXTURED),
      compositor_(NULL),
      parent_(NULL),
      visible_(true),
      fills_bounds_opaquely_(true),
      fills_bounds_completely_(false),
      background_blur_radius_(0),
      layer_saturation_(0.0f),
      layer_brightness_(0.0f),
      layer_grayscale_(0.0f),
      layer_inverted_(false),
      layer_temperature_(0.0f),
      layer_blue_scale_(1.0f),
      layer_green_scale_(1.0f),
      layer_mask_(NULL),
      layer_mask_back_link_(NULL),
      zoom_(1),
      zoom_inset_(0),
      delegate_(NULL),
      owner_(NULL),
      cc_layer_(NULL),
      device_scale_factor_(1.0f) {
  CreateCcLayer();
}

Layer::Layer(LayerType type)
    : type_(type),
      compositor_(NULL),
      parent_(NULL),
      visible_(true),
      fills_bounds_opaquely_(true),
      fills_bounds_completely_(false),
      background_blur_radius_(0),
      layer_saturation_(0.0f),
      layer_brightness_(0.0f),
      layer_grayscale_(0.0f),
      layer_inverted_(false),
      layer_temperature_(0.0f),
      layer_blue_scale_(1.0f),
      layer_green_scale_(1.0f),
      layer_mask_(NULL),
      layer_mask_back_link_(NULL),
      zoom_(1),
      zoom_inset_(0),
      delegate_(NULL),
      owner_(NULL),
      cc_layer_(NULL),
      device_scale_factor_(1.0f) {
  CreateCcLayer();
}

Layer::~Layer() {
  for (auto& observer : observer_list_)
    observer.LayerDestroyed(this);

  // Destroying the animator may cause observers to use the layer (and
  // indirectly the WebLayer). Destroy the animator first so that the WebLayer
  // is still around.
  SetAnimator(nullptr);
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

  cc_layer_->RemoveFromParent();
  if (mailbox_release_callback_)
    mailbox_release_callback_->Run(gpu::SyncToken(), false);
}

std::unique_ptr<Layer> Layer::Clone() const {
  auto clone = base::MakeUnique<Layer>(type_);

  // Background filters.
  clone->SetBackgroundBlur(background_blur_radius_);
  clone->SetBackgroundZoom(zoom_, zoom_inset_);

  // Filters.
  clone->SetLayerTemperature(GetTargetTemperature());
  clone->SetLayerSaturation(layer_saturation_);
  clone->SetLayerBrightness(GetTargetBrightness());
  clone->SetLayerGrayscale(GetTargetGrayscale());
  clone->SetLayerInverted(layer_inverted_);
  if (alpha_shape_)
    clone->SetAlphaShape(base::MakeUnique<SkRegion>(*alpha_shape_));

  // cc::Layer state.
  if (surface_layer_ && surface_layer_->primary_surface_info().is_valid()) {
    clone->SetShowPrimarySurface(surface_layer_->primary_surface_info(),
                                 surface_layer_->surface_reference_factory());
  } else if (type_ == LAYER_SOLID_COLOR) {
    clone->SetColor(GetTargetColor());
  }

  clone->SetTransform(GetTargetTransform());
  clone->SetBounds(bounds_);
  clone->SetSubpixelPositionOffset(subpixel_position_offset_);
  clone->SetMasksToBounds(GetMasksToBounds());
  clone->SetOpacity(GetTargetOpacity());
  clone->SetVisible(GetTargetVisibility());
  clone->SetFillsBoundsOpaquely(fills_bounds_opaquely_);
  clone->SetFillsBoundsCompletely(fills_bounds_completely_);
  clone->set_name(name_);

  return clone;
}

std::unique_ptr<Layer> Layer::Mirror() {
  auto mirror = Clone();
  mirrors_.emplace_back(base::MakeUnique<LayerMirror>(this, mirror.get()));
  return mirror;
}

const Compositor* Layer::GetCompositor() const {
  return GetRoot(this)->compositor_;
}

float Layer::opacity() const {
  return cc_layer_->opacity();
}

void Layer::SetCompositor(Compositor* compositor,
                          scoped_refptr<cc::Layer> root_layer) {
  // This function must only be called to set the compositor on the root ui
  // layer.
  DCHECK(compositor);
  DCHECK(!compositor_);
  DCHECK(compositor->root_layer() == this);
  DCHECK(!parent_);

  compositor_ = compositor;
  OnDeviceScaleFactorChanged(compositor->device_scale_factor());

  root_layer->AddChild(cc_layer_);
  SetCompositorForAnimatorsInTree(compositor);
}

void Layer::ResetCompositor() {
  DCHECK(!parent_);
  if (compositor_) {
    ResetCompositorForAnimatorsInTree(compositor_);
    compositor_ = nullptr;
  }
}

void Layer::AddObserver(LayerObserver* observer) {
  observer_list_.AddObserver(observer);
}

void Layer::RemoveObserver(LayerObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void Layer::Add(Layer* child) {
  DCHECK(!child->compositor_);
  if (child->parent_)
    child->parent_->Remove(child);
  child->parent_ = this;
  children_.push_back(child);
  cc_layer_->AddChild(child->cc_layer_);
  child->OnDeviceScaleFactorChanged(device_scale_factor_);
  Compositor* compositor = GetCompositor();
  if (compositor)
    child->SetCompositorForAnimatorsInTree(compositor);
}

void Layer::Remove(Layer* child) {
  // Current bounds are used to calculate offsets when layers are reparented.
  // Stop (and complete) an ongoing animation to update the bounds immediately.
  LayerAnimator* child_animator = child->animator_.get();
  if (child_animator)
    child_animator->StopAnimatingProperty(ui::LayerAnimationElement::BOUNDS);

  Compositor* compositor = GetCompositor();
  if (compositor)
    child->ResetCompositorForAnimatorsInTree(compositor);

  std::vector<Layer*>::iterator i =
      std::find(children_.begin(), children_.end(), child);
  DCHECK(i != children_.end());
  children_.erase(i);
  child->parent_ = NULL;
  child->cc_layer_->RemoveFromParent();
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
  Compositor* compositor = GetCompositor();

  if (animator_) {
    if (compositor)
      animator_->DetachLayerAndTimeline(compositor);
    animator_->SetDelegate(nullptr);
  }

  animator_ = animator;

  if (animator_) {
    animator_->SetDelegate(this);
    if (compositor)
      animator_->AttachLayerAndTimeline(compositor);
  }
}

LayerAnimator* Layer::GetAnimator() {
  if (!animator_)
    SetAnimator(LayerAnimator::CreateDefaultAnimator());
  return animator_.get();
}

void Layer::SetTransform(const gfx::Transform& transform) {
  GetAnimator()->SetTransform(transform);
}

gfx::Transform Layer::GetTargetTransform() const {
  if (animator_ && animator_->IsAnimatingProperty(
      LayerAnimationElement::TRANSFORM)) {
    return animator_->GetTargetTransform();
  }
  return transform();
}

void Layer::SetBounds(const gfx::Rect& bounds) {
  GetAnimator()->SetBounds(bounds);
}

void Layer::SetSubpixelPositionOffset(const gfx::Vector2dF& offset) {
  subpixel_position_offset_ = offset;
  RecomputePosition();
}

gfx::Rect Layer::GetTargetBounds() const {
  if (animator_ && animator_->IsAnimatingProperty(
      LayerAnimationElement::BOUNDS)) {
    return animator_->GetTargetBounds();
  }
  return bounds_;
}

void Layer::SetMasksToBounds(bool masks_to_bounds) {
  cc_layer_->SetMasksToBounds(masks_to_bounds);
}

bool Layer::GetMasksToBounds() const {
  return cc_layer_->masks_to_bounds();
}

void Layer::SetOpacity(float opacity) {
  GetAnimator()->SetOpacity(opacity);
}

float Layer::GetCombinedOpacity() const {
  float opacity = this->opacity();
  Layer* current = this->parent_;
  while (current) {
    opacity *= current->opacity();
    current = current->parent_;
  }
  return opacity;
}

void Layer::SetLayerTemperature(float value) {
  GetAnimator()->SetTemperature(value);
}

float Layer::GetTargetTemperature() const {
  if (animator_ &&
      animator_->IsAnimatingProperty(LayerAnimationElement::TEMPERATURE)) {
    return animator_->GetTargetTemperature();
  }
  return layer_temperature();
}

void Layer::SetBackgroundBlur(int blur_radius) {
  background_blur_radius_ = blur_radius;

  SetLayerBackgroundFilters();
}

void Layer::SetLayerSaturation(float saturation) {
  layer_saturation_ = saturation;
  SetLayerFilters();
}

void Layer::SetLayerBrightness(float brightness) {
  GetAnimator()->SetBrightness(brightness);
}

float Layer::GetTargetBrightness() const {
  if (animator_ && animator_->IsAnimatingProperty(
      LayerAnimationElement::BRIGHTNESS)) {
    return animator_->GetTargetBrightness();
  }
  return layer_brightness();
}

void Layer::SetLayerGrayscale(float grayscale) {
  GetAnimator()->SetGrayscale(grayscale);
}

float Layer::GetTargetGrayscale() const {
  if (animator_ && animator_->IsAnimatingProperty(
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
  cc_layer_->SetMaskLayer(layer_mask ? layer_mask->cc_layer_ : NULL);
  // We need to reference the linked object so that it can properly break the
  // link to us when it gets deleted.
  if (layer_mask) {
    layer_mask->layer_mask_back_link_ = this;
    layer_mask->OnDeviceScaleFactorChanged(device_scale_factor_);
  }
}

void Layer::SetBackgroundZoom(float zoom, int inset) {
  zoom_ = zoom;
  zoom_inset_ = inset;

  SetLayerBackgroundFilters();
}

void Layer::SetAlphaShape(std::unique_ptr<SkRegion> region) {
  alpha_shape_ = std::move(region);

  SetLayerFilters();
}

void Layer::SetLayerFilters() {
  cc::FilterOperations filters;
  if (layer_saturation_) {
    filters.Append(cc::FilterOperation::CreateSaturateFilter(
        layer_saturation_));
  }
  if (layer_grayscale_) {
    filters.Append(cc::FilterOperation::CreateGrayscaleFilter(
        layer_grayscale_));
  }
  if (layer_temperature_) {
    float color_matrix[] = {
        1.0f,               0.0f,              0.0f, 0.0f, 0.0f,
        0.0f, layer_green_scale_,              0.0f, 0.0f, 0.0f,
        0.0f,               0.0f, layer_blue_scale_, 0.0f, 0.0f,
        0.0f,               0.0f,              0.0f, 1.0f, 0.0f
    };
    filters.Append(cc::FilterOperation::CreateColorMatrixFilter(color_matrix));
  }
  if (layer_inverted_)
    filters.Append(cc::FilterOperation::CreateInvertFilter(1.0));
  // Brightness goes last, because the resulting colors neeed clamping, which
  // cause further color matrix filters to be applied separately. In this order,
  // they all can be combined in a single pass.
  if (layer_brightness_) {
    filters.Append(cc::FilterOperation::CreateSaturatingBrightnessFilter(
        layer_brightness_));
  }
  if (alpha_shape_) {
    filters.Append(cc::FilterOperation::CreateAlphaThresholdFilter(
            *alpha_shape_, 0.f, 0.f));
  }

  cc_layer_->SetFilters(filters);
}

void Layer::SetLayerBackgroundFilters() {
  cc::FilterOperations filters;
  if (zoom_ != 1)
    filters.Append(cc::FilterOperation::CreateZoomFilter(zoom_, zoom_inset_));

  if (background_blur_radius_) {
    filters.Append(cc::FilterOperation::CreateBlurFilter(
        background_blur_radius_));
  }

  cc_layer_->SetBackgroundFilters(filters);
}

float Layer::GetTargetOpacity() const {
  if (animator_ && animator_->IsAnimatingProperty(
      LayerAnimationElement::OPACITY))
    return animator_->GetTargetOpacity();
  return opacity();
}

void Layer::SetVisible(bool visible) {
  GetAnimator()->SetVisibility(visible);
}

bool Layer::GetTargetVisibility() const {
  if (animator_ && animator_->IsAnimatingProperty(
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

bool Layer::GetTargetTransformRelativeTo(const Layer* ancestor,
                                         gfx::Transform* transform) const {
  const Layer* p = this;
  for (; p && p != ancestor; p = p->parent()) {
    gfx::Transform translation;
    translation.Translate(static_cast<float>(p->bounds().x()),
                          static_cast<float>(p->bounds().y()));
    // Use target transform so that result will be correct once animation is
    // finished.
    if (!p->GetTargetTransform().IsIdentity())
      transform->ConcatTransform(p->GetTargetTransform());
    transform->ConcatTransform(translation);
  }
  return p == ancestor;
}

void Layer::SetFillsBoundsOpaquely(bool fills_bounds_opaquely) {
  if (fills_bounds_opaquely_ == fills_bounds_opaquely)
    return;

  fills_bounds_opaquely_ = fills_bounds_opaquely;

  cc_layer_->SetContentsOpaque(fills_bounds_opaquely);
}

void Layer::SetFillsBoundsCompletely(bool fills_bounds_completely) {
  fills_bounds_completely_ = fills_bounds_completely;
}

void Layer::SwitchToLayer(scoped_refptr<cc::Layer> new_layer) {
  // Finish animations being handled by cc_layer_.
  if (animator_) {
    animator_->StopAnimatingProperty(LayerAnimationElement::TRANSFORM);
    animator_->StopAnimatingProperty(LayerAnimationElement::OPACITY);
    animator_->SwitchToLayer(new_layer);
  }

  if (texture_layer_.get())
    texture_layer_->ClearClient();

  cc_layer_->RemoveAllChildren();
  if (cc_layer_->parent()) {
    cc_layer_->parent()->ReplaceChild(cc_layer_, new_layer);
  }
  cc_layer_->SetLayerClient(NULL);
  new_layer->SetOpacity(cc_layer_->opacity());
  new_layer->SetTransform(cc_layer_->transform());
  new_layer->SetPosition(cc_layer_->position());
  new_layer->SetBackgroundColor(cc_layer_->background_color());

  cc_layer_ = new_layer.get();
  content_layer_ = NULL;
  solid_color_layer_ = NULL;
  texture_layer_ = NULL;
  surface_layer_ = NULL;

  for (size_t i = 0; i < children_.size(); ++i) {
    DCHECK(children_[i]->cc_layer_);
    cc_layer_->AddChild(children_[i]->cc_layer_);
  }
  cc_layer_->SetLayerClient(this);
  cc_layer_->SetTransformOrigin(gfx::Point3F());
  cc_layer_->SetContentsOpaque(fills_bounds_opaquely_);
  cc_layer_->SetIsDrawable(type_ != LAYER_NOT_DRAWN);
  cc_layer_->SetHideLayerAndSubtree(!visible_);
  cc_layer_->SetElementId(cc::ElementId(cc_layer_->id()));

  SetLayerFilters();
  SetLayerBackgroundFilters();
}

void Layer::SwitchCCLayerForTest() {
  scoped_refptr<cc::Layer> new_layer = cc::PictureLayer::Create(this);
  SwitchToLayer(new_layer);
  content_layer_ = new_layer;
}

void Layer::SetTextureMailbox(
    const cc::TextureMailbox& mailbox,
    std::unique_ptr<cc::SingleReleaseCallback> release_callback,
    gfx::Size texture_size_in_dip) {
  DCHECK(type_ == LAYER_TEXTURED || type_ == LAYER_SOLID_COLOR);
  DCHECK(mailbox.IsValid());
  DCHECK(release_callback);
  if (!texture_layer_.get()) {
    scoped_refptr<cc::TextureLayer> new_layer =
        cc::TextureLayer::CreateForMailbox(this);
    new_layer->SetFlipped(true);
    SwitchToLayer(new_layer);
    texture_layer_ = new_layer;
    // Reset the frame_size_in_dip_ so that SetTextureSize() will not early out,
    // the frame_size_in_dip_ was for a previous (different) |texture_layer_|.
    frame_size_in_dip_ = gfx::Size();
  }
  if (mailbox_release_callback_)
    mailbox_release_callback_->Run(gpu::SyncToken(), false);
  mailbox_release_callback_ = std::move(release_callback);
  mailbox_ = mailbox;
  SetTextureSize(texture_size_in_dip);
}

void Layer::SetTextureSize(gfx::Size texture_size_in_dip) {
  DCHECK(texture_layer_.get());
  if (frame_size_in_dip_ == texture_size_in_dip)
    return;
  frame_size_in_dip_ = texture_size_in_dip;
  RecomputeDrawsContentAndUVRect();
  texture_layer_->SetNeedsDisplay();
}

void Layer::SetTextureFlipped(bool flipped) {
  DCHECK(texture_layer_.get());
  texture_layer_->SetFlipped(flipped);
}

bool Layer::TextureFlipped() const {
  DCHECK(texture_layer_.get());
  return texture_layer_->flipped();
}

void Layer::SetShowPrimarySurface(
    const cc::SurfaceInfo& surface_info,
    scoped_refptr<cc::SurfaceReferenceFactory> ref_factory) {
  DCHECK(type_ == LAYER_TEXTURED || type_ == LAYER_SOLID_COLOR);

  if (!surface_layer_) {
    scoped_refptr<cc::SurfaceLayer> new_layer =
        cc::SurfaceLayer::Create(ref_factory);
    SwitchToLayer(new_layer);
    surface_layer_ = new_layer;
  }

  surface_layer_->SetPrimarySurfaceInfo(surface_info);

  frame_size_in_dip_ = gfx::ConvertSizeToDIP(surface_info.device_scale_factor(),
                                             surface_info.size_in_pixels());
  RecomputeDrawsContentAndUVRect();

  for (const auto& mirror : mirrors_)
    mirror->dest()->SetShowPrimarySurface(surface_info, ref_factory);
}

void Layer::SetFallbackSurface(const cc::SurfaceInfo& surface_info) {
  DCHECK(type_ == LAYER_TEXTURED || type_ == LAYER_SOLID_COLOR);
  DCHECK(surface_layer_);

  // TODO(fsamuel): We should compute the gutter in the display compositor.
  surface_layer_->SetFallbackSurfaceInfo(surface_info);

  for (const auto& mirror : mirrors_)
    mirror->dest()->SetFallbackSurface(surface_info);
}

const cc::SurfaceInfo* Layer::GetFallbackSurfaceInfo() const {
  if (surface_layer_)
    return &surface_layer_->fallback_surface_info();
  return nullptr;
}

void Layer::SetShowSolidColorContent() {
  DCHECK_EQ(type_, LAYER_SOLID_COLOR);

  if (solid_color_layer_.get())
    return;

  scoped_refptr<cc::SolidColorLayer> new_layer = cc::SolidColorLayer::Create();
  SwitchToLayer(new_layer);
  solid_color_layer_ = new_layer;

  mailbox_ = cc::TextureMailbox();
  if (mailbox_release_callback_) {
    mailbox_release_callback_->Run(gpu::SyncToken(), false);
    mailbox_release_callback_.reset();
  }
  RecomputeDrawsContentAndUVRect();
}

void Layer::UpdateNinePatchLayerImage(const gfx::ImageSkia& image) {
  DCHECK_EQ(type_, LAYER_NINE_PATCH);
  DCHECK(nine_patch_layer_.get());

  nine_patch_layer_image_ = image;
  // TODO(estade): we don't clean up old bitmaps in the UIResourceManager when
  // the scale factor changes. Currently for the way NinePatchLayers are used,
  // we don't need/want to, but we should address this in the future if it
  // becomes an issue.
  nine_patch_layer_->SetBitmap(
      image.GetRepresentation(device_scale_factor_).sk_bitmap());
}

void Layer::UpdateNinePatchLayerAperture(const gfx::Rect& aperture_in_dip) {
  DCHECK_EQ(type_, LAYER_NINE_PATCH);
  DCHECK(nine_patch_layer_.get());
  nine_patch_layer_aperture_ = aperture_in_dip;
  gfx::Rect aperture_in_pixel = ConvertRectToPixel(this, aperture_in_dip);
  nine_patch_layer_->SetAperture(aperture_in_pixel);
}

void Layer::UpdateNinePatchLayerBorder(const gfx::Rect& border) {
  DCHECK_EQ(type_, LAYER_NINE_PATCH);
  DCHECK(nine_patch_layer_.get());
  nine_patch_layer_->SetBorder(border);
}

void Layer::UpdateNinePatchOcclusion(const gfx::Rect& occlusion) {
  DCHECK_EQ(type_, LAYER_NINE_PATCH);
  DCHECK(nine_patch_layer_.get());
  nine_patch_layer_->SetLayerOcclusion(occlusion);
}

void Layer::SetColor(SkColor color) { GetAnimator()->SetColor(color); }

SkColor Layer::GetTargetColor() const {
  if (animator_ && animator_->IsAnimatingProperty(
      LayerAnimationElement::COLOR))
    return animator_->GetTargetColor();
  return cc_layer_->background_color();
}

SkColor Layer::background_color() const {
  return cc_layer_->background_color();
}

bool Layer::SchedulePaint(const gfx::Rect& invalid_rect) {
  if ((type_ == LAYER_SOLID_COLOR && !texture_layer_.get()) ||
      type_ == LAYER_NINE_PATCH || (!delegate_ && !mailbox_.IsValid()))
    return false;

  damaged_region_.Union(invalid_rect);
  ScheduleDraw();

  if (layer_mask_) {
    layer_mask_->damaged_region_.Union(invalid_rect);
    layer_mask_->ScheduleDraw();
  }
  return true;
}

void Layer::ScheduleDraw() {
  Compositor* compositor = GetCompositor();
  if (compositor)
    compositor->ScheduleDraw();
}

void Layer::SendDamagedRects() {
  if (damaged_region_.IsEmpty())
    return;
  if (!delegate_ && !mailbox_.IsValid())
    return;

  for (cc::Region::Iterator iter(damaged_region_); iter.has_rect(); iter.next())
    cc_layer_->SetNeedsDisplayRect(iter.rect());
  if (layer_mask_)
    layer_mask_->SendDamagedRects();

  if (content_layer_)
    paint_region_.Union(damaged_region_);
  damaged_region_.Clear();
}

void Layer::CompleteAllAnimations() {
  typedef std::vector<scoped_refptr<LayerAnimator> > LayerAnimatorVector;
  LayerAnimatorVector animators;
  CollectAnimators(&animators);
  for (LayerAnimatorVector::const_iterator it = animators.begin();
       it != animators.end();
       ++it) {
    (*it)->StopAnimating();
  }
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
  if (animator_)
    animator_->StopAnimatingProperty(LayerAnimationElement::TRANSFORM);
  device_scale_factor_ = device_scale_factor;
  RecomputeDrawsContentAndUVRect();
  RecomputePosition();
  if (nine_patch_layer_) {
    if (!nine_patch_layer_image_.isNull())
      UpdateNinePatchLayerImage(nine_patch_layer_image_);
    UpdateNinePatchLayerAperture(nine_patch_layer_aperture_);
  }
  SchedulePaint(gfx::Rect(bounds_.size()));
  if (delegate_)
    delegate_->OnDeviceScaleFactorChanged(device_scale_factor);
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->OnDeviceScaleFactorChanged(device_scale_factor);
  if (layer_mask_)
    layer_mask_->OnDeviceScaleFactorChanged(device_scale_factor);
}

void Layer::OnDelegatedFrameDamage(const gfx::Rect& damage_rect_in_dip) {
  DCHECK(surface_layer_.get());
  if (delegate_)
    delegate_->OnDelegatedFrameDamage(damage_rect_in_dip);
}

void Layer::SetScrollable(
    Layer* parent_clip_layer,
    const base::Callback<void(const gfx::ScrollOffset&)>& on_scroll) {
  cc_layer_->SetScrollClipLayerId(parent_clip_layer->cc_layer_->id());
  cc_layer_->set_did_scroll_callback(on_scroll);
  cc_layer_->SetUserScrollable(true, true);
}

gfx::ScrollOffset Layer::CurrentScrollOffset() const {
  const Compositor* compositor = GetCompositor();
  gfx::ScrollOffset offset;
  if (compositor &&
      compositor->GetScrollOffsetForLayer(cc_layer_->id(), &offset))
    return offset;
  return cc_layer_->scroll_offset();
}

void Layer::SetScrollOffset(const gfx::ScrollOffset& offset) {
  Compositor* compositor = GetCompositor();
  bool scrolled_on_impl_side =
      compositor && compositor->ScrollLayerTo(cc_layer_->id(), offset);

  if (!scrolled_on_impl_side)
    cc_layer_->SetScrollOffset(offset);

  DCHECK_EQ(offset.x(), CurrentScrollOffset().x());
  DCHECK_EQ(offset.y(), CurrentScrollOffset().y());
}

void Layer::RequestCopyOfOutput(
    std::unique_ptr<cc::CopyOutputRequest> request) {
  cc_layer_->RequestCopyOfOutput(std::move(request));
}

gfx::Rect Layer::PaintableRegion() {
  return gfx::Rect(size());
}

scoped_refptr<cc::DisplayItemList> Layer::PaintContentsToDisplayList(
    ContentLayerClient::PaintingControlSetting painting_control) {
  TRACE_EVENT1("ui", "Layer::PaintContentsToDisplayList", "name", name_);
  gfx::Rect local_bounds(bounds().size());
  gfx::Rect invalidation(
      gfx::IntersectRects(paint_region_.bounds(), local_bounds));
  paint_region_.Clear();
  auto display_list = make_scoped_refptr(new cc::DisplayItemList);
  if (delegate_) {
    delegate_->OnPaintLayer(
        PaintContext(display_list.get(), device_scale_factor_, invalidation));
  }
  display_list->Finalize();
  // TODO(domlaskowski): Move mirror invalidation to Layer::SchedulePaint.
  for (const auto& mirror : mirrors_)
    mirror->dest()->SchedulePaint(invalidation);
  return display_list;
}

bool Layer::FillsBoundsCompletely() const { return fills_bounds_completely_; }

size_t Layer::GetApproximateUnsharedMemoryUsage() const {
  // Most of the "picture memory" is shared with the cc::DisplayItemList, so
  // there's nothing significant to report here.
  return 0;
}

bool Layer::PrepareTextureMailbox(
    cc::TextureMailbox* mailbox,
    std::unique_ptr<cc::SingleReleaseCallback>* release_callback) {
  if (!mailbox_release_callback_)
    return false;
  *mailbox = mailbox_;
  *release_callback = std::move(mailbox_release_callback_);
  return true;
}

class LayerDebugInfo : public base::trace_event::ConvertableToTraceFormat {
 public:
  explicit LayerDebugInfo(const std::string& name) : name_(name) {}
  ~LayerDebugInfo() override {}
  void AppendAsTraceFormat(std::string* out) const override {
    base::DictionaryValue dictionary;
    dictionary.SetString("layer_name", name_);
    std::string tmp;
    base::JSONWriter::Write(dictionary, &tmp);
    out->append(tmp);
  }

 private:
  std::string name_;
};

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
Layer::TakeDebugInfo(cc::Layer* layer) {
  return base::WrapUnique(new LayerDebugInfo(name_));
}

void Layer::didUpdateMainThreadScrollingReasons() {}
void Layer::didChangeScrollbarsHidden(bool) {}

void Layer::CollectAnimators(
    std::vector<scoped_refptr<LayerAnimator>>* animators) {
  if (animator_ && animator_->is_animating())
    animators->push_back(animator_);
  for (auto* child : children_)
    child->CollectAnimators(animators);
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

  child->cc_layer_->RemoveFromParent();
  cc_layer_->InsertChild(child->cc_layer_, dest_i);
}

bool Layer::ConvertPointForAncestor(const Layer* ancestor,
                                    gfx::Point* point) const {
  gfx::Transform transform;
  bool result = GetTargetTransformRelativeTo(ancestor, &transform);
  auto p = gfx::Point3F(gfx::PointF(*point));
  transform.TransformPoint(&p);
  *point = gfx::ToFlooredPoint(p.AsPointF());
  return result;
}

bool Layer::ConvertPointFromAncestor(const Layer* ancestor,
                                     gfx::Point* point) const {
  gfx::Transform transform;
  bool result = GetTargetTransformRelativeTo(ancestor, &transform);
  auto p = gfx::Point3F(gfx::PointF(*point));
  transform.TransformPointReverse(&p);
  *point = gfx::ToFlooredPoint(p.AsPointF());
  return result;
}

void Layer::SetBoundsFromAnimation(const gfx::Rect& bounds) {
  if (bounds == bounds_)
    return;

  base::Closure closure;
  const gfx::Rect old_bounds = bounds_;
  bounds_ = bounds;

  RecomputeDrawsContentAndUVRect();
  RecomputePosition();

  if (delegate_)
    delegate_->OnLayerBoundsChanged(old_bounds);

  if (bounds.size() == old_bounds.size()) {
    // Don't schedule a draw if we're invisible. We'll schedule one
    // automatically when we get visible.
    if (IsDrawn())
      ScheduleDraw();
  } else {
    // Always schedule a paint, even if we're invisible.
    SchedulePaint(gfx::Rect(bounds.size()));
  }

  if (sync_bounds_) {
    for (const auto& mirror : mirrors_)
      mirror->dest()->SetBounds(bounds);
  }
}

void Layer::SetTransformFromAnimation(const gfx::Transform& transform) {
  cc_layer_->SetTransform(transform);
}

void Layer::SetOpacityFromAnimation(float opacity) {
  cc_layer_->SetOpacity(opacity);
  ScheduleDraw();
}

void Layer::SetVisibilityFromAnimation(bool visible) {
  if (visible_ == visible)
    return;

  visible_ = visible;
  cc_layer_->SetHideLayerAndSubtree(!visible_);
}

void Layer::SetBrightnessFromAnimation(float brightness) {
  layer_brightness_ = brightness;
  SetLayerFilters();
}

void Layer::SetGrayscaleFromAnimation(float grayscale) {
  layer_grayscale_ = grayscale;
  SetLayerFilters();
}

void Layer::SetColorFromAnimation(SkColor color) {
  DCHECK_EQ(type_, LAYER_SOLID_COLOR);
  cc_layer_->SetBackgroundColor(color);
  SetFillsBoundsOpaquely(SkColorGetA(color) == 0xFF);
}

void Layer::SetTemperatureFromAnimation(float temperature) {
  layer_temperature_ = temperature;

  // If we only tone down the blue scale, the screen will look very green so we
  // also need to tone down the green, but with a less value compared to the
  // blue scale to avoid making things look very red.
  layer_blue_scale_ = 1.0f - temperature;
  layer_green_scale_ = 1.0f - 0.3f * temperature;
  SetLayerFilters();
}

void Layer::ScheduleDrawForAnimation() {
  ScheduleDraw();
}

const gfx::Rect& Layer::GetBoundsForAnimation() const {
  return bounds();
}

gfx::Transform Layer::GetTransformForAnimation() const {
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

SkColor Layer::GetColorForAnimation() const {
  // WebColor is equivalent to SkColor, per WebColor.h.
  // The NULL check is here since this is invoked regardless of whether we have
  // been configured as LAYER_SOLID_COLOR.
  return solid_color_layer_.get() ?
      solid_color_layer_->background_color() : SK_ColorBLACK;
}

float Layer::GetTemperatureFromAnimation() const {
  return layer_temperature_;
}

float Layer::GetDeviceScaleFactor() const {
  return device_scale_factor_;
}

LayerAnimatorCollection* Layer::GetLayerAnimatorCollection() {
  Compositor* compositor = GetCompositor();
  return compositor ? compositor->layer_animator_collection() : NULL;
}

int Layer::GetFrameNumber() const {
  const Compositor* compositor = GetCompositor();
  return compositor ? compositor->activated_frame_count() : 0;
}

float Layer::GetRefreshRate() const {
  const Compositor* compositor = GetCompositor();
  return compositor ? compositor->refresh_rate() : 60.0;
}

cc::Layer* Layer::GetCcLayer() const {
  return cc_layer_;
}

LayerThreadedAnimationDelegate* Layer::GetThreadedAnimationDelegate() {
  DCHECK(animator_);
  return animator_.get();
}

void Layer::CreateCcLayer() {
  if (type_ == LAYER_SOLID_COLOR) {
    solid_color_layer_ = cc::SolidColorLayer::Create();
    cc_layer_ = solid_color_layer_.get();
  } else if (type_ == LAYER_NINE_PATCH) {
    nine_patch_layer_ = cc::NinePatchLayer::Create();
    cc_layer_ = nine_patch_layer_.get();
  } else {
    content_layer_ = cc::PictureLayer::Create(this);
    cc_layer_ = content_layer_.get();
  }
  cc_layer_->SetTransformOrigin(gfx::Point3F());
  cc_layer_->SetContentsOpaque(true);
  cc_layer_->SetIsDrawable(type_ != LAYER_NOT_DRAWN);
  cc_layer_->SetLayerClient(this);
  cc_layer_->SetElementId(cc::ElementId(cc_layer_->id()));
  RecomputePosition();
}

gfx::Transform Layer::transform() const {
  return cc_layer_->transform();
}

void Layer::RecomputeDrawsContentAndUVRect() {
  DCHECK(cc_layer_);
  gfx::Size size(bounds_.size());
  if (texture_layer_.get()) {
    size.SetToMin(frame_size_in_dip_);
    gfx::PointF uv_top_left(0.f, 0.f);
    gfx::PointF uv_bottom_right(
      static_cast<float>(size.width()) / frame_size_in_dip_.width(),
      static_cast<float>(size.height()) / frame_size_in_dip_.height());
    texture_layer_->SetUV(uv_top_left, uv_bottom_right);
  } else if (surface_layer_.get()) {
    size.SetToMin(frame_size_in_dip_);
  }
  cc_layer_->SetBounds(size);
}

void Layer::RecomputePosition() {
  cc_layer_->SetPosition(gfx::PointF(bounds_.origin()) +
                         subpixel_position_offset_);
}

void Layer::SetCompositorForAnimatorsInTree(Compositor* compositor) {
  DCHECK(compositor);
  LayerAnimatorCollection* collection = compositor->layer_animator_collection();

  if (animator_) {
    if (animator_->is_animating())
      animator_->AddToCollection(collection);
    animator_->AttachLayerAndTimeline(compositor);
  }

  for (auto* child : children_)
    child->SetCompositorForAnimatorsInTree(compositor);
}

void Layer::ResetCompositorForAnimatorsInTree(Compositor* compositor) {
  DCHECK(compositor);
  LayerAnimatorCollection* collection = compositor->layer_animator_collection();

  if (animator_) {
    animator_->DetachLayerAndTimeline(compositor);
    animator_->RemoveFromCollection(collection);
  }

  for (auto* child : children_)
    child->ResetCompositorForAnimatorsInTree(compositor);
}

void Layer::OnMirrorDestroyed(LayerMirror* mirror) {
  const auto it = std::find_if(mirrors_.begin(), mirrors_.end(),
      [mirror](const std::unique_ptr<LayerMirror>& mirror_ptr) {
        return mirror_ptr.get() == mirror;
      });

  DCHECK(it != mirrors_.end());
  mirrors_.erase(it);
}

}  // namespace ui
