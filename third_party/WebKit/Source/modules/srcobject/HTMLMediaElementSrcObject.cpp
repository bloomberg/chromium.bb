// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/srcobject/HTMLMediaElementSrcObject.h"

#include "core/html/media/HTMLMediaElement.h"
#include "modules/mediastream/MediaStream.h"
#include "platform/mediastream/MediaStreamDescriptor.h"

namespace blink {

// static
MediaStream* HTMLMediaElementSrcObject::srcObject(HTMLMediaElement& element) {
  MediaStreamDescriptor* descriptor = element.GetSrcObject();
  if (descriptor) {
    MediaStream* stream = ToMediaStream(descriptor);
    return stream;
  }

  return nullptr;
}

// static
void HTMLMediaElementSrcObject::setSrcObject(HTMLMediaElement& element,
                                             MediaStream* media_stream) {
  if (!media_stream) {
    element.SetSrcObject(nullptr);
    return;
  }
  element.SetSrcObject(media_stream->Descriptor());
}

}  // namespace blink
