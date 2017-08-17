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

#include "bindings/core/v8/AddEventListenerOptionsOrBoolean.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/RawDataDocumentParser.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/events/Event.h"
#include "core/events/EventListener.h"
#include "core/events/KeyboardEvent.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLContentElement.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/html/HTMLMetaElement.h"
#include "core/html/HTMLSourceElement.h"
#include "core/html/HTMLStyleElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "platform/Histogram.h"
#include "platform/KeyboardCodes.h"
#include "platform/text/PlatformLocale.h"

namespace blink {

using namespace HTMLNames;

// Enums used for UMA histogram.
enum MediaDocumentDownloadButtonValue {
  kMediaDocumentDownloadButtonShown,
  kMediaDocumentDownloadButtonClicked,
  // Only append new enums here.
  kMediaDocumentDownloadButtonMax
};

void RecordDownloadMetric(MediaDocumentDownloadButtonValue value) {
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, media_document_download_button_histogram,
      ("Blink.MediaDocument.DownloadButton", kMediaDocumentDownloadButtonMax));
  media_document_download_button_histogram.Count(value);
}

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

class MediaDownloadEventListener final : public EventListener {
 public:
  static MediaDownloadEventListener* Create() {
    return new MediaDownloadEventListener();
  }

  bool operator==(const EventListener& other) const override {
    return this == &other;
  }

 private:
  MediaDownloadEventListener()
      : EventListener(kCPPEventListenerType), clicked_(false) {}

  void handleEvent(ExecutionContext* context, Event* event) override {
    if (!clicked_) {
      RecordDownloadMetric(kMediaDocumentDownloadButtonClicked);
      clicked_ = true;
    }
  }

  bool clicked_;
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
  body->setAttribute(styleAttr, "margin: 0px;");

  GetDocument()->WillInsertBody();

  HTMLDivElement* div = HTMLDivElement::Create(*GetDocument());
  // Style sheets for media controls are lazily loaded until a media element is
  // encountered.  As a result, elements encountered before the media element
  // will not get the right style at first if we put the styles in
  // mediacontrols.css. To solve this issue, set the styles inline so that they
  // will be applied when the page loads.  See w3c example on how to centering
  // an element: https://www.w3.org/Style/Examples/007/center.en.html
  div->setAttribute(styleAttr,
                    "display: flex;"
                    "flex-direction: column;"
                    "justify-content: center;"
                    "align-items: center;"
                    "min-height: min-content;"
                    "height: 100%;");
  HTMLContentElement* content = HTMLContentElement::Create(*GetDocument());
  div->AppendChild(content);

  if (GetDocument()->GetSettings() &&
      GetDocument()->GetSettings()->GetEmbeddedMediaExperienceEnabled() &&
      source->type().StartsWithIgnoringASCIICase("video/")) {
    EventListener* listener = MediaLoadedEventListener::Create();
    AddEventListenerOptions options;
    options.setOnce(true);
    AddEventListenerOptionsOrBoolean options_or_boolean;
    options_or_boolean.setAddEventListenerOptions(options);
    media->addEventListener(EventTypeNames::loadedmetadata, listener,
                            options_or_boolean);
  }

  if (RuntimeEnabledFeatures::MediaDocumentDownloadButtonEnabled()) {
    HTMLAnchorElement* anchor = HTMLAnchorElement::Create(*GetDocument());
    anchor->setAttribute(downloadAttr, "");
    anchor->SetURL(GetDocument()->Url());
    anchor->setTextContent(
        GetDocument()
            ->GetCachedLocale(GetDocument()->ContentLanguage())
            .QueryString(WebLocalizedString::kDownloadButtonLabel)
            .UpperUnicode(GetDocument()->ContentLanguage()));
    // Using CSS style according to Android material design.
    anchor->setAttribute(
        styleAttr,
        "display: inline-block;"
        "margin-top: 32px;"
        "padding: 0 16px 0 16px;"
        "height: 36px;"
        "background: #000000;"
        "-webkit-tap-highlight-color: rgba(255, 255, 255, 0.12);"
        "font-family: Roboto;"
        "font-size: 14px;"
        "border-radius: 5px;"
        "color: white;"
        "font-weight: 500;"
        "text-decoration: none;"
        "line-height: 36px;");
    EventListener* listener = MediaDownloadEventListener::Create();
    anchor->addEventListener(EventTypeNames::click, listener, false);
    HTMLDivElement* button_container = HTMLDivElement::Create(*GetDocument());
    button_container->setAttribute(styleAttr,
                                   "text-align: center;"
                                   "height: 0;"
                                   "flex: none");
    button_container->AppendChild(anchor);
    div->AppendChild(button_container);
    RecordDownloadMetric(kMediaDocumentDownloadButtonShown);
  }

  // According to
  // https://html.spec.whatwg.org/multipage/browsers.html#read-media,
  // MediaDocument should have a single child which is the video element. Use
  // shadow root to hide all the elements we added here.
  ShadowRoot& shadow_root = body->EnsureUserAgentShadowRoot();
  shadow_root.AppendChild(div);
  body->AppendChild(media);
  root_element->AppendChild(head);
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
