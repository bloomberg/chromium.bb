// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/imagebitmap/ImageBitmapSource.h"

#include "core/dom/DOMException.h"
#include "core/imagebitmap/ImageBitmap.h"
#include "core/imagebitmap/ImageBitmapOptions.h"

namespace blink {

ScriptPromise ImageBitmapSource::FulfillImageBitmap(ScriptState* script_state,
                                                    ImageBitmap* image_bitmap) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();
  if (image_bitmap && image_bitmap->BitmapImage()) {
    resolver->Resolve(image_bitmap);
  } else {
    resolver->Reject(DOMException::Create(
        kInvalidStateError, "The ImageBitmap could not be allocated."));
  }
  return promise;
}

ScriptPromise ImageBitmapSource::CreateImageBitmap(
    ScriptState* script_state,
    EventTarget& event_target,
    Optional<IntRect> crop_rect,
    const ImageBitmapOptions& options) {
  return ScriptPromise();
}

}  // namespace blink
