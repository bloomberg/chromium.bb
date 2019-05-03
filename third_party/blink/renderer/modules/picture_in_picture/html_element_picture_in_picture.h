// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PICTURE_IN_PICTURE_HTML_ELEMENT_PICTURE_IN_PICTURE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PICTURE_IN_PICTURE_HTML_ELEMENT_PICTURE_IN_PICTURE_H_

#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class DOMException;
class HTMLElement;
class PictureInPictureOptions;
class ScriptPromise;
class ScriptState;

class HTMLElementPictureInPicture {
  STATIC_ONLY(HTMLElementPictureInPicture);

 public:
  static ScriptPromise requestPictureInPicture(
      ScriptState*,
      HTMLElement&,
      PictureInPictureOptions* options);

  static DOMException* CheckIfPictureInPictureIsAllowed(HTMLElement&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PICTURE_IN_PICTURE_HTML_ELEMENT_PICTURE_IN_PICTURE_H_
