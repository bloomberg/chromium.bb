// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/imagecapture/ImageCapture.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/ImageBitmap.h"
#include "modules/EventTargetModules.h"
#include "modules/mediastream/MediaStreamTrack.h"
#include "public/platform/Platform.h"
#include "public/platform/WebImageCaptureFrameGrabber.h"
#include "public/platform/WebMediaStreamTrack.h"

namespace blink {

ImageCapture* ImageCapture::create(ExecutionContext* context, MediaStreamTrack* track, ExceptionState& exceptionState)
{
    if (track->kind() != "video") {
        exceptionState.throwDOMException(NotSupportedError, "Cannot create an ImageCapturer from a non-video Track.");
        return nullptr;
    }

    return new ImageCapture(context, track);
}

ImageCapture::~ImageCapture()
{
    DCHECK(!hasEventListeners());
}

const AtomicString& ImageCapture::interfaceName() const
{
    return EventTargetNames::ImageCapture;
}

ExecutionContext* ImageCapture::getExecutionContext() const
{
    return ContextLifecycleObserver::getExecutionContext();
}

bool ImageCapture::hasPendingActivity() const
{
    return hasEventListeners();
}

void ImageCapture::contextDestroyed()
{
    removeAllEventListeners();
    DCHECK(!hasEventListeners());
}

ScriptPromise ImageCapture::grabFrame(ScriptState* scriptState, ExceptionState& exceptionState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    // Spec instructs to return an exception if the track's ready state is not "live". Also reject if the track is disabled or muted.
    if (m_streamTrack->readyState() != "live" || !m_streamTrack->enabled() || m_streamTrack->muted()) {
        resolver->reject(DOMException::create(InvalidStateError, "The associated Track is in an invalid state."));
        return promise;
    }

    // Create |m_frameGrabber| the first time.
    if (!m_frameGrabber) {
        m_frameGrabber = adoptPtr(Platform::current()->createImageCaptureFrameGrabber());

    }

    if (!m_frameGrabber) {
        resolver->reject(DOMException::create(UnknownError, "Couldn't create platform resources"));
        return promise;
    }

    // The platform does not know about MediaStreamTrack, so we wrap it up.
    WebMediaStreamTrack track(m_streamTrack->component());
    m_frameGrabber->grabFrame(&track, new CallbackPromiseAdapter<ImageBitmap, void>(resolver));

    return promise;
}

ImageCapture::ImageCapture(ExecutionContext* context, MediaStreamTrack* track)
    : ActiveScriptWrappable(this)
    , ContextLifecycleObserver(context)
    , m_streamTrack(track)
{
    DCHECK(m_streamTrack);
}

bool ImageCapture::addEventListenerInternal(const AtomicString& eventType, EventListener* listener, const EventListenerOptions& options)
{
    return EventTarget::addEventListenerInternal(eventType, listener, options);
}

DEFINE_TRACE(ImageCapture)
{
    visitor->trace(m_streamTrack);
    EventTargetWithInlineData::trace(visitor);
    ContextLifecycleObserver::trace(visitor);
}

} // namespace blink
