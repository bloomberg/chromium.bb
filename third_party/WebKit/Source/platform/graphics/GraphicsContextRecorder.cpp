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
#include "third_party/skia/include/core/SkBitmapDevice.h"

namespace WebCore {

GraphicsContext* GraphicsContextRecorder::record(const IntSize& size, bool isCertainlyOpaque)
{
    ASSERT(!m_picture);
    ASSERT(!m_recorder);
    ASSERT(!m_context);
    m_isCertainlyOpaque = isCertainlyOpaque;
    m_recorder = adoptPtr(new SkPictureRecorder);
    SkCanvas* canvas = m_recorder->beginRecording(size.width(), size.height());
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

}
