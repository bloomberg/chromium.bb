// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/srcobject/HTMLMediaElementSrcObject.h"

#include "core/html/HTMLMediaElement.h"
#include "modules/mediastream/MediaStream.h"
#include "platform/mediastream/MediaStreamDescriptor.h"
#include "public/platform/WebMediaPlayerSource.h"
#include "public/platform/WebMediaStream.h"

namespace blink {

// static
MediaStream* HTMLMediaElementSrcObject::srcObject(HTMLMediaElement& element)
{
    const WebMediaPlayerSource& source = element.getSrcObject();
    if (source.isMediaStream()) {
        MediaStreamDescriptor* descriptor = source.getAsMediaStream();
        MediaStream* stream = toMediaStream(descriptor);
        return stream;
    }

    return nullptr;
}

// static
void HTMLMediaElementSrcObject::setSrcObject(HTMLMediaElement& element, MediaStream* mediaStream)
{
    if (!mediaStream) {
        element.setSrcObject(WebMediaPlayerSource());
        return;
    }
    WebMediaStream webStream = WebMediaStream(mediaStream->descriptor());
    WebMediaPlayerSource source(webStream);
    element.setSrcObject(source);
}

} // namespace blink
