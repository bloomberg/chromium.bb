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

#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutObject.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"

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

void ImageQualityController::Remove(LayoutObject& layout_object) {
  if (g_image_quality_controller) {
    g_image_quality_controller->ObjectDestroyed(layout_object);
    if (g_image_quality_controller->IsEmpty()) {
      delete g_image_quality_controller;
      g_image_quality_controller = nullptr;
    }
  }
}

bool ImageQualityController::Has(const LayoutObject& layout_object) {
  return g_image_quality_controller &&
         g_image_quality_controller->object_layer_size_map_.Contains(
             &layout_object);
}

InterpolationQuality ImageQualityController::ChooseInterpolationQuality(
    const LayoutObject& object,
    Image* image,
    const void* layer,
    const LayoutSize& layout_size) {
  if (object.Style()->ImageRendering() == kImageRenderingPixelated)
    return kInterpolationNone;

  if (kInterpolationDefault == kInterpolationLow)
    return kInterpolationLow;

  if (ShouldPaintAtLowQuality(object, image, layer, layout_size,
                              object.GetFrameView()
                                  ->GetPage()
                                  ->GetChromeClient()
                                  .LastFrameTimeMonotonic()))
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

void ImageQualityController::RemoveLayer(const LayoutObject& object,
                                         LayerSizeMap* inner_map,
                                         const void* layer) {
  if (inner_map) {
    inner_map->erase(layer);
    if (inner_map->IsEmpty())
      ObjectDestroyed(object);
  }
}

void ImageQualityController::Set(const LayoutObject& object,
                                 LayerSizeMap* inner_map,
                                 const void* layer,
                                 const LayoutSize& size,
                                 bool is_resizing) {
  if (inner_map) {
    inner_map->Set(layer, size);
    object_layer_size_map_.Find(&object)->value.is_resizing = is_resizing;
  } else {
    ObjectResizeInfo new_resize_info;
    new_resize_info.layer_size_map.Set(layer, size);
    new_resize_info.is_resizing = is_resizing;
    object_layer_size_map_.Set(&object, new_resize_info);
  }
}

void ImageQualityController::ObjectDestroyed(const LayoutObject& object) {
  object_layer_size_map_.erase(&object);
  if (object_layer_size_map_.IsEmpty()) {
    timer_->Stop();
  }
}

void ImageQualityController::HighQualityRepaintTimerFired(TimerBase*) {
  for (auto& i : object_layer_size_map_) {
    // Only invalidate the object if it is animating.
    if (!i.value.is_resizing)
      continue;

    // TODO(wangxianzhu): Use LayoutObject::getMutableForPainting().
    const_cast<LayoutObject*>(i.key)->SetShouldDoFullPaintInvalidation();
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
    const LayoutObject& object,
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

  if (object.Style()->ImageRendering() == kImageRenderingOptimizeContrast)
    return true;

  if (LocalFrame* frame = object.GetFrame()) {
    if (frame->GetSettings() &&
        frame->GetSettings()->GetUseDefaultImageInterpolationQuality())
      return false;
  }

  // Look ourselves up in the hashtables.
  ObjectLayerSizeMap::iterator i = object_layer_size_map_.Find(&object);
  LayerSizeMap* inner_map = nullptr;
  bool object_is_resizing = false;
  if (i != object_layer_size_map_.end()) {
    inner_map = &i->value.layer_size_map;
    object_is_resizing = i->value.is_resizing;
  }
  LayoutSize old_size;
  bool is_first_resize = true;
  if (inner_map) {
    LayerSizeMap::iterator j = inner_map->Find(layer);
    if (j != inner_map->end()) {
      is_first_resize = false;
      old_size = j->value;
    }
  }

  if (layout_size == image->Size()) {
    // There is no scale in effect. If we had a scale in effect before, we can
    // just remove this object from the list.
    RemoveLayer(object, inner_map, layer);
    return false;
  }

  // If an animated resize is active for this object, paint in low quality and
  // kick the timer ahead.
  if (object_is_resizing) {
    bool sizes_changed = old_size != layout_size;
    Set(object, inner_map, layer, layout_size, sizes_changed);
    if (sizes_changed)
      RestartTimer(last_frame_time_monotonic);
    return true;
  }
  // If this is the first time resizing this image, or its size is the
  // same as the last resize, draw at high res, but record the paint
  // size and set the timer.
  if (is_first_resize || old_size == layout_size) {
    RestartTimer(last_frame_time_monotonic);
    Set(object, inner_map, layer, layout_size, false);
    return false;
  }
  // If the timer is no longer active, draw at high quality and don't
  // set the timer.
  if (!timer_->IsActive()) {
    RemoveLayer(object, inner_map, layer);
    return false;
  }
  // This object has been resized to two different sizes while the timer
  // is active, so draw at low quality, set the flag for animated resizes and
  // the object to the list for high quality redraw.
  Set(object, inner_map, layer, layout_size, true);
  RestartTimer(last_frame_time_monotonic);
  return true;
}

}  // namespace blink
