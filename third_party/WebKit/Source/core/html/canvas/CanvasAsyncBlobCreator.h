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
    enum IdleTaskStatus {
        IdleTaskNotStarted,
        IdleTaskStarted,
        IdleTaskCompleted,
        IdleTaskFailed,
        IdleTaskSwitchedToMainThreadTask,
        IdleTaskNotSupported // Idle tasks are not implemented for some image types
    };
    // Methods are virtual for mocking in unit tests
    virtual void signalTaskSwitchInStartTimeoutEventForTesting() { }
    virtual void signalTaskSwitchInCompleteTimeoutEventForTesting() { }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_data);
    }

protected:
    CanvasAsyncBlobCreator(DOMUint8ClampedArray* data, const String& mimeType, const IntSize&, BlobCallback*);
    // Methods are virtual for unit testing
    virtual void scheduleInitiatePngEncoding();
    virtual void idleEncodeRowsPng(double deadlineSeconds);
    virtual void postDelayedTaskToMainThread(const WebTraceLocation&, SameThreadTask*, double delayMs);
    virtual void signalAlternativeCodePathFinishedForTesting() { }
    virtual void createBlobAndInvokeCallback();
    virtual void createNullAndInvokeCallback();

    void initiatePngEncoding(double deadlineSeconds);
    IdleTaskStatus m_idleTaskStatus;

private:
    friend class CanvasAsyncBlobCreatorTest;

    OwnPtr<PNGImageEncoderState> m_pngEncoderState;
    OwnPtr<JPEGImageEncoderState> m_jpegEncoderState;
    Member<DOMUint8ClampedArray> m_data;
    OwnPtr<Vector<unsigned char>> m_encodedImage;
    int m_numRowsCompleted;

    const IntSize m_size;
    size_t m_pixelRowStride;
    const String m_mimeType;
    CrossThreadPersistent<BlobCallback> m_callback;

    void scheduleIdleEncodeRowsPng();

    bool initializePngStruct();
    void encodeRowsPngOnMainThread(); // Similar to idleEncodeRowsPng without deadline
    void encodeImageOnEncoderThread(double quality);

    void initiateJpegEncoding(const double& quality);

    void idleTaskStartTimeoutEvent(double quality);
    void idleTaskCompleteTimeoutEvent();
};

} // namespace blink
