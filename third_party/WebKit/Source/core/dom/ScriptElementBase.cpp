// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ScriptElementBase.h"

#include "core/html/HTMLScriptElement.h"
#include "core/svg/SVGScriptElement.h"

namespace blink {

ScriptElementBase* ScriptElementBase::FromElementIfPossible(Element* element) {
  if (auto* html_script = ToHTMLScriptElementOrNull(*element))
    return html_script;
  if (auto* svg_script = ToSVGScriptElementOrNull(*element))
    return svg_script;
  return nullptr;
}

ScriptLoader* ScriptElementBase::InitializeScriptLoader(
    bool parser_inserted,
    bool already_started,
    bool created_during_document_write) {
  return ScriptLoader::Create(this, parser_inserted, already_started,
                              created_during_document_write);
}

}  // namespace blink
