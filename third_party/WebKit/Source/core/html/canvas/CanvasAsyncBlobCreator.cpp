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
    m_numRowsCompleted = 0;
}

CanvasAsyncBlobCreator::~CanvasAsyncBlobCreator()
{
}

void CanvasAsyncBlobCreator::scheduleAsyncBlobCreation(bool canUseIdlePeriodScheduling, double quality)
{
    // TODO: async blob creation should be supported in worker_pool threads as well. but right now blink does not have that
    ASSERT(isMainThread());

    // At the time being, progressive encoding is only applicable to png image format,
    // and thus idle tasks scheduling can only be applied to png image format.
    // TODO(xlai): Progressive encoding on jpeg and webp image formats (crbug.com/571398, crbug.com/571399)
    if (canUseIdlePeriodScheduling) {
        ASSERT(m_mimeType == "image/png");
        Platform::current()->mainThread()->scheduler()->postIdleTask(BLINK_FROM_HERE, bind<double>(&CanvasAsyncBlobCreator::initiatePngEncoding, this));
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
        Platform::current()->mainThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, bind(&BlobCallback::handleEvent, m_callback, nullptr));
        return;
    }
    BackgroundTaskRunner::TaskSize taskSize = (m_size.height() * m_size.width() >= LongTaskImageSizeThreshold) ? BackgroundTaskRunner::TaskSizeLongRunningTask : BackgroundTaskRunner::TaskSizeShortRunningTask;
    BackgroundTaskRunner::postOnBackgroundThread(BLINK_FROM_HERE, threadSafeBind(&CanvasAsyncBlobCreator::encodeImageOnEncoderThread, AllowCrossThreadAccess(this), quality), taskSize);
}

void CanvasAsyncBlobCreator::initiatePngEncoding(double deadlineSeconds)
{
    ASSERT(isMainThread());
    m_pngEncoderState = PNGImageEncoderState::create(m_size, m_encodedImage.get());
    if (!m_pngEncoderState) {
        Platform::current()->mainThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, bind(&BlobCallback::handleEvent, m_callback, nullptr));
        return;
    }

    CanvasAsyncBlobCreator::idleEncodeRowsPng(deadlineSeconds);
}

void CanvasAsyncBlobCreator::scheduleIdleEncodeRowsPng()
{
    ASSERT(isMainThread());
    Platform::current()->currentThread()->scheduler()->postIdleTask(BLINK_FROM_HERE, WTF::bind<double>(&CanvasAsyncBlobCreator::idleEncodeRowsPng, this));
}

void CanvasAsyncBlobCreator::idleEncodeRowsPng(double deadlineSeconds)
{
    ASSERT(isMainThread());
    unsigned char* inputPixels = m_data->data() + m_pixelRowStride * m_numRowsCompleted;
    for (int y = m_numRowsCompleted; y < m_size.height(); ++y) {
        if (isDeadlineNearOrPassed(deadlineSeconds)) {
            m_numRowsCompleted = y;
            CanvasAsyncBlobCreator::scheduleIdleEncodeRowsPng();
            return;
        }
        PNGImageEncoder::writeOneRowToPng(inputPixels, m_pngEncoderState.get());
        inputPixels += m_pixelRowStride;
    }
    m_numRowsCompleted = m_size.height();
    PNGImageEncoder::finalizePng(m_pngEncoderState.get());

    if (isDeadlineNearOrPassed(deadlineSeconds))
        Platform::current()->mainThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, bind(&CanvasAsyncBlobCreator::createBlobAndCall, this));
    else
        this->createBlobAndCall();
}

void CanvasAsyncBlobCreator::createBlobAndCall()
{
    ASSERT(isMainThread());
    Blob* resultBlob = Blob::create(m_encodedImage->data(), m_encodedImage->size(), m_mimeType);
    Platform::current()->mainThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, bind(&BlobCallback::handleEvent, m_callback, resultBlob));
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

    Platform::current()->mainThread()->getWebTaskRunner()->postTask(BLINK_FROM_HERE, threadSafeBind(&CanvasAsyncBlobCreator::createBlobAndCall, AllowCrossThreadAccess(this)));
}

} // namespace blink
