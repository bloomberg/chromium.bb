// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DocumentPictureInPicture_h
#define DocumentPictureInPicture_h

#include "platform/heap/Handle.h"

namespace blink {

class Document;
class Element;
class ScriptPromise;
class ScriptState;
class TreeScope;

class DocumentPictureInPicture {
  STATIC_ONLY(DocumentPictureInPicture);

 public:
  static bool pictureInPictureEnabled(Document&);

  static ScriptPromise exitPictureInPicture(ScriptState*, Document&);

  static Element* pictureInPictureElement(TreeScope&);
};

}  // namespace blink

#endif  // DocumentPictureInPicture_h
