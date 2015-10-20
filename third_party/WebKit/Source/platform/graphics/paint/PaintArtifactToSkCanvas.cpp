// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/PaintArtifactToSkCanvas.h"

#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/PaintArtifact.h"
#include "platform/graphics/paint/PaintChunk.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/transforms/AffineTransform.h"
#include "platform/transforms/TransformationMatrix.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"

namespace blink {

namespace {

void paintDisplayItemToSkCanvas(const DisplayItem& displayItem, SkCanvas* canvas)
{
    DisplayItem::Type type = displayItem.type();

    if (DisplayItem::isDrawingType(type)) {
        canvas->drawPicture(static_cast<const DrawingDisplayItem&>(displayItem).picture());
        return;
    }
}

} // namespace

void paintArtifactToSkCanvas(const PaintArtifact& artifact, SkCanvas* canvas)
{
    SkAutoCanvasRestore restore(canvas, true);
    const DisplayItemList& displayItems = artifact.displayItemList();
    for (const PaintChunk& chunk : artifact.paintChunks()) {
        // Compute the total transformation matrix for this chunk.
        TransformationMatrix matrix;
        for (const TransformPaintPropertyNode* transformNode = chunk.properties.transform.get();
            transformNode; transformNode = transformNode->parent()) {
            TransformationMatrix localMatrix = transformNode->matrix();
            localMatrix.applyTransformOrigin(transformNode->origin());
            matrix = localMatrix * matrix;
        }

        // Set the canvas state to match the paint properties.
        canvas->setMatrix(TransformationMatrix::toSkMatrix44(matrix));

        // Draw the display items in the paint chunk.
        DisplayItemList::const_iterator begin = displayItems.begin() + chunk.beginIndex;
        DisplayItemList::const_iterator end = displayItems.begin() + chunk.endIndex;
        for (DisplayItemList::const_iterator it = begin; it != end; ++it)
            paintDisplayItemToSkCanvas(*it, canvas);
    }
}

PassRefPtr<SkPicture> paintArtifactToSkPicture(const PaintArtifact& artifact, const SkRect& bounds)
{
    SkPictureRecorder recorder;
    SkCanvas* canvas = recorder.beginRecording(bounds);
    paintArtifactToSkCanvas(artifact, canvas);
    return adoptRef(recorder.endRecordingAsPicture());
}

} // namespace blink
