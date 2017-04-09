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

void TransformDocumentToXMLTreeView(Document& document) {
  String script_string = LoadResourceAsASCIIString("DocumentXMLTreeViewer.js");
  String css_string = LoadResourceAsASCIIString("DocumentXMLTreeViewer.css");

  HeapVector<ScriptSourceCode> sources;
  sources.push_back(ScriptSourceCode(script_string));
  v8::HandleScope handle_scope(V8PerIsolateData::MainThreadIsolate());

  document.GetFrame()->Script().ExecuteScriptInIsolatedWorld(
      DOMWrapperWorld::kDocumentXMLTreeViewerWorldId, sources, nullptr);

  Element* element = document.GetElementById("xml-viewer-style");
  if (element) {
    element->setTextContent(css_string);
  }
}

}  // namespace blink
