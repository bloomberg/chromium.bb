// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PictureInPictureController_h
#define PictureInPictureController_h

#include "core/CoreExport.h"
#include "core/frame/LocalFrame.h"
#include "platform/Supplementable.h"

namespace blink {

// PictureInPictureController allows to know if Picture-in-Picture is allowed
// for a video element in Blink outside of modules/ module. It
// is an interface that the module will implement and add a provider for.
class CORE_EXPORT PictureInPictureController
    : public GarbageCollectedFinalized<PictureInPictureController>,
      public Supplement<Document> {
  USING_GARBAGE_COLLECTED_MIXIN(PictureInPictureController);

 public:
  static const char kSupplementName[];

  virtual ~PictureInPictureController() = default;

  // Should be called before any other call to make sure a document is attached.
  static PictureInPictureController& From(Document&);

  // List of Picture-in-Picture support statuses. If status is kEnabled,
  // Picture-in-Picture is enabled for a document or element, otherwise it is
  // not supported.
  enum class Status {
    kEnabled,
    kFrameDetached,
    kDisabledBySystem,
    kDisabledByFeaturePolicy,
    kDisabledByAttribute,
  };

  // Returns whether a given video element in a document associated with the
  // controller is allowed to request Picture-in-Picture.
  virtual Status IsElementAllowed(const HTMLVideoElement&) const = 0;

  virtual void Trace(blink::Visitor*);

 protected:
  explicit PictureInPictureController(Document&);

  DISALLOW_COPY_AND_ASSIGN(PictureInPictureController);
};

}  // namespace blink

#endif  // PictureInPictureController_h
