// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/CoreExport.h"
#include "core/dom/DOMTypedArray.h"
#include "core/fileapi/BlobCallback.h"
#include "platform/geometry/IntSize.h"
#include "platform/heap/Handle.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class PNGImageEncoderState;
class JPEGImageEncoderState;

class CORE_EXPORT CanvasAsyncBlobCreator : public GarbageCollectedFinalized<CanvasAsyncBlobCreator> {
public:
    static CanvasAsyncBlobCreator* create(DOMUint8ClampedArray* unpremultipliedRGBAImageData, const String& mimeType, const IntSize&, BlobCallback*);
    void scheduleAsyncBlobCreation(bool canUseIdlePeriodScheduling, double quality = 0.0);
    virtual ~CanvasAsyncBlobCreator();

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_data);
    }

private:
    CanvasAsyncBlobCreator(DOMUint8ClampedArray* data, const String& mimeType, const IntSize&, BlobCallback*);

    OwnPtr<PNGImageEncoderState> m_pngEncoderState;
    OwnPtr<JPEGImageEncoderState> m_jpegEncoderState;
    Member<DOMUint8ClampedArray> m_data;
    OwnPtr<Vector<unsigned char>> m_encodedImage;
    int m_numRowsCompleted;

    const IntSize m_size;
    size_t m_pixelRowStride;
    const String m_mimeType;
    CrossThreadPersistent<BlobCallback> m_callback;

    void initiatePngEncoding(double deadlineSeconds);
    void scheduleIdleEncodeRowsPng();
    void idleEncodeRowsPng(double deadlineSeconds);

    void initiateJpegEncoding(const double& quality);

    void createBlobAndCall();

    void encodeImageOnEncoderThread(double quality);
};

} // namespace blink
