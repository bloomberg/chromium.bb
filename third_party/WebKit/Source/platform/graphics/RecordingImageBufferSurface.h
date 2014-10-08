// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RecordingImageBufferSurface_h
#define RecordingImageBufferSurface_h

#include "platform/graphics/ImageBufferSurface.h"
#include "public/platform/WebThread.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "wtf/LinkedStack.h"
#include "wtf/OwnPtr.h"
#include "wtf/RefPtr.h"

class SkPicture;
class SkPictureRecorder;
class RecordingImageBufferSurfaceTest;

namespace blink {

class ImageBuffer;

class PLATFORM_EXPORT RecordingImageBufferSurface : public ImageBufferSurface {
    WTF_MAKE_NONCOPYABLE(RecordingImageBufferSurface); WTF_MAKE_FAST_ALLOCATED;
public:
    RecordingImageBufferSurface(const IntSize&, OpacityMode = NonOpaque);
    virtual ~RecordingImageBufferSurface();

    // Implementation of ImageBufferSurface interfaces
    virtual SkCanvas* canvas() const override;
    virtual PassRefPtr<SkPicture> getPicture() override;
    virtual bool isValid() const override { return true; }
    virtual void willAccessPixels() override;
    virtual void finalizeFrame(const FloatRect&) override;
    virtual void didClearCanvas() override;
    virtual void setImageBuffer(ImageBuffer*) override;

private:
    struct StateRec {
    public:
        SkMatrix m_ctm;
        // FIXME: handle transferring non-rectangular clip to the new frame, crbug.com/392614
        SkIRect m_clip;
    };
    typedef LinkedStack<StateRec> StateStack;
    friend class ::RecordingImageBufferSurfaceTest; // for unit testing
    void fallBackToRasterCanvas();
    bool initializeCurrentFrame();
    bool finalizeFrameInternal();

    // saves current clip and transform matrix of canvas
    bool saveState(SkCanvas*, StateStack*);
    // we should make sure that we can transfer state in saveState
    void setCurrentState(SkCanvas*, StateStack*);

    OwnPtr<SkPictureRecorder> m_currentFrame;
    RefPtr<SkPicture> m_previousFrame;
    OwnPtr<SkCanvas> m_rasterCanvas;
    ImageBuffer* m_imageBuffer;
    int m_initialSaveCount;
    bool m_frameWasCleared;
};

} // namespace blink

#endif
