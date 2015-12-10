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

#include "base/gtest_prod_util.h"
#include "core/CoreExport.h"
#include "core/layout/LayoutObject.h"
#include "platform/geometry/IntSize.h"
#include "platform/geometry/LayoutSize.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/ImageOrientation.h"
#include "platform/graphics/ImageSource.h"
#include "wtf/HashMap.h"

namespace blink {

typedef HashMap<const void*, LayoutSize> LayerSizeMap;
typedef HashMap<const LayoutObject*, LayerSizeMap> ObjectLayerSizeMap;

class CORE_EXPORT ImageQualityController final {
    WTF_MAKE_NONCOPYABLE(ImageQualityController); USING_FAST_MALLOC(ImageQualityController);
public:
    ~ImageQualityController();

    static ImageQualityController* imageQualityController();

    static void remove(LayoutObject&);

    InterpolationQuality chooseInterpolationQuality(const LayoutObject&, Image*, const void* layer, const LayoutSize&);

private:
    ImageQualityController();

    static bool has(const LayoutObject&);
    void set(const LayoutObject&, LayerSizeMap* innerMap, const void* layer, const LayoutSize&);

    bool shouldPaintAtLowQuality(const LayoutObject&, Image*, const void* layer, const LayoutSize&);
    void removeLayer(const LayoutObject&, LayerSizeMap* innerMap, const void* layer);
    void objectDestroyed(const LayoutObject&);
    bool isEmpty() { return m_objectLayerSizeMap.isEmpty(); }

    void highQualityRepaintTimerFired(Timer<ImageQualityController>*);
    void restartTimer();

    // Only for use in testing.
    void setTimer(Timer<ImageQualityController>*);

    ObjectLayerSizeMap m_objectLayerSizeMap;
    OwnPtr<Timer<ImageQualityController>> m_timer;
    bool m_animatedResizeIsActive;
    bool m_liveResizeOptimizationIsActive;

    // For calling set().
    FRIEND_TEST_ALL_PREFIXES(LayoutPartTest, DestroyUpdatesImageQualityController);

    // For calling setTimer(),
    FRIEND_TEST_ALL_PREFIXES(ImageQualityControllerTest, LowQualityFilterForLiveResize);
    FRIEND_TEST_ALL_PREFIXES(ImageQualityControllerTest, LowQualityFilterForResizingImage);
    FRIEND_TEST_ALL_PREFIXES(ImageQualityControllerTest, DontKickTheAnimationTimerWhenPaintingAtTheSameSize);
};

} // namespace blink

#endif
