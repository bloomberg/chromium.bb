/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/PluginDocument.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/RawDataDocumentParser.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLEmbedElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/html_names.h"
#include "core/layout/LayoutEmbeddedObject.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/plugins/PluginView.h"

namespace blink {

using namespace HTMLNames;

// FIXME: Share more code with MediaDocumentParser.
class PluginDocumentParser : public RawDataDocumentParser {
 public:
  static PluginDocumentParser* Create(PluginDocument* document) {
    return new PluginDocumentParser(document);
  }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(embed_element_);
    RawDataDocumentParser::Trace(visitor);
  }

 private:
  PluginDocumentParser(Document* document)
      : RawDataDocumentParser(document), embed_element_(nullptr) {}

  void AppendBytes(const char*, size_t) override;

  void Finish() override;

  void CreateDocumentStructure();

  PluginView* GetPluginView() const;

  Member<HTMLEmbedElement> embed_element_;
};

void PluginDocumentParser::CreateDocumentStructure() {
  // FIXME: Assert we have a loader to figure out why the original null checks
  // and assert were added for the security bug in
  // http://trac.webkit.org/changeset/87566
  DCHECK(GetDocument());
  CHECK(GetDocument()->Loader());

  LocalFrame* frame = GetDocument()->GetFrame();
  if (!frame)
    return;

  // FIXME: Why does this check settings?
  if (!frame->GetSettings() ||
      !frame->Loader().AllowPlugins(kNotAboutToInstantiatePlugin))
    return;

  HTMLHtmlElement* root_element = HTMLHtmlElement::Create(*GetDocument());
  GetDocument()->AppendChild(root_element);
  root_element->InsertedByParser();
  if (IsStopped())
    return;  // runScriptsAtDocumentElementAvailable can detach the frame.

  HTMLBodyElement* body = HTMLBodyElement::Create(*GetDocument());
  body->setAttribute(styleAttr,
                     "background-color: rgb(38,38,38); height: 100%; width: "
                     "100%; overflow: hidden; margin: 0");
  root_element->AppendChild(body);
  if (IsStopped()) {
    // Possibly detached by a mutation event listener installed in
    // runScriptsAtDocumentElementAvailable.
    return;
  }

  embed_element_ = HTMLEmbedElement::Create(*GetDocument());
  embed_element_->setAttribute(widthAttr, "100%");
  embed_element_->setAttribute(heightAttr, "100%");
  embed_element_->setAttribute(nameAttr, "plugin");
  embed_element_->setAttribute(idAttr, "plugin");
  embed_element_->setAttribute(srcAttr,
                               AtomicString(GetDocument()->Url().GetString()));
  embed_element_->setAttribute(typeAttr, GetDocument()->Loader()->MimeType());
  body->AppendChild(embed_element_);
  if (IsStopped()) {
    // Possibly detached by a mutation event listener installed in
    // runScriptsAtDocumentElementAvailable.
    return;
  }

  ToPluginDocument(GetDocument())->SetPluginNode(embed_element_);

  GetDocument()->UpdateStyleAndLayout();

  // We need the plugin to load synchronously so we can get the PluginView
  // below so flush the layout tasks now instead of waiting on the timer.
  frame->View()->FlushAnyPendingPostLayoutTasks();
  // Focus the plugin here, as the line above is where the plugin is created.
  if (frame->IsMainFrame()) {
    embed_element_->focus();
    if (IsStopped()) {
      // Possibly detached by a mutation event listener installed in
      // runScriptsAtDocumentElementAvailable.
      return;
    }
  }

  if (PluginView* view = GetPluginView())
    view->DidReceiveResponse(GetDocument()->Loader()->GetResponse());
}

void PluginDocumentParser::AppendBytes(const char* data, size_t length) {
  if (!embed_element_) {
    CreateDocumentStructure();
    if (IsStopped())
      return;
  }

  if (!length)
    return;
  if (PluginView* view = GetPluginView())
    view->DidReceiveData(data, length);
}

void PluginDocumentParser::Finish() {
  embed_element_ = nullptr;
  RawDataDocumentParser::Finish();
}

PluginView* PluginDocumentParser::GetPluginView() const {
  return ToPluginDocument(GetDocument())->GetPluginView();
}

PluginDocument::PluginDocument(const DocumentInit& initializer)
    : HTMLDocument(initializer, kPluginDocumentClass) {
  SetCompatibilityMode(kQuirksMode);
  LockCompatibilityMode();
  UseCounter::Count(*this, WebFeature::kPluginDocument);
  if (!IsInMainFrame())
    UseCounter::Count(*this, WebFeature::kPluginDocumentInFrame);
}

DocumentParser* PluginDocument::CreateParser() {
  return PluginDocumentParser::Create(this);
}

PluginView* PluginDocument::GetPluginView() {
  return plugin_node_ ? plugin_node_->OwnedPlugin() : nullptr;
}

void PluginDocument::Shutdown() {
  // Release the plugin node so that we don't have a circular reference.
  plugin_node_ = nullptr;
  HTMLDocument::Shutdown();
}

void PluginDocument::Trace(blink::Visitor* visitor) {
  visitor->Trace(plugin_node_);
  HTMLDocument::Trace(visitor);
}

}  // namespace blink
