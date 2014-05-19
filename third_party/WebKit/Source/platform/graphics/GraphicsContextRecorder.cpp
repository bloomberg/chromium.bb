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

#include "config.h"
#include "platform/graphics/GraphicsContextRecorder.h"

#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/ImageSource.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "platform/image-decoders/ImageFrame.h"
#include "third_party/skia/include/core/SkBitmapDevice.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkStream.h"

namespace WebCore {

GraphicsContext* GraphicsContextRecorder::record(const IntSize& size, bool isCertainlyOpaque)
{
    ASSERT(!m_picture);
    ASSERT(!m_recorder);
    ASSERT(!m_context);
    m_isCertainlyOpaque = isCertainlyOpaque;
    m_recorder = adoptPtr(new SkPictureRecorder);
    SkCanvas* canvas = m_recorder->beginRecording(size.width(), size.height(), 0, 0);
    m_context = adoptPtr(new GraphicsContext(canvas));
    m_context->setTrackOpaqueRegion(isCertainlyOpaque);
    m_context->setCertainlyOpaque(isCertainlyOpaque);
    return m_context.get();
}

PassRefPtr<GraphicsContextSnapshot> GraphicsContextRecorder::stop()
{
    m_context.clear();
    m_picture = adoptRef(m_recorder->endRecording());
    m_recorder.clear();
    return adoptRef(new GraphicsContextSnapshot(m_picture.release(), m_isCertainlyOpaque));
}

GraphicsContextSnapshot::GraphicsContextSnapshot(PassRefPtr<SkPicture> picture, bool isCertainlyOpaque)
    : m_picture(picture)
    , m_isCertainlyOpaque(isCertainlyOpaque)
{
}


class SnapshotPlayer : public SkDrawPictureCallback {
    WTF_MAKE_NONCOPYABLE(SnapshotPlayer);
public:
    explicit SnapshotPlayer(PassRefPtr<SkPicture> picture, SkCanvas* canvas)
        : m_picture(picture)
        , m_canvas(canvas)
    {
    }

    SkCanvas* canvas() { return m_canvas; }

    void play()
    {
        m_picture->draw(m_canvas, this);
    }

private:
    RefPtr<SkPicture> m_picture;
    SkCanvas* m_canvas;
};

class FragmentSnapshotPlayer : public SnapshotPlayer {
public:
    FragmentSnapshotPlayer(PassRefPtr<SkPicture> picture, SkCanvas* canvas)
        : SnapshotPlayer(picture, canvas)
    {
    }

    void play(unsigned fromStep, unsigned toStep)
    {
        m_fromStep = fromStep;
        m_toStep = toStep;
        m_stepCount = 0;
        SnapshotPlayer::play();
    }

    virtual bool abortDrawing() OVERRIDE
    {
        ++m_stepCount;
        if (m_stepCount == m_fromStep) {
            const SkBitmap& bitmap = canvas()->getDevice()->accessBitmap(true);
            bitmap.eraseARGB(0, 0, 0, 0); // FIXME: get layers background color, it might make resulting image a bit more plausable.
        }
        return m_toStep && m_stepCount > m_toStep;
    }

private:
    unsigned m_fromStep;
    unsigned m_toStep;
    unsigned m_stepCount;
};

class ProfilingSnapshotPlayer : public SnapshotPlayer {
public:
    ProfilingSnapshotPlayer(PassRefPtr<SkPicture> picture, SkCanvas* canvas)
        : SnapshotPlayer(picture, canvas)
    {
    }

    void play(GraphicsContextSnapshot::Timings* timingsVector, unsigned minRepeatCount, double minDuration)
    {
        m_timingsVector = timingsVector;
        m_timingsVector->reserveCapacity(minRepeatCount);

        double now = WTF::monotonicallyIncreasingTime();
        double stopTime = now + minDuration;
        for (unsigned step = 0; step < minRepeatCount || now < stopTime; ++step) {
            m_timingsVector->append(Vector<double>());
            m_currentTimings = &m_timingsVector->last();
            if (m_timingsVector->size() > 1)
                m_currentTimings->reserveCapacity(m_timingsVector->begin()->size());
            SnapshotPlayer::play();
            now = WTF::monotonicallyIncreasingTime();
            m_currentTimings->append(now);
        }
    }

    virtual bool abortDrawing() OVERRIDE
    {
        m_currentTimings->append(WTF::monotonicallyIncreasingTime());
        return false;
    }

    const GraphicsContextSnapshot::Timings& timingsVector() const { return *m_timingsVector; }

private:
    GraphicsContextSnapshot::Timings* m_timingsVector;
    Vector<double>* m_currentTimings;
};

class LoggingSnapshotPlayer : public SnapshotPlayer {
public:
    LoggingSnapshotPlayer(PassRefPtr<SkPicture> picture, SkCanvas* canvas)
        : SnapshotPlayer(picture, canvas)
    {
    }

    virtual bool abortDrawing() OVERRIDE
    {
        return false;
    }
};

class LoggingCanvas : public SkCanvas {
public:
    LoggingCanvas()
    {
        m_log = JSONArray::create();
    }

    void clear(SkColor color) OVERRIDE
    {
        addItemWithParams("clear")->setNumber("color", color);
    }

    void drawPaint(const SkPaint& paint) OVERRIDE
    {
        addItemWithParams("drawPaint")->setObject("paint", objectForSkPaint(paint));
    }

    void drawPoints(PointMode mode, size_t count, const SkPoint pts[], const SkPaint& paint) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawPoints");
        params->setString("pointMode", pointModeName(mode));
        params->setArray("points", arrayForSkPoints(count, pts));
        params->setObject("paint", objectForSkPaint(paint));
    }

    void drawPicture(SkPicture& picture) OVERRIDE
    {
        addItemWithParams("drawPicture")->setObject("picture", objectForSkPicture(picture));
    }

    void drawRect(const SkRect& rect, const SkPaint& paint) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawRect");
        params->setObject("rect", objectForSkRect(rect));
        params->setObject("paint", objectForSkPaint(paint));
    }

    void drawOval(const SkRect& oval, const SkPaint& paint) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawOval");
        params->setObject("oval", objectForSkRect(oval));
        params->setObject("paint", objectForSkPaint(paint));
    }

    void drawRRect(const SkRRect& rrect, const SkPaint& paint) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawRRect");
        params->setObject("rrect", objectForSkRRect(rrect));
        params->setObject("paint", objectForSkPaint(paint));
    }

    void drawPath(const SkPath& path, const SkPaint& paint) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawPath");
        params->setObject("path", objectForSkPath(path));
        params->setObject("paint", objectForSkPaint(paint));
    }

    void drawBitmap(const SkBitmap& bitmap, SkScalar left, SkScalar top, const SkPaint* paint) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawBitmap");
        params->setNumber("left", left);
        params->setNumber("top", top);
        params->setObject("bitmap", objectForSkBitmap(bitmap));
        params->setObject("paint", objectForSkPaint(*paint));
    }

    void drawBitmapRectToRect(const SkBitmap& bitmap, const SkRect* src, const SkRect& dst, const SkPaint* paint, DrawBitmapRectFlags flags) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawBitmapRectToRect");
        params->setObject("bitmap", objectForSkBitmap(bitmap));
        params->setObject("src", objectForSkRect(*src));
        params->setObject("dst", objectForSkRect(dst));
        params->setObject("paint", objectForSkPaint(*paint));
        params->setNumber("flags", flags);
    }

    void drawBitmapMatrix(const SkBitmap& bitmap, const SkMatrix& m, const SkPaint* paint) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawBitmapMatrix");
        params->setObject("bitmap", objectForSkBitmap(bitmap));
        params->setArray("matrix", arrayForSkMatrix(m));
        params->setObject("paint", objectForSkPaint(*paint));
    }

    void drawBitmapNine(const SkBitmap& bitmap, const SkIRect& center, const SkRect& dst, const SkPaint* paint) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawBitmapNine");
        params->setObject("bitmap", objectForSkBitmap(bitmap));
        params->setObject("center", objectForSkIRect(center));
        params->setObject("dst", objectForSkRect(dst));
        params->setObject("paint", objectForSkPaint(*paint));
    }

    void drawSprite(const SkBitmap& bitmap, int left, int top, const SkPaint* paint) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawSprite");
        params->setObject("bitmap", objectForSkBitmap(bitmap));
        params->setNumber("left", left);
        params->setNumber("top", top);
        params->setObject("paint", objectForSkPaint(*paint));
    }

    void drawVertices(VertexMode vmode, int vertexCount, const SkPoint vertices[], const SkPoint texs[], const SkColor colors[], SkXfermode* xmode,
        const uint16_t indices[], int indexCount, const SkPaint& paint) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawVertices");
        params->setObject("paint", objectForSkPaint(paint));
    }

    void drawData(const void* data, size_t length) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("drawData");
        params->setNumber("length", length);
    }

    void beginCommentGroup(const char* description) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("beginCommentGroup");
        params->setString("description", description);
    }

    void addComment(const char* keyword, const char* value) OVERRIDE
    {
        RefPtr<JSONObject> params = addItemWithParams("addComment");
        params->setString("key", keyword);
        params->setString("value", value);
    }

    void endCommentGroup() OVERRIDE
    {
        addItem("endCommentGroup");
    }

    PassRefPtr<JSONArray> log()
    {
        return m_log;
    }

private:
    RefPtr<JSONArray> m_log;

    PassRefPtr<JSONObject> addItem(const String& name)
    {
        RefPtr<JSONObject> item = JSONObject::create();
        item->setString("method", name);
        m_log->pushObject(item);
        return item.release();
    }

    PassRefPtr<JSONObject> addItemWithParams(const String& name)
    {
        RefPtr<JSONObject> item = addItem(name);
        RefPtr<JSONObject> params = JSONObject::create();
        item->setObject("params", params);
        return params.release();
    }

    PassRefPtr<JSONObject> objectForSkRect(const SkRect& rect)
    {
        RefPtr<JSONObject> rectItem = JSONObject::create();
        rectItem->setNumber("left", rect.left());
        rectItem->setNumber("top", rect.top());
        rectItem->setNumber("right", rect.right());
        rectItem->setNumber("bottom", rect.bottom());
        return rectItem.release();
    }

    PassRefPtr<JSONObject> objectForSkIRect(const SkIRect& rect)
    {
        RefPtr<JSONObject> rectItem = JSONObject::create();
        rectItem->setNumber("left", rect.left());
        rectItem->setNumber("top", rect.top());
        rectItem->setNumber("right", rect.right());
        rectItem->setNumber("bottom", rect.bottom());
        return rectItem.release();
    }

    String pointModeName(PointMode mode)
    {
        switch (mode) {
        case SkCanvas::kPoints_PointMode: return "Points";
        case SkCanvas::kLines_PointMode: return "Lines";
        case SkCanvas::kPolygon_PointMode: return "Polygon";
        default:
            ASSERT_NOT_REACHED();
            return "?";
        };
    }

    PassRefPtr<JSONObject> objectForSkPoint(const SkPoint& point)
    {
        RefPtr<JSONObject> pointItem = JSONObject::create();
        pointItem->setNumber("x", point.x());
        pointItem->setNumber("y", point.y());
        return pointItem.release();
    }

    PassRefPtr<JSONArray> arrayForSkPoints(size_t count, const SkPoint points[])
    {
        RefPtr<JSONArray> pointsArrayItem = JSONArray::create();
        for (size_t i = 0; i < count; ++i)
            pointsArrayItem->pushObject(objectForSkPoint(points[i]));
        return pointsArrayItem.release();
    }

    PassRefPtr<JSONObject> objectForSkPicture(const SkPicture& picture)
    {
        RefPtr<JSONObject> pictureItem = JSONObject::create();
        pictureItem->setNumber("width", picture.width());
        pictureItem->setNumber("height", picture.height());
        return pictureItem.release();
    }

    PassRefPtr<JSONObject> objectForRadius(const SkRRect& rrect, SkRRect::Corner corner)
    {
        RefPtr<JSONObject> radiusItem = JSONObject::create();
        SkVector radius = rrect.radii(corner);
        radiusItem->setNumber("xRadius", radius.x());
        radiusItem->setNumber("yRadius", radius.y());
        return radiusItem.release();
    }

    String rrectTypeName(SkRRect::Type type)
    {
        switch (type) {
        case SkRRect::kEmpty_Type: return "Empty";
        case SkRRect::kRect_Type: return "Rect";
        case SkRRect::kOval_Type: return "Oval";
        case SkRRect::kSimple_Type: return "Simple";
        case SkRRect::kNinePatch_Type: return "Nine-patch";
        case SkRRect::kComplex_Type: return "Complex";
        default:
            ASSERT_NOT_REACHED();
            return "?";
        };
    }

    String radiusName(SkRRect::Corner corner)
    {
        switch (corner) {
        case SkRRect::kUpperLeft_Corner: return "upperLeftRadius";
        case SkRRect::kUpperRight_Corner: return "upperRightRadius";
        case SkRRect::kLowerRight_Corner: return "lowerRightRadius";
        case SkRRect::kLowerLeft_Corner: return "lowerLeftRadius";
        default:
            ASSERT_NOT_REACHED();
            return "?";
        }
    }

    PassRefPtr<JSONObject> objectForSkRRect(const SkRRect& rrect)
    {
        RefPtr<JSONObject> rrectItem = JSONObject::create();
        rrectItem->setString("type", rrectTypeName(rrect.type()));
        rrectItem->setNumber("left", rrect.rect().left());
        rrectItem->setNumber("top", rrect.rect().top());
        rrectItem->setNumber("right", rrect.rect().right());
        rrectItem->setNumber("bottom", rrect.rect().bottom());
        for (int i = 0; i < 4; ++i)
            rrectItem->setObject(radiusName((SkRRect::Corner) i), objectForRadius(rrect, (SkRRect::Corner) i));
        return rrectItem.release();
    }

    String fillTypeName(SkPath::FillType type)
    {
        switch (type) {
        case SkPath::kWinding_FillType: return "Winding";
        case SkPath::kEvenOdd_FillType: return "EvenOdd";
        case SkPath::kInverseWinding_FillType: return "InverseWinding";
        case SkPath::kInverseEvenOdd_FillType: return "InverseEvenOdd";
        default:
            ASSERT_NOT_REACHED();
            return "?";
        };
    }

    String convexityName(SkPath::Convexity convexity)
    {
        switch (convexity) {
        case SkPath::kUnknown_Convexity: return "Unknown";
        case SkPath::kConvex_Convexity: return "Convex";
        case SkPath::kConcave_Convexity: return "Concave";
        default:
            ASSERT_NOT_REACHED();
            return "?";
        };
    }

    String verbName(SkPath::Verb verb)
    {
        switch (verb) {
        case SkPath::kMove_Verb: return "Move";
        case SkPath::kLine_Verb: return "Line";
        case SkPath::kQuad_Verb: return "Quad";
        case SkPath::kConic_Verb: return "Conic";
        case SkPath::kCubic_Verb: return "Cubic";
        case SkPath::kClose_Verb: return "Close";
        case SkPath::kDone_Verb: return "Done";
        default:
            ASSERT_NOT_REACHED();
            return "?";
        };
    }

    struct VerbParams {
        String name;
        unsigned pointCount;
        unsigned pointOffset;

        VerbParams(const String& name, unsigned pointCount, unsigned pointOffset)
            : name(name)
            , pointCount(pointCount)
            , pointOffset(pointOffset) { }
    };

    VerbParams segmentParams(SkPath::Verb verb)
    {
        switch (verb) {
        case SkPath::kMove_Verb: return VerbParams("Move", 1, 0);
        case SkPath::kLine_Verb: return VerbParams("Line", 1, 1);
        case SkPath::kQuad_Verb: return VerbParams("Quad", 2, 1);
        case SkPath::kConic_Verb: return VerbParams("Conic", 2, 1);
        case SkPath::kCubic_Verb: return VerbParams("Cubic", 3, 1);
        case SkPath::kClose_Verb: return VerbParams("Close", 0, 0);
        case SkPath::kDone_Verb: return VerbParams("Done", 0, 0);
        default:
            ASSERT_NOT_REACHED();
            return VerbParams("?", 0, 0);
        };
    }

    PassRefPtr<JSONObject> objectForSkPath(const SkPath& path)
    {
        RefPtr<JSONObject> pathItem = JSONObject::create();
        pathItem->setString("fillType", fillTypeName(path.getFillType()));
        pathItem->setString("convexity", convexityName(path.getConvexity()));
        pathItem->setBoolean("isRect", path.isRect(0));
        SkPath::Iter iter(path, false);
        SkPoint points[4];
        RefPtr<JSONArray> pathPointsArray = JSONArray::create();
        for (SkPath::Verb verb = iter.next(points, false); verb != SkPath::kDone_Verb; verb = iter.next(points, false)) {
            VerbParams verbParams = segmentParams(verb);
            RefPtr<JSONObject> pathPointItem = JSONObject::create();
            pathPointItem->setString("verb", verbParams.name);
            ASSERT(verbParams.pointCount + verbParams.pointOffset <= WTF_ARRAY_LENGTH(points));
            pathPointItem->setArray("points", arrayForSkPoints(verbParams.pointCount, points + verbParams.pointOffset));
            if (SkPath::kConic_Verb == verb)
                pathPointItem->setNumber("conicWeight", iter.conicWeight());
            pathPointsArray->pushObject(pathPointItem);
        }
        pathItem->setArray("pathPoints", pathPointsArray);
        pathItem->setObject("bounds", objectForSkRect(path.getBounds()));
        return pathItem.release();
    }

    String configName(SkBitmap::Config config)
    {
        switch (config) {
        case SkBitmap::kNo_Config: return "None";
        case SkBitmap::kA8_Config: return "A8";
        case SkBitmap::kIndex8_Config: return "Index8";
        case SkBitmap::kRGB_565_Config: return "RGB565";
        case SkBitmap::kARGB_4444_Config: return "ARGB4444";
        case SkBitmap::kARGB_8888_Config: return "ARGB8888";
        default:
            ASSERT_NOT_REACHED();
            return "?";
        };
    }

    PassRefPtr<JSONObject> objectForSkBitmap(const SkBitmap& bitmap)
    {
        RefPtr<JSONObject> bitmapItem = JSONObject::create();
        bitmapItem->setNumber("width", bitmap.width());
        bitmapItem->setNumber("height", bitmap.height());
        bitmapItem->setString("config", configName(bitmap.config()));
        bitmapItem->setBoolean("opaque", bitmap.isOpaque());
        bitmapItem->setBoolean("immutable", bitmap.isImmutable());
        bitmapItem->setBoolean("volatile", bitmap.isVolatile());
        bitmapItem->setNumber("genID", bitmap.getGenerationID());
        return bitmapItem.release();
    }

    PassRefPtr<JSONObject> objectForSkPaint(const SkPaint& paint)
    {
        RefPtr<JSONObject> paintItem = JSONObject::create();
        return paintItem.release();
    }

    PassRefPtr<JSONArray> arrayForSkMatrix(const SkMatrix& matrix)
    {
        RefPtr<JSONArray> matrixArray = JSONArray::create();
        for (int i = 0; i < 9; ++i)
            matrixArray->pushNumber(matrix[i]);
        return matrixArray.release();
    }
};

static bool decodeBitmap(const void* data, size_t length, SkBitmap* result)
{
    RefPtr<SharedBuffer> buffer = SharedBuffer::create(static_cast<const char*>(data), length);
    OwnPtr<ImageDecoder> imageDecoder = ImageDecoder::create(*buffer, ImageSource::AlphaPremultiplied, ImageSource::GammaAndColorProfileIgnored);
    if (!imageDecoder)
        return false;
    imageDecoder->setData(buffer.get(), true);
    ImageFrame* frame = imageDecoder->frameBufferAtIndex(0);
    if (!frame)
        return true;
    *result = frame->getSkBitmap();
    return true;
}

PassRefPtr<GraphicsContextSnapshot> GraphicsContextSnapshot::load(const char* data, size_t size)
{
    SkMemoryStream stream(data, size);
    RefPtr<SkPicture> picture = adoptRef(SkPicture::CreateFromStream(&stream, decodeBitmap));
    if (!picture)
        return nullptr;
    return adoptRef(new GraphicsContextSnapshot(picture, false));
}

PassOwnPtr<ImageBuffer> GraphicsContextSnapshot::replay(unsigned fromStep, unsigned toStep) const
{
    OwnPtr<ImageBuffer> imageBuffer = createImageBuffer();
    FragmentSnapshotPlayer player(m_picture, imageBuffer->context()->canvas());
    player.play(fromStep, toStep);
    return imageBuffer.release();
}

PassOwnPtr<GraphicsContextSnapshot::Timings> GraphicsContextSnapshot::profile(unsigned minRepeatCount, double minDuration) const
{
    OwnPtr<GraphicsContextSnapshot::Timings> timings = adoptPtr(new GraphicsContextSnapshot::Timings());
    OwnPtr<ImageBuffer> imageBuffer = createImageBuffer();
    ProfilingSnapshotPlayer player(m_picture, imageBuffer->context()->canvas());
    player.play(timings.get(), minRepeatCount, minDuration);
    return timings.release();
}

PassOwnPtr<ImageBuffer> GraphicsContextSnapshot::createImageBuffer() const
{
    return ImageBuffer::create(IntSize(m_picture->width(), m_picture->height()), m_isCertainlyOpaque ? Opaque : NonOpaque);
}

PassRefPtr<JSONArray> GraphicsContextSnapshot::snapshotCommandLog() const
{
    LoggingCanvas canvas;
    FragmentSnapshotPlayer player(m_picture, &canvas);
    player.play(0, 0);
    return canvas.log();
}

}
