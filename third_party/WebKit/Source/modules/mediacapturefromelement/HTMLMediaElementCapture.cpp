// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediacapturefromelement/HTMLMediaElementCapture.h"

#include "core/dom/ExceptionCode.h"
#include "core/html/HTMLMediaElement.h"
#include "modules/encryptedmedia/HTMLMediaElementEncryptedMedia.h"
#include "modules/encryptedmedia/MediaKeys.h"
#include "modules/mediastream/MediaStream.h"
#include "modules/mediastream/MediaStreamRegistry.h"
#include "platform/NotImplemented.h"
#include "platform/mediastream/MediaStreamCenter.h"
#include "public/platform/Platform.h"
#include "public/platform/WebMediaStream.h"
#include "public/platform/WebMediaStreamTrack.h"

namespace blink {

// static
MediaStream* HTMLMediaElementCapture::captureStream(HTMLMediaElement& element, ExceptionState& exceptionState)
{
    if (element.currentSrc().isNull()) {
        exceptionState.throwDOMException(NotSupportedError, "The media element must have a source.");
        return nullptr;
    }

    // Avoid capturing from EME-protected Media Elements.
    if (HTMLMediaElementEncryptedMedia::mediaKeys(element)) {
        // This exception is not defined in the spec, see https://github.com/w3c/mediacapture-fromelement/issues/20.
        exceptionState.throwDOMException(NotSupportedError, "Stream capture not supported with EME");
        return nullptr;
    }

    // If |element| is actually playing a MediaStream, just clone it.
    if (HTMLMediaElement::isMediaStreamURL(element.currentSrc().string())) {
        return MediaStream::create(element.executionContext(), MediaStreamRegistry::registry().lookupMediaStreamDescriptor(element.currentSrc().string()));
    }

    // TODO(mcasas): Only <video> tags are supported at the moment.
    if (element.isHTMLAudioElement()) {
        notImplemented();
        return nullptr;
    }

    WebMediaStream webStream;
    webStream.initialize(WebVector<WebMediaStreamTrack>(), WebVector<WebMediaStreamTrack>());
    MediaStreamCenter::instance().didCreateMediaStream(webStream);

    Platform::current()->createHTMLVideoElementCapturer(&webStream, element.webMediaPlayer());
    return MediaStream::create(element.executionContext(), webStream);
}

} // namespace blink
