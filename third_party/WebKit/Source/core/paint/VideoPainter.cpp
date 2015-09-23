// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/VideoPainter.h"

#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLVideoElement.h"
#include "core/layout/LayoutVideo.h"
#include "core/paint/ImagePainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintInfo.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/paint/ClipRecorder.h"

namespace blink {

void VideoPainter::paintReplaced(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    WebMediaPlayer* mediaPlayer = m_layoutVideo.mediaElement()->webMediaPlayer();
    bool displayingPoster = m_layoutVideo.videoElement()->shouldDisplayPosterImage();
    if (!displayingPoster && !mediaPlayer)
        return;

    LayoutRect rect(m_layoutVideo.videoBox());
    if (rect.isEmpty())
        return;
    rect.moveBy(paintOffset);

    GraphicsContext* context = paintInfo.context;
    LayoutRect contentRect = m_layoutVideo.contentBoxRect();
    contentRect.moveBy(paintOffset);

    Optional<ClipRecorder> clipRecorder;
    if (!contentRect.contains(rect)) {
        clipRecorder.emplace(*context, m_layoutVideo, paintInfo.displayItemTypeForClipping(), contentRect);
    }

    if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(*context, m_layoutVideo, paintInfo.phase, paintOffset))
        return;

    LayoutObjectDrawingRecorder drawingRecorder(*context, m_layoutVideo, paintInfo.phase, contentRect, paintOffset);

    if (displayingPoster) {
        ImagePainter(m_layoutVideo).paintIntoRect(context, rect);
    } else if ((paintInfo.globalPaintFlags() & GlobalPaintFlattenCompositingLayers) || !m_layoutVideo.acceleratedRenderingInUse()) {
        SkPaint videoPaint = context->fillPaint();
        videoPaint.setColor(SK_ColorBLACK);
        m_layoutVideo.videoElement()->paintCurrentFrame(context->canvas(), pixelSnappedIntRect(rect), &videoPaint);
    }
}

} // namespace blink
