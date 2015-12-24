// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "CanvasAsyncBlobCreator.h"

#include "core/fileapi/File.h"
#include "platform/Task.h"
#include "platform/ThreadSafeFunctional.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/image-encoders/skia/PNGImageEncoder.h"
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

bool isDeadlineNearOrPassed(double deadlineSeconds)
{
    return (deadlineSeconds - SlackBeforeDeadline - Platform::current()->monotonicallyIncreasingTimeSeconds() <= 0);
}

} // anonymous namespace

PassRefPtr<CanvasAsyncBlobCreator> CanvasAsyncBlobCreator::create(PassRefPtr<DOMUint8ClampedArray> unpremultipliedRGBAImageData, const String& mimeType, const IntSize& size, FileCallback* callback)
{
    return adoptRef(new CanvasAsyncBlobCreator(unpremultipliedRGBAImageData, mimeType, size, callback));
}

CanvasAsyncBlobCreator::CanvasAsyncBlobCreator(PassRefPtr<DOMUint8ClampedArray> data, const String& mimeType, const IntSize& size, FileCallback* callback)
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

CanvasAsyncBlobCreator::~CanvasAsyncBlobCreator() { }

void CanvasAsyncBlobCreator::scheduleAsyncBlobCreation(bool canUseIdlePeriodScheduling, double quality)
{
    // TODO: async blob creation should be supported in worker_pool threads as well. but right now blink does not have that
    ASSERT(isMainThread());

    // Make self-reference to keep this object alive until the final task completes
    m_selfRef = this;

    if (canUseIdlePeriodScheduling) {
        ASSERT(m_mimeType == "image/png");
        Platform::current()->mainThread()->scheduler()->postIdleTask(BLINK_FROM_HERE, WTF::bind<double>(&CanvasAsyncBlobCreator::initiatePngEncoding, this));
    } else {
        getToBlobThreadInstance()->taskRunner()->postTask(BLINK_FROM_HERE, new Task(threadSafeBind(&CanvasAsyncBlobCreator::encodeImageOnAsyncThread, AllowCrossThreadAccess(this), quality)));
    }
}

void CanvasAsyncBlobCreator::initiatePngEncoding(double deadlineSeconds)
{
    m_encoderState = PNGImageEncoderState::create(m_size, m_encodedImage.get());
    if (!m_encoderState) {
        Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, bind(&FileCallback::handleEvent, m_callback, nullptr));
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
    File* resultBlob = File::create(m_encodedImage->data(), m_encodedImage->size(), m_mimeType);
    Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, bind(&FileCallback::handleEvent, m_callback, resultBlob));
    m_selfRef.clear(); // self-destruct once job is done.
}

void CanvasAsyncBlobCreator::encodeImageOnAsyncThread(double quality)
{
    ASSERT(!isMainThread());

    if (!ImageDataBuffer(m_size, m_data->data()).encodeImage(m_mimeType, quality, m_encodedImage.get())) {
        Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, bind(&FileCallback::handleEvent, m_callback, nullptr));
    } else {
        Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, threadSafeBind(&CanvasAsyncBlobCreator::createBlobAndCall, AllowCrossThreadAccess(this)));
    }
}

WebThread* CanvasAsyncBlobCreator::getToBlobThreadInstance()
{
    DEFINE_STATIC_LOCAL(OwnPtr<WebThread>, s_toBlobThread, ());
    if (!s_toBlobThread) {
        s_toBlobThread = adoptPtr(Platform::current()->createThread("Async toBlob"));
    }
    return s_toBlobThread.get();
}

} // namespace blink
