// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLVideoElementPictureInPicture_h
#define HTMLVideoElementPictureInPicture_h

#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class HTMLVideoElement;
class ScriptPromise;
class ScriptState;

class MODULES_EXPORT HTMLVideoElementPictureInPicture {
  STATIC_ONLY(HTMLVideoElementPictureInPicture);

 public:
  static ScriptPromise requestPictureInPicture(ScriptState*, HTMLVideoElement&);
};

}  // namespace blink

#endif  // HTMLVideoElementPictureInPicture_h
