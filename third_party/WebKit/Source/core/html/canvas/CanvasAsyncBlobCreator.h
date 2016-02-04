// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/CoreExport.h"
#include "core/dom/DOMTypedArray.h"
#include "core/fileapi/BlobCallback.h"
#include "platform/geometry/IntSize.h"
#include "platform/heap/Handle.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class PNGImageEncoderState;

class CORE_EXPORT CanvasAsyncBlobCreator
    : public RefCounted<CanvasAsyncBlobCreator> {
public:
    static PassRefPtr<CanvasAsyncBlobCreator> create(PassRefPtr<DOMUint8ClampedArray> unpremultipliedRGBAImageData, const String& mimeType, const IntSize&, BlobCallback*);
    void scheduleAsyncBlobCreation(bool canUseIdlePeriodScheduling, double quality = 0.0);
    virtual ~CanvasAsyncBlobCreator();

private:
    CanvasAsyncBlobCreator(PassRefPtr<DOMUint8ClampedArray> data, const String& mimeType, const IntSize&, BlobCallback*);
    void scheduleCreateBlobAndCallOnMainThread();
    void scheduleCreateNullptrAndCallOnMainThread();
    void scheduleClearSelfRefOnMainThread();

    OwnPtr<PNGImageEncoderState> m_encoderState;
    RefPtr<DOMUint8ClampedArray> m_data;
    OwnPtr<Vector<unsigned char>> m_encodedImage;
    int m_numRowsCompleted;

    const IntSize m_size;
    size_t m_pixelRowStride;
    const String m_mimeType;
    CrossThreadPersistent<BlobCallback> m_callback;

    RefPtr<CanvasAsyncBlobCreator> m_selfRef;
    void clearSelfReference();

    void initiatePngEncoding(double deadlineSeconds);
    void scheduleIdleEncodeRowsPng();
    void idleEncodeRowsPng(double deadlineSeconds);

    void createBlobAndCall();

    void encodeImageOnEncoderThread(double quality);
};

} // namespace blink
