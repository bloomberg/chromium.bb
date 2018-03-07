// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/picture_in_picture/ShadowRootPictureInPicture.h"

#include "core/dom/Document.h"
#include "modules/picture_in_picture/PictureInPictureControllerImpl.h"

namespace blink {

// static
Element* ShadowRootPictureInPicture::pictureInPictureElement(TreeScope& scope) {
  return PictureInPictureControllerImpl::From(scope.GetDocument())
      .PictureInPictureElement(scope);
}

}  // namespace blink
