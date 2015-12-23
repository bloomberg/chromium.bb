// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "CanvasAsyncBlobCreator.h"

#include "core/dom/ContextLifecycleObserver.h"
#include "core/fileapi/File.h"
#include "platform/Task.h"
#include "platform/ThreadSafeFunctional.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/heap/Handle.h"
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

class CanvasAsyncBlobCreator::ContextObserver final : public NoBaseWillBeGarbageCollected<CanvasAsyncBlobCreator::ContextObserver>, public ContextLifecycleObserver {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(CanvasAsyncBlobCreator::ContextObserver);
public:
    ContextObserver(ExecutionContext* executionContext, CanvasAsyncBlobCreator* asyncBlobCreator)
        : ContextLifecycleObserver(executionContext)
        , m_asyncBlobCreator(asyncBlobCreator)
    {
    }

    DECLARE_VIRTUAL_TRACE();

    void contextDestroyed() override
    {
        ContextLifecycleObserver::contextDestroyed();
        ASSERT(m_asyncBlobCreator);
        m_asyncBlobCreator->m_cancelled = true;
        // After sending cancel signal to asyncBlobCreator, the observer has
        // done its job and thus deref asyncBlobCreator for proper destruction
        m_asyncBlobCreator = nullptr;
    }

private:
    CanvasAsyncBlobCreator* m_asyncBlobCreator;
};

PassRefPtr<CanvasAsyncBlobCreator> CanvasAsyncBlobCreator::create(PassRefPtr<DOMUint8ClampedArray> unpremultipliedRGBAImageData, const String& mimeType, const IntSize& size, FileCallback* callback, ExecutionContext* executionContext)
{
    RefPtr<CanvasAsyncBlobCreator> asyncBlobCreator = adoptRef(new CanvasAsyncBlobCreator(unpremultipliedRGBAImageData, mimeType, size, callback));
    asyncBlobCreator->createContextObserver(executionContext);
    return asyncBlobCreator.release();
}

CanvasAsyncBlobCreator::CanvasAsyncBlobCreator(PassRefPtr<DOMUint8ClampedArray> data, const String& mimeType, const IntSize& size, FileCallback* callback)
    : m_cancelled(false)
    , m_data(data)
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

DEFINE_TRACE(CanvasAsyncBlobCreator::ContextObserver)
{
    ContextLifecycleObserver::trace(visitor);
}

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
        getToBlobThreadInstance()->taskRunner()->postTask(BLINK_FROM_HERE, new Task(threadSafeBind(&CanvasAsyncBlobCreator::encodeImageOnEncoderThread, AllowCrossThreadAccess(this), quality)));
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
    clearSelfReference(); // self-destruct once job is done.
}

void CanvasAsyncBlobCreator::encodeImageOnEncoderThread(double quality)
{
    ASSERT(!isMainThread());
    if (initializeEncodeImageOnEncoderThread()) {
        if (m_mimeType == "image/png") {
            // At the time being, progressive encoding is only applicable to png image format
            // TODO(xlai): Progressive encoding on jpeg and webp image formats (crbug.com/571398, crbug.com/571399)
            progressiveEncodeImageOnEncoderThread();
        } else {
            nonprogressiveEncodeImageOnEncoderThread(quality);
        }
    }
}

bool CanvasAsyncBlobCreator::initializeEncodeImageOnEncoderThread()
{
    if (m_cancelled) {
        scheduleClearSelfRefOnMainThread();
        return false;
    }

    if (m_mimeType == "image/png") {
        m_encoderState = PNGImageEncoderState::create(m_size, m_encodedImage.get());
        if (m_cancelled) {
            scheduleClearSelfRefOnMainThread();
            return false;
        }
        if (!m_encoderState) {
            scheduleCreateNullptrAndCallOnMainThread();
            return false;
        }
    } // else, do nothing; as encoding on other image formats are not progressive
    return true;
}

void CanvasAsyncBlobCreator::nonprogressiveEncodeImageOnEncoderThread(double quality)
{
    if (ImageDataBuffer(m_size, m_data->data()).encodeImage(m_mimeType, quality, m_encodedImage.get())) {
        scheduleCreateBlobAndCallOnMainThread();
    } else {
        scheduleCreateNullptrAndCallOnMainThread();
    }
}

void CanvasAsyncBlobCreator::progressiveEncodeImageOnEncoderThread()
{
    unsigned char* inputPixels = m_data->data() + m_pixelRowStride * m_numRowsCompleted;
    for (int y = 0; y < m_size.height() && !m_cancelled; ++y) {
        PNGImageEncoder::writeOneRowToPng(inputPixels, m_encoderState.get());
        inputPixels += m_pixelRowStride;
    }
    if (m_cancelled) {
        scheduleClearSelfRefOnMainThread();
        return;
    }

    PNGImageEncoder::finalizePng(m_encoderState.get());
    if (m_cancelled) {
        scheduleClearSelfRefOnMainThread();
        return;
    }

    scheduleCreateBlobAndCallOnMainThread();
}

WebThread* CanvasAsyncBlobCreator::getToBlobThreadInstance()
{
    DEFINE_STATIC_LOCAL(OwnPtr<WebThread>, s_toBlobThread, ());
    if (!s_toBlobThread) {
        s_toBlobThread = adoptPtr(Platform::current()->createThread("Async toBlob"));
    }
    return s_toBlobThread.get();
}

void CanvasAsyncBlobCreator::createContextObserver(ExecutionContext* executionContext)
{
    m_contextObserver = adoptPtrWillBeNoop(new ContextObserver(executionContext, this));
}

void CanvasAsyncBlobCreator::clearSelfReference()
{
    // Some persistent members in CanvasAsyncBlobCreator can only be destroyed
    // on the thread that creates them. In this case, it's the main thread.
    ASSERT(isMainThread());
    m_selfRef.clear();
}

void CanvasAsyncBlobCreator::scheduleClearSelfRefOnMainThread()
{
    Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, threadSafeBind(&CanvasAsyncBlobCreator::clearSelfReference, AllowCrossThreadAccess(this)));
}

void CanvasAsyncBlobCreator::scheduleCreateBlobAndCallOnMainThread()
{
    Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, threadSafeBind(&CanvasAsyncBlobCreator::createBlobAndCall, AllowCrossThreadAccess(this)));
}

void CanvasAsyncBlobCreator::scheduleCreateNullptrAndCallOnMainThread()
{
    Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, bind(&FileCallback::handleEvent, m_callback, nullptr));
    Platform::current()->mainThread()->taskRunner()->postTask(BLINK_FROM_HERE, threadSafeBind(&CanvasAsyncBlobCreator::clearSelfReference, AllowCrossThreadAccess(this)));
}

} // namespace blink
