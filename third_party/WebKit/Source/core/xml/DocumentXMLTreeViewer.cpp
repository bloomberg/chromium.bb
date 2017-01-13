// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/xml/DocumentXMLTreeViewer.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/LocalFrame.h"
#include "platform/PlatformResourceLoader.h"

namespace blink {

void transformDocumentToXMLTreeView(Document& document) {
  String scriptString = loadResourceAsASCIIString("DocumentXMLTreeViewer.js");
  String cssString = loadResourceAsASCIIString("DocumentXMLTreeViewer.css");

  HeapVector<ScriptSourceCode> sources;
  sources.push_back(ScriptSourceCode(scriptString));
  v8::HandleScope handleScope(V8PerIsolateData::mainThreadIsolate());

  document.frame()->script().executeScriptInIsolatedWorld(
      WorldIdConstants::DocumentXMLTreeViewerWorldId, sources, nullptr);

  Element* element = document.getElementById("xml-viewer-style");
  if (element) {
    element->setTextContent(cssString);
  }
}

}  // namespace blink
