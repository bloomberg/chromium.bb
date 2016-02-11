// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "CanvasAsyncBlobCreator.h"

#include "core/fileapi/Blob.h"
#include "platform/Task.h"
#include "platform/ThreadSafeFunctional.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/heap/Handle.h"
#include "platform/image-encoders/skia/PNGImageEncoder.h"
#include "platform/threading/BackgroundTaskRunner.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebTaskRunner.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/Functional.h"

namespace blink {

namespace {

const double SlackBeforeDeadline = 0.001; // a small slack period between deadline and current time for safety
const int NumChannelsPng = 4;
const int LongTaskImageSizeThreshold = 1000 * 1000; // The max image size we expect to encode in 14ms on Linux in PNG format

bool isDeadlineNearOrPassed(double deadlineSeconds)
{
    return (deadlineSeconds - SlackBeforeDeadline - Platform::current()->monotonicallyIncreasingTimeSeconds() <= 0);
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
    m_numRowsCompleted = 0;
}

CanvasAsyncBlobCreator::~CanvasAsyncBlobCreator()
{
}

void CanvasAsyncBlobCreator::scheduleAsyncBlobCreation(bool canUseIdlePeriodScheduling, double quality)
{
    // TODO: async blob creation should be supported in worker_pool threads as well. but right now blink does not have that
    ASSERT(isMainThread());

    // Make self-reference to keep this object alive until the final task completes
    m_selfRef = this;

    // At the time being, progressive encoding is only applicable to png image format,
    // and thus idle tasks scheduling can only be applied to png image format.
    // TODO(xlai): Progressive encoding on jpeg and webp image formats (crbug.com/571398, crbug.com/571399)
    if (canUseIdlePeriodScheduling) {
        ASSERT(m_mimeType == "image/png");
        Platform::current()->mainThread()->scheduler()->postIdleTask(BLINK_FROM_HERE, WTF::bind<double>(&CanvasAsyncBlobCreator::initiatePngEncoding, this));
    } else {
        BackgroundTaskRunner::TaskSize taskSize = (m_size.height() * m_size.width() >= LongTaskImageSizeThreshold) ? BackgroundTaskRunner::TaskSizeLongRunningTask : BackgroundTaskRunner::TaskSizeShortRunningTask;
        BackgroundTaskRunner::postOnBackgroundThread(BLINK_FROM_HERE, threadSafeBind(&CanvasAsyncBlobCreator::encodeImageOnEncoderThread, AllowCrossThreadAccess(this), quality), taskSize);
    }
}

void CanvasAsyncBlobCreator::initiatePngEncoding(double deadlineSeconds)
{
    m_encoderState = PNGImageEncoderState::create(m_size, m_encodedImage.get());
    if (!m_encoderState) {
        Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, bind(&BlobCallback::handleEvent, m_callback, nullptr));
        m_selfRef.clear();
        return;
    }

    CanvasAsyncBlobCreator::idleEncodeRowsPng(deadlineSeconds);
}

void CanvasAsyncBlobCreator::scheduleIdleEncodeRowsPng()
{
    Platform::current()->currentThread()->scheduler()->postIdleTask(BLINK_FROM_HERE, WTF::bind<double>(&CanvasAsyncBlobCreator::idleEncodeRowsPng, this));
}

void CanvasAsyncBlobCreator::idleEncodeRowsPng(double deadlineSeconds)
{
    unsigned char* inputPixels = m_data->data() + m_pixelRowStride * m_numRowsCompleted;
    for (int y = m_numRowsCompleted; y < m_size.height(); ++y) {
        if (isDeadlineNearOrPassed(deadlineSeconds)) {
            m_numRowsCompleted = y;
            CanvasAsyncBlobCreator::scheduleIdleEncodeRowsPng();
            return;
        }
        PNGImageEncoder::writeOneRowToPng(inputPixels, m_encoderState.get());
        inputPixels += m_pixelRowStride;
    }
    m_numRowsCompleted = m_size.height();
    PNGImageEncoder::finalizePng(m_encoderState.get());

    if (isDeadlineNearOrPassed(deadlineSeconds)) {
        Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, bind(&CanvasAsyncBlobCreator::createBlobAndCall, this));
    } else {
        this->createBlobAndCall();
    }
}

void CanvasAsyncBlobCreator::createBlobAndCall()
{
    Blob* resultBlob = Blob::create(m_encodedImage->data(), m_encodedImage->size(), m_mimeType);
    Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, bind(&BlobCallback::handleEvent, m_callback, resultBlob));
    clearSelfReference(); // self-destruct once job is done.
}

void CanvasAsyncBlobCreator::encodeImageOnEncoderThread(double quality)
{
    ASSERT(!isMainThread());

    if (ImageDataBuffer(m_size, m_data->data()).encodeImage(m_mimeType, quality, m_encodedImage.get())) {
        scheduleCreateBlobAndCallOnMainThread();
    } else {
        scheduleCreateNullptrAndCallOnMainThread();
    }
}

void CanvasAsyncBlobCreator::clearSelfReference()
{
    // Some persistent members in CanvasAsyncBlobCreator can only be destroyed
    // on the thread that creates them. In this case, it's the main thread.
    ASSERT(isMainThread());
    m_selfRef.clear();
}

void CanvasAsyncBlobCreator::scheduleCreateBlobAndCallOnMainThread()
{
    Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, threadSafeBind(&CanvasAsyncBlobCreator::createBlobAndCall, AllowCrossThreadAccess(this)));
}

void CanvasAsyncBlobCreator::scheduleCreateNullptrAndCallOnMainThread()
{
    Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, bind(&BlobCallback::handleEvent, m_callback, nullptr));
    Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, threadSafeBind(&CanvasAsyncBlobCreator::clearSelfReference, AllowCrossThreadAccess(this)));
}

} // namespace blink
