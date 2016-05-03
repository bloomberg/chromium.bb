// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/canvas/CanvasAsyncBlobCreator.h"

#include "core/fileapi/Blob.h"
#include "platform/ThreadSafeFunctional.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/image-encoders/skia/JPEGImageEncoder.h"
#include "platform/image-encoders/skia/PNGImageEncoder.h"
#include "platform/threading/BackgroundTaskRunner.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebTaskRunner.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/CurrentTime.h"
#include "wtf/Functional.h"

namespace blink {

namespace {

const double SlackBeforeDeadline = 0.001; // a small slack period between deadline and current time for safety
const int NumChannelsPng = 4;
const int LongTaskImageSizeThreshold = 1000 * 1000; // The max image size we expect to encode in 14ms on Linux in PNG format

// The encoding task is highly likely to switch from idle task to alternative
// code path when the startTimeoutDelay is set to be below 150ms. As we want the
// majority of encoding tasks to take the usual async idle task, we set a
// lenient limit -- 200ms here. This limit still needs to be short enough for
// the latency to be negligible to the user.
const double IdleTaskStartTimeoutDelay = 200.0;
// We should be more lenient on completion timeout delay to ensure that the
// switch from idle to main thread only happens to a minority of toBlob calls
#if !OS(ANDROID)
// Png image encoding on 4k by 4k canvas on Mac HDD takes 5.7+ seconds
const double IdleTaskCompleteTimeoutDelay = 6700.0;
#else
// Png image encoding on 4k by 4k canvas on Android One takes 9.0+ seconds
const double IdleTaskCompleteTimeoutDelay = 10000.0;
#endif

bool isDeadlineNearOrPassed(double deadlineSeconds)
{
    return (deadlineSeconds - SlackBeforeDeadline - monotonicallyIncreasingTime() <= 0);
}

} // anonymous namespace

CanvasAsyncBlobCreator* CanvasAsyncBlobCreator::create(DOMUint8ClampedArray* unpremultipliedRGBAImageData, const String& mimeType, const IntSize& size, BlobCallback* callback)
{
    return new CanvasAsyncBlobCreator(unpremultipliedRGBAImageData, mimeType, size, callback);
}

CanvasAsyncBlobCreator::CanvasAsyncBlobCreator(DOMUint8ClampedArray* data, const String& mimeType, const IntSize& size, BlobCallback* callback)
    : m_data(data)
    , m_size(size)
    , m_mimeType(mimeType)
    , m_callback(callback)
{
    ASSERT(m_data->length() == (unsigned) (size.height() * size.width() * 4));
    m_encodedImage = adoptPtr(new Vector<unsigned char>());
    m_pixelRowStride = size.width() * NumChannelsPng;
    m_idleTaskStatus = IdleTaskNotSupported;
    m_numRowsCompleted = 0;
}

CanvasAsyncBlobCreator::~CanvasAsyncBlobCreator()
{
}

void CanvasAsyncBlobCreator::scheduleAsyncBlobCreation(bool canUseIdlePeriodScheduling, double quality)
{
    ASSERT(isMainThread());

    if (canUseIdlePeriodScheduling) {
        // At the time being, progressive encoding is only applicable to png image format,
        // and thus idle tasks scheduling can only be applied to png image format.
        // TODO(xlai): Progressive encoding on jpeg and webp image formats (crbug.com/571398, crbug.com/571399)
        ASSERT(m_mimeType == "image/png");
        m_idleTaskStatus = IdleTaskNotStarted;

        this->scheduleInitiatePngEncoding();
        // We post the below task to check if the above idle task isn't late.
        // There's no risk of concurrency as both tasks are on main thread.
        this->postDelayedTaskToMainThread(BLINK_FROM_HERE, new SameThreadTask(bind(&CanvasAsyncBlobCreator::idleTaskStartTimeoutEvent, this, quality)), IdleTaskStartTimeoutDelay);
    } else if (m_mimeType == "image/jpeg") {
        Platform::current()->mainThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, bind(&CanvasAsyncBlobCreator::initiateJpegEncoding, this, quality));
    } else {
        BackgroundTaskRunner::TaskSize taskSize = (m_size.height() * m_size.width() >= LongTaskImageSizeThreshold) ? BackgroundTaskRunner::TaskSizeLongRunningTask : BackgroundTaskRunner::TaskSizeShortRunningTask;
        BackgroundTaskRunner::postOnBackgroundThread(BLINK_FROM_HERE, threadSafeBind(&CanvasAsyncBlobCreator::encodeImageOnEncoderThread, AllowCrossThreadAccess(this), quality), taskSize);
    }
}

void CanvasAsyncBlobCreator::initiateJpegEncoding(const double& quality)
{
    m_jpegEncoderState = JPEGImageEncoderState::create(m_size, quality, m_encodedImage.get());
    if (!m_jpegEncoderState) {
        this->createNullAndInvokeCallback();
        return;
    }
    BackgroundTaskRunner::TaskSize taskSize = (m_size.height() * m_size.width() >= LongTaskImageSizeThreshold) ? BackgroundTaskRunner::TaskSizeLongRunningTask : BackgroundTaskRunner::TaskSizeShortRunningTask;
    BackgroundTaskRunner::postOnBackgroundThread(BLINK_FROM_HERE, threadSafeBind(&CanvasAsyncBlobCreator::encodeImageOnEncoderThread, AllowCrossThreadAccess(this), quality), taskSize);
}

void CanvasAsyncBlobCreator::scheduleInitiatePngEncoding()
{
    Platform::current()->mainThread()->scheduler()->postIdleTask(BLINK_FROM_HERE, bind<double>(&CanvasAsyncBlobCreator::initiatePngEncoding, this));
}

void CanvasAsyncBlobCreator::initiatePngEncoding(double deadlineSeconds)
{
    ASSERT(isMainThread());
    if (m_idleTaskStatus == IdleTaskSwitchedToMainThreadTask) {
        return;
    }

    ASSERT(m_idleTaskStatus == IdleTaskNotStarted);
    m_idleTaskStatus = IdleTaskStarted;

    if (!initializePngStruct()) {
        m_idleTaskStatus = IdleTaskFailed;
        return;
    }
    this->idleEncodeRowsPng(deadlineSeconds);
}

void CanvasAsyncBlobCreator::idleEncodeRowsPng(double deadlineSeconds)
{
    ASSERT(isMainThread());
    if (m_idleTaskStatus == IdleTaskSwitchedToMainThreadTask) {
        return;
    }

    unsigned char* inputPixels = m_data->data() + m_pixelRowStride * m_numRowsCompleted;
    for (int y = m_numRowsCompleted; y < m_size.height(); ++y) {
        if (isDeadlineNearOrPassed(deadlineSeconds)) {
            m_numRowsCompleted = y;
            Platform::current()->currentThread()->scheduler()->postIdleTask(BLINK_FROM_HERE, bind<double>(&CanvasAsyncBlobCreator::idleEncodeRowsPng, this));
            return;
        }
        PNGImageEncoder::writeOneRowToPng(inputPixels, m_pngEncoderState.get());
        inputPixels += m_pixelRowStride;
    }
    m_numRowsCompleted = m_size.height();
    PNGImageEncoder::finalizePng(m_pngEncoderState.get());

    m_idleTaskStatus = IdleTaskCompleted;

    if (isDeadlineNearOrPassed(deadlineSeconds)) {
        Platform::current()->mainThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, bind(&CanvasAsyncBlobCreator::createBlobAndInvokeCallback, this));
    } else {
        this->createBlobAndInvokeCallback();
    }
}

void CanvasAsyncBlobCreator::encodeRowsPngOnMainThread()
{
    ASSERT(m_idleTaskStatus == IdleTaskSwitchedToMainThreadTask);

    // Continue encoding from the last completed row
    unsigned char* inputPixels = m_data->data() + m_pixelRowStride * m_numRowsCompleted;
    for (int y = m_numRowsCompleted; y < m_size.height(); ++y) {
        PNGImageEncoder::writeOneRowToPng(inputPixels, m_pngEncoderState.get());
        inputPixels += m_pixelRowStride;
    }
    PNGImageEncoder::finalizePng(m_pngEncoderState.get());
    this->createBlobAndInvokeCallback();

    this->signalAlternativeCodePathFinishedForTesting();
}

void CanvasAsyncBlobCreator::createBlobAndInvokeCallback()
{
    ASSERT(isMainThread());
    Blob* resultBlob = Blob::create(m_encodedImage->data(), m_encodedImage->size(), m_mimeType);
    Platform::current()->mainThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, bind(&BlobCallback::handleEvent, m_callback, resultBlob));
    // Since toBlob is done, timeout events are no longer needed. So we clear
    // non-GC members to allow teardown of CanvasAsyncBlobCreator.
    m_data.clear();
    m_callback.clear();
}

void CanvasAsyncBlobCreator::createNullAndInvokeCallback()
{
    ASSERT(isMainThread());
    Platform::current()->mainThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, bind(&BlobCallback::handleEvent, m_callback, nullptr));
    // Since toBlob is done (failed), timeout events are no longer needed. So we
    // clear non-GC members to allow teardown of CanvasAsyncBlobCreator.
    m_data.clear();
    m_callback.clear();
}

void CanvasAsyncBlobCreator::encodeImageOnEncoderThread(double quality)
{
    ASSERT(!isMainThread());

    bool success;
    if (m_mimeType == "image/jpeg")
        success = JPEGImageEncoder::encodeWithPreInitializedState(m_jpegEncoderState.release(), m_data->data());
    else
        success = ImageDataBuffer(m_size, m_data->data()).encodeImage(m_mimeType, quality, m_encodedImage.get());

    if (!success) {
        Platform::current()->mainThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, threadSafeBind(&BlobCallback::handleEvent, m_callback.get(), nullptr));
        return;
    }

    Platform::current()->mainThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, threadSafeBind(&CanvasAsyncBlobCreator::createBlobAndInvokeCallback, AllowCrossThreadAccess(this)));
}

bool CanvasAsyncBlobCreator::initializePngStruct()
{
    m_pngEncoderState = PNGImageEncoderState::create(m_size, m_encodedImage.get());
    if (!m_pngEncoderState) {
        this->createNullAndInvokeCallback();
        return false;
    }
    return true;
}


void CanvasAsyncBlobCreator::idleTaskStartTimeoutEvent(double quality)
{
    if (m_idleTaskStatus == IdleTaskStarted) {
        // Even if the task started quickly, we still want to ensure completion
        this->postDelayedTaskToMainThread(BLINK_FROM_HERE, new SameThreadTask(bind(&CanvasAsyncBlobCreator::idleTaskCompleteTimeoutEvent, this)), IdleTaskCompleteTimeoutDelay);
    } else if (m_idleTaskStatus == IdleTaskNotStarted) {
        // If the idle task does not start after a delay threshold, we will
        // force it to happen on main thread (even though it may cause more
        // janks) to prevent toBlob being postponed forever in extreme cases.
        m_idleTaskStatus = IdleTaskSwitchedToMainThreadTask;
        signalTaskSwitchInStartTimeoutEventForTesting();
        if (initializePngStruct()) {
            Platform::current()->mainThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, bind(&CanvasAsyncBlobCreator::encodeRowsPngOnMainThread, this));
        } else {
            // Failing in initialization of png struct
            this->signalAlternativeCodePathFinishedForTesting();
        }
    } else {
        ASSERT(m_idleTaskStatus == IdleTaskFailed || m_idleTaskStatus == IdleTaskCompleted);
        this->signalAlternativeCodePathFinishedForTesting();
    }

}

void CanvasAsyncBlobCreator::idleTaskCompleteTimeoutEvent()
{
    ASSERT(m_idleTaskStatus != IdleTaskNotStarted);

    if (m_idleTaskStatus == IdleTaskStarted) {
        // It has taken too long to complete for the idle task.
        m_idleTaskStatus = IdleTaskSwitchedToMainThreadTask;
        signalTaskSwitchInCompleteTimeoutEventForTesting();
        Platform::current()->mainThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, bind(&CanvasAsyncBlobCreator::encodeRowsPngOnMainThread, this));
    } else {
        ASSERT(m_idleTaskStatus == IdleTaskFailed || m_idleTaskStatus == IdleTaskCompleted);
        this->signalAlternativeCodePathFinishedForTesting();
    }
}

void CanvasAsyncBlobCreator::postDelayedTaskToMainThread(const WebTraceLocation& location, SameThreadTask* task, double delayMs)
{
    Platform::current()->mainThread()->getWebTaskRunner()->postDelayedTask(location, task, delayMs);
}

} // namespace blink
