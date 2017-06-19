/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/layout/ImageQualityController.h"

#include "core/frame/Settings.h"
#include "core/loader/resource/ImageResourceObserver.h"
#include "core/style/ComputedStyle.h"

namespace blink {

const double ImageQualityController::kCLowQualityTimeThreshold =
    0.500;                                                             // 500 ms
const double ImageQualityController::kCTimerRestartThreshold = 0.250;  // 250 ms

static ImageQualityController* g_image_quality_controller = nullptr;

ImageQualityController* ImageQualityController::GetImageQualityController() {
  if (!g_image_quality_controller)
    g_image_quality_controller = new ImageQualityController;

  return g_image_quality_controller;
}

void ImageQualityController::Remove(ImageResourceObserver& layout_observer) {
  if (g_image_quality_controller) {
    g_image_quality_controller->ObjectDestroyed(layout_observer);
    if (g_image_quality_controller->IsEmpty()) {
      delete g_image_quality_controller;
      g_image_quality_controller = nullptr;
    }
  }
}

bool ImageQualityController::Has(const ImageResourceObserver& layout_observer) {
  return g_image_quality_controller &&
         g_image_quality_controller->observer_layer_size_map_.Contains(
             &layout_observer);
}

InterpolationQuality ImageQualityController::ChooseInterpolationQuality(
    const ImageResourceObserver& observer,
    const ComputedStyle& style,
    const Settings* settings,
    Image* image,
    const void* layer,
    const LayoutSize& layout_size,
    double last_frame_time_monotonic) {
  if (style.ImageRendering() == EImageRendering::kPixelated)
    return kInterpolationNone;

  if (kInterpolationDefault == kInterpolationLow)
    return kInterpolationLow;

  if (ShouldPaintAtLowQuality(observer, style, settings, image, layer,
                              layout_size, last_frame_time_monotonic))
    return kInterpolationLow;

  // For images that are potentially animated we paint them at medium quality.
  if (image && image->MaybeAnimated())
    return kInterpolationMedium;

  return kInterpolationDefault;
}

ImageQualityController::~ImageQualityController() {
  // This will catch users of ImageQualityController that forget to call
  // cleanUp.
  DCHECK(!g_image_quality_controller || g_image_quality_controller->IsEmpty());
}

ImageQualityController::ImageQualityController()
    : timer_(WTF::WrapUnique(new Timer<ImageQualityController>(
          this,
          &ImageQualityController::HighQualityRepaintTimerFired))),
      frame_time_when_timer_started_(0.0) {}

void ImageQualityController::SetTimer(std::unique_ptr<TimerBase> new_timer) {
  timer_ = std::move(new_timer);
}

void ImageQualityController::RemoveLayer(const ImageResourceObserver& observer,
                                         LayerSizeMap* inner_map,
                                         const void* layer) {
  if (inner_map) {
    inner_map->erase(layer);
    if (inner_map->IsEmpty())
      ObjectDestroyed(observer);
  }
}

void ImageQualityController::Set(const ImageResourceObserver& observer,
                                 LayerSizeMap* inner_map,
                                 const void* layer,
                                 const LayoutSize& size,
                                 bool is_resizing) {
  if (inner_map) {
    inner_map->Set(layer, size);
    observer_layer_size_map_.find(&observer)->value.is_resizing = is_resizing;
  } else {
    ObjectResizeInfo new_resize_info;
    new_resize_info.layer_size_map.Set(layer, size);
    new_resize_info.is_resizing = is_resizing;
    observer_layer_size_map_.Set(&observer, new_resize_info);
  }
}

void ImageQualityController::ObjectDestroyed(
    const ImageResourceObserver& observer) {
  observer_layer_size_map_.erase(&observer);
  if (observer_layer_size_map_.IsEmpty()) {
    timer_->Stop();
  }
}

void ImageQualityController::HighQualityRepaintTimerFired(TimerBase*) {
  for (auto& i : observer_layer_size_map_) {
    // Only invalidate the observer if it is animating.
    if (!i.value.is_resizing)
      continue;

    i.key->RequestFullPaintInvalidationForImage();
    i.value.is_resizing = false;
  }
  frame_time_when_timer_started_ = 0.0;
}

void ImageQualityController::RestartTimer(double last_frame_time_monotonic) {
  if (!timer_->IsActive() || last_frame_time_monotonic == 0.0 ||
      frame_time_when_timer_started_ == 0.0 ||
      (last_frame_time_monotonic - frame_time_when_timer_started_ >
       kCTimerRestartThreshold)) {
    timer_->StartOneShot(kCLowQualityTimeThreshold, BLINK_FROM_HERE);
    frame_time_when_timer_started_ = last_frame_time_monotonic;
  }
}

bool ImageQualityController::ShouldPaintAtLowQuality(
    const ImageResourceObserver& observer,
    const ComputedStyle& style,
    const Settings* settings,
    Image* image,
    const void* layer,
    const LayoutSize& layout_size,
    double last_frame_time_monotonic) {
  // If the image is not a bitmap image, then none of this is relevant and we
  // just paint at high quality.
  if (!image || !image->IsBitmapImage())
    return false;

  if (!layer)
    return false;

  if (style.ImageRendering() == EImageRendering::kWebkitOptimizeContrast)
    return true;

  if (settings && settings->GetUseDefaultImageInterpolationQuality())
    return false;

  // Look ourselves up in the hashtables.
  ResourceSizeMap::iterator i = observer_layer_size_map_.find(&observer);
  LayerSizeMap* inner_map = nullptr;
  bool observer_is_resizing = false;
  if (i != observer_layer_size_map_.end()) {
    inner_map = &i->value.layer_size_map;
    observer_is_resizing = i->value.is_resizing;
  }
  LayoutSize old_size;
  bool is_first_resize = true;
  if (inner_map) {
    LayerSizeMap::iterator j = inner_map->find(layer);
    if (j != inner_map->end()) {
      is_first_resize = false;
      old_size = j->value;
    }
  }

  if (layout_size == image->Size()) {
    // There is no scale in effect. If we had a scale in effect before, we can
    // just remove this observer from the list.
    RemoveLayer(observer, inner_map, layer);
    return false;
  }

  // If an animated resize is active for this observer, paint in low quality and
  // kick the timer ahead.
  if (observer_is_resizing) {
    bool sizes_changed = old_size != layout_size;
    Set(observer, inner_map, layer, layout_size, sizes_changed);
    if (sizes_changed)
      RestartTimer(last_frame_time_monotonic);
    return true;
  }
  // If this is the first time resizing this image, or its size is the
  // same as the last resize, draw at high res, but record the paint
  // size and set the timer.
  if (is_first_resize || old_size == layout_size) {
    RestartTimer(last_frame_time_monotonic);
    Set(observer, inner_map, layer, layout_size, false);
    return false;
  }
  // If the timer is no longer active, draw at high quality and don't
  // set the timer.
  if (!timer_->IsActive()) {
    RemoveLayer(observer, inner_map, layer);
    return false;
  }
  // This observer has been resized to two different sizes while the timer
  // is active, so draw at low quality, set the flag for animated resizes and
  // the observer to the list for high quality redraw.
  Set(observer, inner_map, layer, layout_size, true);
  RestartTimer(last_frame_time_monotonic);
  return true;
}

}  // namespace blink
