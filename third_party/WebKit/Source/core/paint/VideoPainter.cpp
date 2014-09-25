// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/VideoPainter.h"

#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLVideoElement.h"
#include "core/paint/ImagePainter.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderVideo.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/media/MediaPlayer.h"

namespace blink {

void VideoPainter::paintReplaced(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    WebMediaPlayer* mediaPlayer = m_renderVideo.mediaElement()->webMediaPlayer();
    bool displayingPoster = m_renderVideo.videoElement()->shouldDisplayPosterImage();
    if (!displayingPoster && !mediaPlayer)
        return;

    LayoutRect rect = m_renderVideo.videoBox();
    if (rect.isEmpty())
        return;
    rect.moveBy(paintOffset);

    LayoutRect contentRect = m_renderVideo.contentBoxRect();
    contentRect.moveBy(paintOffset);
    GraphicsContext* context = paintInfo.context;
    bool clip = !contentRect.contains(rect);
    if (clip) {
        context->save();
        context->clip(contentRect);
    }

    if (displayingPoster)
        ImagePainter(m_renderVideo).paintIntoRect(context, rect);
    else if ((m_renderVideo.document().view() && m_renderVideo.document().view()->paintBehavior() & PaintBehaviorFlattenCompositingLayers) || !m_renderVideo.acceleratedRenderingInUse())
        m_renderVideo.videoElement()->paintCurrentFrameInContext(context, pixelSnappedIntRect(rect));

    if (clip)
        context->restore();
}

} // namespace blink
