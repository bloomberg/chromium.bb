// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PictureInPictureController_h
#define PictureInPictureController_h

#include "core/frame/LocalFrame.h"
#include "modules/ModulesExport.h"

namespace blink {

class HTMLVideoElement;

class MODULES_EXPORT PictureInPictureController
    : public GarbageCollectedFinalized<PictureInPictureController>,
      public Supplement<Document> {
  USING_GARBAGE_COLLECTED_MIXIN(PictureInPictureController);
  WTF_MAKE_NONCOPYABLE(PictureInPictureController);

 public:
  virtual ~PictureInPictureController();

  static PictureInPictureController& Ensure(Document&);

  static const char* SupplementName();

  bool PictureInPictureEnabled() const;

  void SetPictureInPictureEnabledForTesting(bool);

  enum class Status {
    kEnabled,
    kDisabledBySystem,
    kDisabledByFeaturePolicy,
    kDisabledByAttribute,
  };

  Status IsDocumentAllowed() const;

  Status IsElementAllowed(HTMLVideoElement&) const;

  void SetPictureInPictureElement(HTMLVideoElement&);

  void UnsetPictureInPictureElement();

  HTMLVideoElement* PictureInPictureElement() const;

  void Trace(blink::Visitor*) override;

 private:
  explicit PictureInPictureController(Document&);

  bool picture_in_picture_enabled_ = true;

  Member<HTMLVideoElement> picture_in_picture_element_;
};

}  // namespace blink

#endif  // PictureInPictureController_h
