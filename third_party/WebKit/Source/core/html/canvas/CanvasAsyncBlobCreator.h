// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DOMTypedArray.h"
#include "core/fileapi/FileCallback.h"
#include "platform/geometry/IntSize.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebThread.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class PNGImageEncoderState;

class CanvasAsyncBlobCreator final : public ThreadSafeRefCounted<CanvasAsyncBlobCreator> {
public:
    static PassRefPtr<CanvasAsyncBlobCreator> create(PassRefPtr<DOMUint8ClampedArray> unpremultipliedRGBAImageData, const String& mimeType, const IntSize&, FileCallback*);
    void scheduleAsyncBlobCreation(bool canUseIdlePeriodScheduling, double quality = 0.0);
    static WebThread* getToBlobThreadInstance();
    virtual ~CanvasAsyncBlobCreator();

private:
    CanvasAsyncBlobCreator(PassRefPtr<DOMUint8ClampedArray> data, const String& mimeType, const IntSize&, FileCallback*);

    OwnPtr<PNGImageEncoderState> m_encoderState;
    RefPtr<DOMUint8ClampedArray> m_data;
    OwnPtr<Vector<unsigned char>> m_encodedImage;
    int m_numRowsCompleted;

    const IntSize m_size;
    size_t m_pixelRowStride;
    const String m_mimeType;
    Persistent<FileCallback> m_callback;

    RefPtr<CanvasAsyncBlobCreator> m_selfRef;

    void initiatePngEncoding(double deadlineSeconds);
    void scheduleIdleEncodeRowsPng();
    void idleEncodeRowsPng(double deadlineSeconds);
    void createBlobAndCall();

    void encodeImageOnAsyncThread(double quality);
};

} // namespace blink
