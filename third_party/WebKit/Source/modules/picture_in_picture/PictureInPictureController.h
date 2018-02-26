// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PictureInPictureController_h
#define PictureInPictureController_h

#include "core/frame/LocalFrame.h"

namespace blink {

class HTMLVideoElement;
class PictureInPictureWindow;

// The PictureInPictureController is keeping the state and implementing the
// logic around the Picture-in-Picture feature. It is meant to be used as well
// by the Picture-in-Picture Web API and internally (eg. media controls). All
// consumers inside Blink should use this class to access Picture-in-Picture. A
// PictureInPictureController instance is associated to a Document. It is
// supplement and therefore can be lazy-initiated. Callers should consider
// whether they want to instantiate an object when they make a call.
class PictureInPictureController
    : public GarbageCollectedFinalized<PictureInPictureController>,
      public Supplement<Document> {
  USING_GARBAGE_COLLECTED_MIXIN(PictureInPictureController);
  WTF_MAKE_NONCOPYABLE(PictureInPictureController);

 public:
  static const char kSupplementName[];

  virtual ~PictureInPictureController();

  static PictureInPictureController& Ensure(Document&);

  // Returns whether system allows Picture-in-Picture feature or not for
  // the associated document.
  bool PictureInPictureEnabled() const;

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

  // Returns whether the document associated with the controller is allowed to
  // request Picture-in-Picture.
  Status IsDocumentAllowed() const;

  // Returns whether a given video element in a document associated with the
  // controller is allowed to request Picture-in-Picture.
  Status IsElementAllowed(HTMLVideoElement&) const;

  // Meant to be called by HTMLVideoElementPictureInPicture and DOM objects
  // but not internally.
  void SetPictureInPictureElement(HTMLVideoElement&);

  // Meant to be called by DocumentPictureInPicture,
  // HTMLVideoElementPictureInPicture, and DOM objects but not internally.
  void UnsetPictureInPictureElement();

  // Returns element currently in Picture-in-Picture if any. Null otherwise.
  Element* PictureInPictureElement(TreeScope&) const;

  // Meant to be called by HTMLVideoElementPictureInPicture, and DOM objects but
  // not internally. It closes the current Picture-in-Picture window if any.
  PictureInPictureWindow* CreatePictureInPictureWindow(int width, int height);

  // Meant to be called by DocumentPictureInPicture,
  // HTMLVideoElementPictureInPicture, and DOM objects but not internally.
  void OnClosePictureInPictureWindow();

  void Trace(blink::Visitor*) override;

 private:
  explicit PictureInPictureController(Document&);

  // The Picture-in-Picture element for the associated document.
  Member<HTMLVideoElement> picture_in_picture_element_;

  // The Picture-in-Picture window for the associated document.
  Member<PictureInPictureWindow> picture_in_picture_window_;
};

}  // namespace blink

#endif  // PictureInPictureController_h
