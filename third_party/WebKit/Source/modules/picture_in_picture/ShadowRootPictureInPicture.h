// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ShadowRootPictureInPicture_h
#define ShadowRootPictureInPicture_h

#include "platform/heap/Handle.h"

namespace blink {

class Element;
class TreeScope;

class ShadowRootPictureInPicture {
  STATIC_ONLY(ShadowRootPictureInPicture);

 public:
  static Element* pictureInPictureElement(TreeScope&);
};

}  // namespace blink

#endif  // ShadowRootPictureInPicture_h
