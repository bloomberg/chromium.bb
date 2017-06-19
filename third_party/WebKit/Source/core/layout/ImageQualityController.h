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

#ifndef ImageQualityController_h
#define ImageQualityController_h

#include <memory>
#include "base/gtest_prod_util.h"
#include "core/CoreExport.h"
#include "platform/Timer.h"
#include "platform/geometry/LayoutSize.h"
#include "platform/graphics/Image.h"
#include "platform/wtf/HashMap.h"

namespace blink {

class ImageResourceObserver;
class ComputedStyle;
class Settings;

typedef HashMap<const void*, LayoutSize> LayerSizeMap;

struct ObjectResizeInfo {
  LayerSizeMap layer_size_map;
  bool is_resizing;
};

typedef HashMap<const ImageResourceObserver*, ObjectResizeInfo> ResourceSizeMap;

class CORE_EXPORT ImageQualityController final {
  WTF_MAKE_NONCOPYABLE(ImageQualityController);
  USING_FAST_MALLOC(ImageQualityController);

 public:
  ~ImageQualityController();

  static ImageQualityController* GetImageQualityController();

  static void Remove(ImageResourceObserver&);

  InterpolationQuality ChooseInterpolationQuality(
      const ImageResourceObserver&,
      const ComputedStyle&,
      const Settings*,
      Image*,
      const void* layer,
      const LayoutSize&,
      double last_frame_time_monotonic);

 private:
  static const double kCLowQualityTimeThreshold;
  static const double kCTimerRestartThreshold;

  ImageQualityController();

  static bool Has(const ImageResourceObserver&);
  void Set(const ImageResourceObserver&,
           LayerSizeMap* inner_map,
           const void* layer,
           const LayoutSize&,
           bool is_resizing);

  bool ShouldPaintAtLowQuality(const ImageResourceObserver&,
                               const ComputedStyle&,
                               const Settings*,
                               Image*,
                               const void* layer,
                               const LayoutSize&,
                               double last_frame_time_monotonic);
  void RemoveLayer(const ImageResourceObserver&,
                   LayerSizeMap* inner_map,
                   const void* layer);
  void ObjectDestroyed(const ImageResourceObserver&);
  bool IsEmpty() { return observer_layer_size_map_.IsEmpty(); }

  void HighQualityRepaintTimerFired(TimerBase*);
  void RestartTimer(double last_frame_time_monotonic);

  // Only for use in testing.
  void SetTimer(std::unique_ptr<TimerBase>);

  ResourceSizeMap observer_layer_size_map_;
  std::unique_ptr<TimerBase> timer_;
  double frame_time_when_timer_started_;

  // For calling set().
  FRIEND_TEST_ALL_PREFIXES(LayoutEmbeddedContentTest,
                           DestroyUpdatesImageQualityController);

  // For calling setTimer(),
  FRIEND_TEST_ALL_PREFIXES(ImageQualityControllerTest,
                           LowQualityFilterForResizingImage);
  FRIEND_TEST_ALL_PREFIXES(
      ImageQualityControllerTest,
      MediumQualityFilterForNotAnimatedWhileAnotherAnimates);
  FRIEND_TEST_ALL_PREFIXES(ImageQualityControllerTest,
                           DontKickTheAnimationTimerWhenPaintingAtTheSameSize);
  FRIEND_TEST_ALL_PREFIXES(ImageQualityControllerTest,
                           DontRestartTimerUnlessAdvanced);
};

}  // namespace blink

#endif
