// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLVideoElementPictureInPicture_h
#define HTMLVideoElementPictureInPicture_h

#include "core/dom/QualifiedName.h"
#include "modules/EventTargetModules.h"
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

  static bool FastHasAttribute(const QualifiedName&, const HTMLVideoElement&);

  static void SetBooleanAttribute(const QualifiedName&,
                                  HTMLVideoElement&,
                                  bool);

  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(enterpictureinpicture);
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(leavepictureinpicture);
};

}  // namespace blink

#endif  // HTMLVideoElementPictureInPicture_h
