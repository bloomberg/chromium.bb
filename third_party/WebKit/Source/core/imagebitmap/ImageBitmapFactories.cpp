/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/imagebitmap/ImageBitmapFactories.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExecutionContext.h"
#include "core/fileapi/Blob.h"
#include "core/frame/ImageBitmap.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/ImageData.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/SharedBuffer.h"
#include "platform/graphics/ImageSource.h"
#include <v8.h>

namespace blink {

static inline ImageBitmapSource* toImageBitmapSourceInternal(const ImageBitmapSourceUnion& value)
{
    if (value.isHTMLImageElement())
        return value.getAsHTMLImageElement().get();
    if (value.isHTMLVideoElement())
        return value.getAsHTMLVideoElement().get();
    if (value.isHTMLCanvasElement())
        return value.getAsHTMLCanvasElement().get();
    if (value.isBlob())
        return value.getAsBlob();
    if (value.isImageData())
        return value.getAsImageData();
    if (value.isImageBitmap())
        return value.getAsImageBitmap().get();
    ASSERT_NOT_REACHED();
    return nullptr;
}

ScriptPromise ImageBitmapFactories::createImageBitmap(ScriptState* scriptState, EventTarget& eventTarget, const ImageBitmapSourceUnion& bitmapSource, ExceptionState& exceptionState)
{
    ImageBitmapSource* bitmapSourceInternal = toImageBitmapSourceInternal(bitmapSource);
    if (bitmapSourceInternal->isBlob()) {
        Blob* blob = static_cast<Blob*>(bitmapSourceInternal);
        ImageBitmapLoader* loader = ImageBitmapFactories::ImageBitmapLoader::create(from(eventTarget), IntRect(), scriptState);
        ScriptPromise promise = loader->promise();
        from(eventTarget).addLoader(loader);
        loader->loadBlobAsync(eventTarget.executionContext(), blob);
        return promise;
    }
    IntSize srcSize = bitmapSourceInternal->bitmapSourceSize();
    return createImageBitmap(scriptState, eventTarget, bitmapSourceInternal, 0, 0, srcSize.width(), srcSize.height(), exceptionState);
}

ScriptPromise ImageBitmapFactories::createImageBitmap(ScriptState* scriptState, EventTarget& eventTarget, const ImageBitmapSourceUnion& bitmapSource, int sx, int sy, int sw, int sh, ExceptionState& exceptionState)
{
    ImageBitmapSource* bitmapSourceInternal = toImageBitmapSourceInternal(bitmapSource);
    return createImageBitmap(scriptState, eventTarget, bitmapSourceInternal, sx, sy, sw, sh, exceptionState);
}

ScriptPromise ImageBitmapFactories::createImageBitmap(ScriptState* scriptState, EventTarget& eventTarget, ImageBitmapSource* bitmapSource, int sx, int sy, int sw, int sh, ExceptionState& exceptionState)
{
    if (bitmapSource->isBlob()) {
        if (!sw || !sh) {
            exceptionState.throwDOMException(IndexSizeError, String::format("The source %s provided is 0.", sw ? "height" : "width"));
            return ScriptPromise();
        }
        Blob* blob = static_cast<Blob*>(bitmapSource);
        ImageBitmapLoader* loader = ImageBitmapFactories::ImageBitmapLoader::create(from(eventTarget), IntRect(sx, sy, sw, sh), scriptState);
        ScriptPromise promise = loader->promise();
        from(eventTarget).addLoader(loader);
        loader->loadBlobAsync(eventTarget.executionContext(), blob);
        return promise;
    }

    return bitmapSource->createImageBitmap(scriptState, eventTarget, sx, sy, sw, sh, exceptionState);
}

const char* ImageBitmapFactories::supplementName()
{
    return "ImageBitmapFactories";
}

ImageBitmapFactories& ImageBitmapFactories::from(EventTarget& eventTarget)
{
    if (LocalDOMWindow* window = eventTarget.toDOMWindow())
        return fromInternal(*window);

    ASSERT(eventTarget.executionContext()->isWorkerGlobalScope());
    return ImageBitmapFactories::fromInternal(*toWorkerGlobalScope(eventTarget.executionContext()));
}

template<class GlobalObject>
ImageBitmapFactories& ImageBitmapFactories::fromInternal(GlobalObject& object)
{
    ImageBitmapFactories* supplement = static_cast<ImageBitmapFactories*>(WillBeHeapSupplement<GlobalObject>::from(object, supplementName()));
    if (!supplement) {
        supplement = new ImageBitmapFactories();
        WillBeHeapSupplement<GlobalObject>::provideTo(object, supplementName(), adoptPtrWillBeNoop(supplement));
    }
    return *supplement;
}

void ImageBitmapFactories::addLoader(ImageBitmapLoader* loader)
{
    m_pendingLoaders.add(loader);
}

void ImageBitmapFactories::didFinishLoading(ImageBitmapLoader* loader)
{
    ASSERT(m_pendingLoaders.contains(loader));
    m_pendingLoaders.remove(loader);
}

ImageBitmapFactories::ImageBitmapLoader::ImageBitmapLoader(ImageBitmapFactories& factory, const IntRect& cropRect, ScriptState* scriptState)
    : m_loader(FileReaderLoader::ReadAsArrayBuffer, this)
    , m_factory(&factory)
    , m_resolver(ScriptPromiseResolver::create(scriptState))
    , m_cropRect(cropRect)
{
}

void ImageBitmapFactories::ImageBitmapLoader::loadBlobAsync(ExecutionContext* context, Blob* blob)
{
    m_loader.start(context, blob->blobDataHandle());
}

DEFINE_TRACE(ImageBitmapFactories)
{
    visitor->trace(m_pendingLoaders);
    WillBeHeapSupplement<LocalDOMWindow>::trace(visitor);
    WillBeHeapSupplement<WorkerGlobalScope>::trace(visitor);
}

void ImageBitmapFactories::ImageBitmapLoader::rejectPromise()
{
    m_resolver->reject(ScriptValue(m_resolver->scriptState(), v8::Null(m_resolver->scriptState()->isolate())));
    m_factory->didFinishLoading(this);
}

void ImageBitmapFactories::ImageBitmapLoader::didFinishLoading()
{
    if (!m_loader.arrayBufferResult()) {
        rejectPromise();
        return;
    }
    RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create((char*)m_loader.arrayBufferResult()->data(), static_cast<size_t>(m_loader.arrayBufferResult()->byteLength()));

    OwnPtr<ImageSource> source = adoptPtr(new ImageSource());
    source->setData(*sharedBuffer, true);

    RefPtr<SkImage> frame = source->createFrameAtIndex(0);
    ASSERT(!frame || (frame->width() && frame->height()));
    if (!frame) {
        rejectPromise();
        return;
    }

    RefPtr<StaticBitmapImage> image = StaticBitmapImage::create(frame);
    image->setOriginClean(true);
    if (!m_cropRect.width() && !m_cropRect.height()) {
        // No cropping variant was called.
        m_cropRect = IntRect(IntPoint(), image->size());
    }

    RefPtrWillBeRawPtr<ImageBitmap> imageBitmap = ImageBitmap::create(image, m_cropRect);
    m_resolver->resolve(imageBitmap.release());
    m_factory->didFinishLoading(this);
}

void ImageBitmapFactories::ImageBitmapLoader::didFail(FileError::ErrorCode)
{
    rejectPromise();
}

DEFINE_TRACE(ImageBitmapFactories::ImageBitmapLoader)
{
    visitor->trace(m_factory);
    visitor->trace(m_resolver);
}

} // namespace blink
