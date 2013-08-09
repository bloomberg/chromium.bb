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

#include "config.h"
#include "core/page/ImageBitmapFactories.h"

#include "RuntimeEnabledFeatures.h"
#include "V8ImageBitmap.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/ImageData.h"
#include "core/html/canvas/CanvasRenderingContext2D.h"
#include "core/page/DOMWindow.h"
#include "core/page/ImageBitmap.h"

namespace WebCore {

namespace ImageBitmapFactories {

static LayoutSize sizeFor(HTMLImageElement* image)
{
    if (ImageResource* cachedImage = image->cachedImage())
        return cachedImage->imageSizeForRenderer(image->renderer(), 1.0f); // FIXME: Not sure about this.
    return IntSize();
}

static IntSize sizeFor(HTMLVideoElement* video)
{
    if (MediaPlayer* player = video->player())
        return player->naturalSize();
    return IntSize();
}

static ScriptObject resolveImageBitmap(PassRefPtr<ImageBitmap> imageBitmap)
{
    // Promises must be enabled.
    ASSERT(RuntimeEnabledFeatures::promiseEnabled());

    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create();
    resolver->fulfill(imageBitmap);
    return resolver->promise();
}

ScriptObject createImageBitmap(EventTarget* eventTarget, HTMLImageElement* image, ExceptionState& es)
{
    LayoutSize s = sizeFor(image);
    return createImageBitmap(eventTarget, image, 0, 0, s.width(), s.height(), es);
}

ScriptObject createImageBitmap(EventTarget* eventTarget, HTMLImageElement* image, int sx, int sy, int sw, int sh, ExceptionState& es)
{
    if (!image) {
        es.throwTypeError();
        return ScriptObject();
    }
    if (!image->cachedImage()) {
        es.throwDOMException(InvalidStateError);
        return ScriptObject();
    }
    if (image->cachedImage()->image()->isSVGImage()) {
        es.throwDOMException(InvalidStateError);
        return ScriptObject();
    }
    if (!sw || !sh) {
        es.throwDOMException(IndexSizeError);
        return ScriptObject();
    }
    if (!image->cachedImage()->image()->hasSingleSecurityOrigin()) {
        es.throwDOMException(SecurityError);
        return ScriptObject();
    }
    if (!image->cachedImage()->passesAccessControlCheck(eventTarget->toDOMWindow()->document()->securityOrigin())
    && eventTarget->toDOMWindow()->document()->securityOrigin()->taintsCanvas(image->src())) {
        es.throwDOMException(SecurityError);
        return ScriptObject();
    }
    // FIXME: make ImageBitmap creation asynchronous crbug.com/258082
    return resolveImageBitmap(ImageBitmap::create(image, IntRect(sx, sy, sw, sh)));
}

ScriptObject createImageBitmap(EventTarget* eventTarget, HTMLVideoElement* video, ExceptionState& es)
{
    IntSize s = sizeFor(video);
    return createImageBitmap(eventTarget, video, 0, 0, s.width(), s.height(), es);
}

ScriptObject createImageBitmap(EventTarget* eventTarget, HTMLVideoElement* video, int sx, int sy, int sw, int sh, ExceptionState& es)
{
    if (!video) {
        es.throwTypeError();
        return ScriptObject();
    }
    if (!video->player()) {
        es.throwDOMException(InvalidStateError);
        return ScriptObject();
    }
    if (video->networkState() == HTMLMediaElement::NETWORK_EMPTY) {
        es.throwDOMException(InvalidStateError);
        return ScriptObject();
    }
    if (video->player()->readyState() <= MediaPlayer::HaveMetadata) {
        es.throwDOMException(InvalidStateError);
        return ScriptObject();
    }
    if (!sw || !sh) {
        es.throwDOMException(IndexSizeError);
        return ScriptObject();
    }
    if (!video->hasSingleSecurityOrigin()) {
        es.throwDOMException(SecurityError);
        return ScriptObject();
    }
    if (!video->player()->didPassCORSAccessCheck() && eventTarget->toDOMWindow()->document()->securityOrigin()->taintsCanvas(video->currentSrc())) {
        es.throwDOMException(SecurityError);
        return ScriptObject();
    }
    // FIXME: make ImageBitmap creation asynchronous crbug.com/258082
    return resolveImageBitmap(ImageBitmap::create(video, IntRect(sx, sy, sw, sh)));
}

ScriptObject createImageBitmap(EventTarget* eventTarget, CanvasRenderingContext2D* context, ExceptionState& es)
{
    return createImageBitmap(eventTarget, context->canvas(), es);
}

ScriptObject createImageBitmap(EventTarget* eventTarget, CanvasRenderingContext2D* context, int sx, int sy, int sw, int sh, ExceptionState& es)
{
    return createImageBitmap(eventTarget, context->canvas(), sx, sy, sw, sh, es);
}

ScriptObject createImageBitmap(EventTarget* eventTarget, HTMLCanvasElement* canvas, ExceptionState& es)
{
    return createImageBitmap(eventTarget, canvas, 0, 0, canvas->width(), canvas->height(), es);
}

ScriptObject createImageBitmap(EventTarget* eventTarget, HTMLCanvasElement* canvas, int sx, int sy, int sw, int sh, ExceptionState& es)
{
    if (!canvas) {
        es.throwTypeError();
        return ScriptObject();
    }
    if (!canvas->originClean()) {
        es.throwDOMException(InvalidStateError);
        return ScriptObject();
    }
    if (!sw || !sh) {
        es.throwDOMException(IndexSizeError);
        return ScriptObject();
    }
    // FIXME: make ImageBitmap creation asynchronous crbug.com/258082
    return resolveImageBitmap(ImageBitmap::create(canvas, IntRect(sx, sy, sw, sh)));
}

ScriptObject createImageBitmap(EventTarget* eventTarget, ImageData* data, ExceptionState& es)
{
    return createImageBitmap(eventTarget, data, 0, 0, data->width(), data->height(), es);
}

ScriptObject createImageBitmap(EventTarget* eventTarget, ImageData* data, int sx, int sy, int sw, int sh, ExceptionState& es)
{
    if (!data) {
        es.throwTypeError();
        return ScriptObject();
    }
    if (!sw || !sh) {
        es.throwDOMException(IndexSizeError);
        return ScriptObject();
    }
    // FIXME: make ImageBitmap creation asynchronous crbug.com/258082
    return resolveImageBitmap(ImageBitmap::create(data, IntRect(sx, sy, sw, sh)));
}

ScriptObject createImageBitmap(EventTarget* eventTarget, ImageBitmap* bitmap, ExceptionState& es)
{
    return createImageBitmap(eventTarget, bitmap, 0, 0, bitmap->width(), bitmap->height(), es);
}

ScriptObject createImageBitmap(EventTarget* eventTarget, ImageBitmap* bitmap, int sx, int sy, int sw, int sh, ExceptionState& es)
{
    if (!bitmap) {
        es.throwTypeError();
        return ScriptObject();
    }
    if (!sw || !sh) {
        es.throwDOMException(IndexSizeError);
        return ScriptObject();
    }
    // FIXME: make ImageBitmap creation asynchronous crbug.com/258082
    return resolveImageBitmap(ImageBitmap::create(bitmap, IntRect(sx, sy, sw, sh)));
}

} // namespace ImageBitmapFactories
} // namespace WebCore
