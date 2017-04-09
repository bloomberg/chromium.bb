// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ScriptElementBase.h"

#include "core/html/HTMLScriptElement.h"
#include "core/svg/SVGScriptElement.h"

namespace blink {

ScriptElementBase* ScriptElementBase::FromElementIfPossible(Element* element) {
  if (isHTMLScriptElement(*element))
    return toHTMLScriptElement(element);
  if (isSVGScriptElement(*element))
    return toSVGScriptElement(element);
  return nullptr;
}

void ScriptElementBase::InitializeScriptLoader(
    bool parser_inserted,
    bool already_started,
    bool created_during_document_write) {
  loader_ = ScriptLoader::Create(this, parser_inserted, already_started,
                                 created_during_document_write);
}

DEFINE_TRACE(ScriptElementBase) {
  visitor->Trace(loader_);
}

}  // namespace blink
