// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "CanvasAsyncBlobCreator.h"

#include "core/fileapi/Blob.h"
#include "platform/ThreadSafeFunctional.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/heap/Handle.h"
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
// lenient limit -- 200ms here.
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

PassRefPtr<CanvasAsyncBlobCreator> CanvasAsyncBlobCreator::create(PassRefPtr<DOMUint8ClampedArray> unpremultipliedRGBAImageData, const String& mimeType, const IntSize& size, BlobCallback* callback)
{
    RefPtr<CanvasAsyncBlobCreator> asyncBlobCreator = adoptRef(new CanvasAsyncBlobCreator(unpremultipliedRGBAImageData, mimeType, size, callback));
    return asyncBlobCreator.release();
}

CanvasAsyncBlobCreator::CanvasAsyncBlobCreator(PassRefPtr<DOMUint8ClampedArray> data, const String& mimeType, const IntSize& size, BlobCallback* callback)
    : m_data(data)
    , m_size(size)
    , m_mimeType(mimeType)
    , m_callback(callback)
{
    ASSERT(m_data->length() == (unsigned) (size.height() * size.width() * 4));
    m_encodedImage = adoptPtr(new Vector<unsigned char>());
    m_pixelRowStride = size.width() * NumChannelsPng;
    m_idleTaskStatus = IdleTaskStatus::Default;
    m_numRowsCompleted = 0;
}

CanvasAsyncBlobCreator::~CanvasAsyncBlobCreator()
{
}

void CanvasAsyncBlobCreator::scheduleAsyncBlobCreation(bool canUseIdlePeriodScheduling, double quality)
{
    ASSERT(isMainThread());

    // Make self-reference to keep this object alive until the final task completes
    m_selfRef = this;

    if (canUseIdlePeriodScheduling) {
        // At the time being, progressive encoding is only applicable to png image format,
        // and thus idle tasks scheduling can only be applied to png image format.
        // TODO(xlai): Progressive encoding on jpeg and webp image formats (crbug.com/571398, crbug.com/571399)
        ASSERT(m_mimeType == "image/png");
        m_idleTaskStatus = IdleTaskStatus::IdleTaskNotStarted;
        m_alternativeSelfRef = this;

        this->scheduleInitiatePngEncoding();
        // We post the below task to check if the above idle task isn't late.
        // There's no risk of concurrency as both tasks are on main thread.
        this->postDelayedTaskToMainThread(BLINK_FROM_HERE, new Task(bind(&CanvasAsyncBlobCreator::idleTaskStartTimeoutEvent, this, quality)), IdleTaskStartTimeoutDelay);
    } else if (m_mimeType == "image/jpeg") {
        Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, bind(&CanvasAsyncBlobCreator::initiateJpegEncoding, this, quality));
    } else {
        BackgroundTaskRunner::TaskSize taskSize = (m_size.height() * m_size.width() >= LongTaskImageSizeThreshold) ? BackgroundTaskRunner::TaskSizeLongRunningTask : BackgroundTaskRunner::TaskSizeShortRunningTask;
        BackgroundTaskRunner::postOnBackgroundThread(BLINK_FROM_HERE, threadSafeBind(&CanvasAsyncBlobCreator::encodeImageOnEncoderThread, AllowCrossThreadAccess(this), quality), taskSize);
    }
}

void CanvasAsyncBlobCreator::initiateJpegEncoding(const double& quality)
{
    m_jpegEncoderState = JPEGImageEncoderState::create(m_size, quality, m_encodedImage.get());
    if (!m_jpegEncoderState) {
        Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, bind(&BlobCallback::handleEvent, m_callback, nullptr));
        clearSelfReference();
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
    if (m_idleTaskStatus == IdleTaskStatus::IdleTaskSwitchedToMainThreadTask) {
        // Clear the selfRef as the encoding task has already been carried out
        // on alternative code path.
        clearSelfReference();
        return;
    }

    ASSERT(m_idleTaskStatus == IdleTaskStatus::IdleTaskNotStarted);
    m_idleTaskStatus = IdleTaskStatus::IdleTaskStarted;

    if (!initializePngStruct()) {
        m_idleTaskStatus = IdleTaskStatus::IdleTaskFailed;
        // Clears the selfRef as the idle task has failed.
        clearSelfReference();
        return;
    }
    this->idleEncodeRowsPng(deadlineSeconds);
}

void CanvasAsyncBlobCreator::idleEncodeRowsPng(double deadlineSeconds)
{
    ASSERT(isMainThread());
    if (m_idleTaskStatus == IdleTaskStatus::IdleTaskSwitchedToMainThreadTask) {
        // Clear the selfRef as the encoding task has already been carried out
        // on alternative code path.
        clearSelfReference();
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

    m_idleTaskStatus = IdleTaskStatus::IdleTaskCompleted;

    if (isDeadlineNearOrPassed(deadlineSeconds)) {
        Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, bind(&CanvasAsyncBlobCreator::createBlobAndCall, this));
    } else {
        this->createBlobAndCall();
    }

    // Clears the selfRef as the idle task has come to a completion.
    Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, bind(&CanvasAsyncBlobCreator::clearSelfReference, this));
}

void CanvasAsyncBlobCreator::encodeRowsPngOnMainThread()
{
    ASSERT(m_idleTaskStatus == IdleTaskStatus::IdleTaskSwitchedToMainThreadTask);

    // Continue encoding from the last completed row
    unsigned char* inputPixels = m_data->data() + m_pixelRowStride * m_numRowsCompleted;
    for (int y = m_numRowsCompleted; y < m_size.height(); ++y) {
        PNGImageEncoder::writeOneRowToPng(inputPixels, m_pngEncoderState.get());
        inputPixels += m_pixelRowStride;
    }
    PNGImageEncoder::finalizePng(m_pngEncoderState.get());
    this->createBlobAndCall();

    // Clears alternative selfRef as the alternative code path has completed.
    this->clearAlternativeSelfReference();
}

void CanvasAsyncBlobCreator::createBlobAndCall()
{
    ASSERT(isMainThread());
    Blob* resultBlob = Blob::create(m_encodedImage->data(), m_encodedImage->size(), m_mimeType);
    Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, bind(&BlobCallback::handleEvent, m_callback, resultBlob));
}

void CanvasAsyncBlobCreator::createNullptrAndCall()
{
    ASSERT(isMainThread());
    Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, bind(&BlobCallback::handleEvent, m_callback, nullptr));
}

void CanvasAsyncBlobCreator::encodeImageOnEncoderThread(double quality)
{
    ASSERT(!isMainThread());

    bool encodeSuccess;
    if (m_mimeType == "image/jpeg") {
        JPEGImageEncoder::encodeWithPreInitializedState(m_jpegEncoderState.get(), m_data->data());
        encodeSuccess = true;
    } else {
        encodeSuccess = ImageDataBuffer(m_size, m_data->data()).encodeImage(m_mimeType, quality, m_encodedImage.get());
    }

    if (encodeSuccess) {
        Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, threadSafeBind(&CanvasAsyncBlobCreator::createBlobAndCall, AllowCrossThreadAccess(this)));
    } else {
        Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, threadSafeBind(&BlobCallback::handleEvent, m_callback.get(), nullptr));
    }

    Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, threadSafeBind(&CanvasAsyncBlobCreator::clearSelfReference, AllowCrossThreadAccess(this)));
}

bool CanvasAsyncBlobCreator::initializePngStruct()
{
    m_pngEncoderState = PNGImageEncoderState::create(m_size, m_encodedImage.get());
    if (!m_pngEncoderState) {
        this->createNullptrAndCall();
        return false;
    }
    return true;
}

void CanvasAsyncBlobCreator::clearSelfReference()
{
    // Some persistent members in CanvasAsyncBlobCreator can only be destroyed
    // on the thread that creates them. In this case, it's the main thread.
    ASSERT(isMainThread());
    m_selfRef.clear();
}

void CanvasAsyncBlobCreator::clearAlternativeSelfReference()
{
    ASSERT(isMainThread());
    m_alternativeSelfRef.clear();
}

void CanvasAsyncBlobCreator::idleTaskStartTimeoutEvent(double quality)
{
    if (m_idleTaskStatus == IdleTaskStatus::IdleTaskStarted) {
        // Even if the task started quickly, we still want to ensure completion
        this->postDelayedTaskToMainThread(BLINK_FROM_HERE, new Task(bind(&CanvasAsyncBlobCreator::idleTaskCompleteTimeoutEvent, this)), IdleTaskCompleteTimeoutDelay);
    } else if (m_idleTaskStatus == IdleTaskStatus::IdleTaskNotStarted) {
        // If the idle task does not start after a delay threshold, we will
        // force it to happen on main thread (even though it may cause more
        // janks) to prevent toBlob being postponed forever in extreme cases.
        m_idleTaskStatus = IdleTaskStatus::IdleTaskSwitchedToMainThreadTask;
        signalTaskSwitchInStartTimeoutEventForTesting();
        if (initializePngStruct()) {
            Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, bind(&CanvasAsyncBlobCreator::encodeRowsPngOnMainThread, this));
        } else {
            this->clearAlternativeSelfReference(); // As it fails on alternative path
        }
    } else {
        // No need to hold the alternativeSelfRef as the alternative code path
        // will not occur for IdleTaskFailed and IdleTaskCompleted
        this->clearAlternativeSelfReference();
    }

}

void CanvasAsyncBlobCreator::idleTaskCompleteTimeoutEvent()
{
    ASSERT(m_idleTaskStatus != IdleTaskStatus::IdleTaskNotStarted);

    if (m_idleTaskStatus == IdleTaskStatus::IdleTaskStarted) {
        // It has taken too long to complete for the idle task.
        m_idleTaskStatus = IdleTaskStatus::IdleTaskSwitchedToMainThreadTask;
        signalTaskSwitchInCompleteTimeoutEventForTesting();
        Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, bind(&CanvasAsyncBlobCreator::encodeRowsPngOnMainThread, this));
    } else {
        // No need to hold the alternativeSelfRef as the alternative code path
        // will not occur for IdleTaskFailed and IdleTaskCompleted
        this->clearAlternativeSelfReference();
    }
}

void CanvasAsyncBlobCreator::postDelayedTaskToMainThread(const WebTraceLocation& location, Task* task, double delayMs)
{
    Platform::current()->mainThread()->taskRunner()->postDelayedTask(location, task, delayMs);
}

} // namespace blink
