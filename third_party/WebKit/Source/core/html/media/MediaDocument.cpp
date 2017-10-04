/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/media/MediaDocument.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/add_event_listener_options_or_boolean.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/RawDataDocumentParser.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/dom/events/Event.h"
#include "core/dom/events/EventListener.h"
#include "core/events/KeyboardEvent.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/html/HTMLMetaElement.h"
#include "core/html/HTMLSourceElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html_names.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "platform/KeyboardCodes.h"

namespace blink {

using namespace HTMLNames;

// FIXME: Share more code with PluginDocumentParser.
class MediaDocumentParser : public RawDataDocumentParser {
 public:
  static MediaDocumentParser* Create(MediaDocument* document) {
    return new MediaDocumentParser(document);
  }

 private:
  explicit MediaDocumentParser(Document* document)
      : RawDataDocumentParser(document), did_build_document_structure_(false) {}

  void AppendBytes(const char*, size_t) override;

  void CreateDocumentStructure();

  bool did_build_document_structure_;
};

class MediaLoadedEventListener final : public EventListener {
  WTF_MAKE_NONCOPYABLE(MediaLoadedEventListener);

 public:
  static MediaLoadedEventListener* Create() {
    return new MediaLoadedEventListener();
  }

  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

 private:
  MediaLoadedEventListener() : EventListener(kCPPEventListenerType) {}

  void handleEvent(ExecutionContext* context, Event* event) override {
    HTMLVideoElement* media =
        static_cast<HTMLVideoElement*>(event->target()->ToNode());
    std::unique_ptr<UserGestureIndicator> gesture =
        LocalFrame::CreateUserGesture(media->GetDocument().GetFrame());
    // TODO(shaktisahu): Enable fullscreen after https://crbug/698353 is fixed.
    media->Play();
  }
};

void MediaDocumentParser::CreateDocumentStructure() {
  DCHECK(GetDocument());
  HTMLHtmlElement* root_element = HTMLHtmlElement::Create(*GetDocument());
  GetDocument()->AppendChild(root_element);
  root_element->InsertedByParser();

  if (IsDetached())
    return;  // runScriptsAtDocumentElementAvailable can detach the frame.

  HTMLHeadElement* head = HTMLHeadElement::Create(*GetDocument());
  HTMLMetaElement* meta = HTMLMetaElement::Create(*GetDocument());
  meta->setAttribute(nameAttr, "viewport");
  meta->setAttribute(contentAttr, "width=device-width");
  head->AppendChild(meta);

  HTMLVideoElement* media = HTMLVideoElement::Create(*GetDocument());
  media->setAttribute(controlsAttr, "");
  media->setAttribute(autoplayAttr, "");
  media->setAttribute(nameAttr, "media");

  HTMLSourceElement* source = HTMLSourceElement::Create(*GetDocument());
  source->SetSrc(GetDocument()->Url());

  if (DocumentLoader* loader = GetDocument()->Loader())
    source->setType(loader->MimeType());

  media->AppendChild(source);

  HTMLBodyElement* body = HTMLBodyElement::Create(*GetDocument());

  GetDocument()->WillInsertBody();

  if (GetDocument()->GetSettings() &&
      GetDocument()->GetSettings()->GetEmbeddedMediaExperienceEnabled() &&
      source->type().StartsWithIgnoringASCIICase("video/")) {
    EventListener* listener = MediaLoadedEventListener::Create();
    AddEventListenerOptions options;
    options.setOnce(true);
    AddEventListenerOptionsOrBoolean options_or_boolean;
    options_or_boolean.SetAddEventListenerOptions(options);
    media->addEventListener(EventTypeNames::loadedmetadata, listener,
                            options_or_boolean);
  }

  body->AppendChild(media);
  root_element->AppendChild(head);
  if (IsDetached())
    return;  // DOM insertion events can detach the frame.
  root_element->AppendChild(body);

  did_build_document_structure_ = true;
}

void MediaDocumentParser::AppendBytes(const char*, size_t) {
  if (did_build_document_structure_)
    return;

  CreateDocumentStructure();
  Finish();
}

MediaDocument::MediaDocument(const DocumentInit& initializer)
    : HTMLDocument(initializer, kMediaDocumentClass) {
  SetCompatibilityMode(kQuirksMode);
  LockCompatibilityMode();
  UseCounter::Count(*this, WebFeature::kMediaDocument);
  if (!IsInMainFrame())
    UseCounter::Count(*this, WebFeature::kMediaDocumentInFrame);
}

DocumentParser* MediaDocument::CreateParser() {
  return MediaDocumentParser::Create(this);
}

void MediaDocument::DefaultEventHandler(Event* event) {
  Node* target_node = event->target()->ToNode();
  if (!target_node)
    return;

  if (event->type() == EventTypeNames::keydown && event->IsKeyboardEvent()) {
    HTMLVideoElement* video =
        Traversal<HTMLVideoElement>::FirstWithin(*target_node);
    if (!video)
      return;

    KeyboardEvent* keyboard_event = ToKeyboardEvent(event);
    if (keyboard_event->key() == " " ||
        keyboard_event->keyCode() == VKEY_MEDIA_PLAY_PAUSE) {
      // space or media key (play/pause)
      video->TogglePlayState();
      event->SetDefaultHandled();
    }
  }
}

}  // namespace blink
