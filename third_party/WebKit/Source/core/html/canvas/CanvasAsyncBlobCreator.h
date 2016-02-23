// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/CoreExport.h"
#include "core/dom/DOMTypedArray.h"
#include "core/fileapi/BlobCallback.h"
#include "platform/Task.h"
#include "platform/geometry/IntSize.h"
#include "platform/heap/Handle.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class PNGImageEncoderState;
class JPEGImageEncoderState;

class CORE_EXPORT CanvasAsyncBlobCreator
    : public RefCounted<CanvasAsyncBlobCreator> {
public:
    static PassRefPtr<CanvasAsyncBlobCreator> create(PassRefPtr<DOMUint8ClampedArray> unpremultipliedRGBAImageData, const String& mimeType, const IntSize&, BlobCallback*);
    void scheduleAsyncBlobCreation(bool canUseIdlePeriodScheduling, double quality = 0.0);
    virtual ~CanvasAsyncBlobCreator();
    enum IdleTaskStatus {
        IdleTaskNotStarted,
        IdleTaskStarted,
        IdleTaskCompleted,
        IdleTaskFailed,
        IdleTaskSwitchedToMainThreadTask,
        Default // Idle tasks are not implemented for some image types
    };
    virtual void signalTaskSwitchInStartTimeoutEventForTesting() { }
    virtual void signalTaskSwitchInCompleteTimeoutEventForTesting() { }

protected:
    CanvasAsyncBlobCreator(PassRefPtr<DOMUint8ClampedArray> data, const String& mimeType, const IntSize&, BlobCallback*);
    virtual void scheduleInitiatePngEncoding();
    virtual void idleEncodeRowsPng(double deadlineSeconds);
    virtual void postDelayedTaskToMainThread(const WebTraceLocation&, Task*, double delayMs);
    virtual void clearAlternativeSelfReference();
    virtual void createBlobAndCall();
    virtual void createNullptrAndCall();

    void clearSelfReference();
    void initiatePngEncoding(double deadlineSeconds);
    IdleTaskStatus m_idleTaskStatus;

private:
    friend class CanvasAsyncBlobCreatorTest;

    OwnPtr<PNGImageEncoderState> m_pngEncoderState;
    OwnPtr<JPEGImageEncoderState> m_jpegEncoderState;
    RefPtr<DOMUint8ClampedArray> m_data;
    OwnPtr<Vector<unsigned char>> m_encodedImage;
    int m_numRowsCompleted;

    const IntSize m_size;
    size_t m_pixelRowStride;
    const String m_mimeType;
    CrossThreadPersistent<BlobCallback> m_callback;

    // To keep this asyncBlobCreator alive before async encoding task completes
    // Used by all image types
    RefPtr<CanvasAsyncBlobCreator> m_selfRef;
    // To keep this asyncBlobCreator alive before the alternative code path (for
    // the case when idle task is postponed for too long) is confirmed not to
    // continue any more; Used by png image type only
    RefPtr<CanvasAsyncBlobCreator> m_alternativeSelfRef;

    bool initializePngStruct();
    void encodeRowsPngOnMainThread(); // Similar to idleEncodeRowsPng without deadline
    void encodeImageOnEncoderThread(double quality);

    void initiateJpegEncoding(const double& quality);

    void idleTaskStartTimeoutEvent(double quality);
    void idleTaskCompleteTimeoutEvent();
};

} // namespace blink
