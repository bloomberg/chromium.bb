// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_CONTROLLER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_CONTROLLER_IMPL_H_

#include "base/macros.h"
#include "third_party/blink/public/mojom/picture_in_picture/picture_in_picture.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document_shutdown_observer.h"
#include "third_party/blink/renderer/core/frame/picture_in_picture_controller.h"
#include "third_party/blink/renderer/core/page/page_visibility_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class HTMLVideoElement;
class PictureInPictureWindow;
class TreeScope;
struct WebSize;

// The PictureInPictureControllerImpl is keeping the state and implementing the
// logic around the Picture-in-Picture feature. It is meant to be used as well
// by the Picture-in-Picture Web API and internally (eg. media controls). All
// consumers inside Blink modules/ should use this class to access
// Picture-in-Picture. In core/, they should use PictureInPictureController.
// PictureInPictureControllerImpl instance is associated to a Document. It is
// supplement and therefore can be lazy-initiated. Callers should consider
// whether they want to instantiate an object when they make a call.
class MODULES_EXPORT PictureInPictureControllerImpl
    : public PictureInPictureController,
      public PageVisibilityObserver,
      public DocumentShutdownObserver,
      public blink::mojom::blink::PictureInPictureDelegate {
  USING_GARBAGE_COLLECTED_MIXIN(PictureInPictureControllerImpl);

 public:
  explicit PictureInPictureControllerImpl(Document&);
  ~PictureInPictureControllerImpl() override = default;

  // Meant to be called internally by PictureInPictureController::From()
  // through ModulesInitializer.
  static PictureInPictureControllerImpl* Create(Document&);

  // Gets, or creates, PictureInPictureControllerImpl supplement on Document.
  // Should be called before any other call to make sure a document is attached.
  static PictureInPictureControllerImpl& From(Document&);

  // Returns whether system allows Picture-in-Picture feature or not for
  // the associated document.
  bool PictureInPictureEnabled() const;

  // Returns whether the document associated with the controller is allowed to
  // request Picture-in-Picture.
  Status IsDocumentAllowed() const;

  // Returns element currently in Picture-in-Picture if any. Null otherwise.
  Element* PictureInPictureElement() const;
  Element* PictureInPictureElement(TreeScope&) const;

  // Returns video element whose autoPictureInPicture attribute was set most
  // recently.
  HTMLVideoElement* AutoPictureInPictureElement() const;

  // Returns whether Auto Picture-in-Picture is allowed. It returns true if
  // it's a Chrome extension or if is's a PWA window in its web app scope.
  // Otherwise it returns false.
  bool IsAutoPictureInPictureAllowed() const;

  // Implementation of PictureInPictureController.
  void EnterPictureInPicture(HTMLVideoElement*,
                             ScriptPromiseResolver*) override;
  void ExitPictureInPicture(HTMLVideoElement*, ScriptPromiseResolver*) override;
  void AddToAutoPictureInPictureElementsList(HTMLVideoElement*) override;
  void RemoveFromAutoPictureInPictureElementsList(HTMLVideoElement*) override;
  Status IsElementAllowed(const HTMLVideoElement&) const override;
  bool IsPictureInPictureElement(const Element*) const override;
  void OnPictureInPictureStateChange() override;

  // Implementation of PictureInPictureDelegate.
  void PictureInPictureWindowSizeChanged(const blink::WebSize&) override;

  // Implementation of PageVisibilityObserver.
  void PageVisibilityChanged() override;

  // Implementation of DocumentShutdownObserver.
  void ContextDestroyed(Document*) override;

  void Trace(blink::Visitor*) override;

  mojo::Binding<mojom::blink::PictureInPictureDelegate>&
  GetDelegateBindingForTesting() {
    return delegate_binding_;
  }

 private:
  void OnEnteredPictureInPicture(HTMLVideoElement*,
                                 ScriptPromiseResolver*,
                                 const WebSize& picture_in_picture_window_size);
  void OnExitedPictureInPicture(ScriptPromiseResolver*) override;

  // Makes sure the `picture_in_picture_service_` is set. Returns whether it was
  // initialized successfully.
  bool EnsureService();

  // Returns true if video has an audio track and if MuteButton origin trial is
  // enabled. Otherwise it returns false.
  bool ShouldShowMuteButton(const HTMLVideoElement& element);

  // The Picture-in-Picture element for the associated document.
  Member<HTMLVideoElement> picture_in_picture_element_;

  // The list of video elements for the associated document that are eligible
  // to Auto Picture-in-Picture.
  HeapDeque<Member<HTMLVideoElement>> auto_picture_in_picture_elements_;

  // The Picture-in-Picture window for the associated document.
  Member<PictureInPictureWindow> picture_in_picture_window_;

  // Mojo bindings for the delegate interface implemented by |this|.
  mojo::Binding<mojom::blink::PictureInPictureDelegate> delegate_binding_;

  // Picture-in-Picture service living in the browser process.
  mojom::blink::PictureInPictureServicePtr picture_in_picture_service_;

  DISALLOW_COPY_AND_ASSIGN(PictureInPictureControllerImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_CONTROLLER_IMPL_H_
