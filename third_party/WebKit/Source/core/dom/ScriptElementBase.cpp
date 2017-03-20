// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ScriptElementBase.h"

#include "core/html/HTMLScriptElement.h"
#include "core/svg/SVGScriptElement.h"

namespace blink {

ScriptElementBase* ScriptElementBase::fromElementIfPossible(Element* element) {
  if (isHTMLScriptElement(*element))
    return toHTMLScriptElement(element);
  if (isSVGScriptElement(*element))
    return toSVGScriptElement(element);
  return nullptr;
}

void ScriptElementBase::initializeScriptLoader(
    bool parserInserted,
    bool alreadyStarted,
    bool createdDuringDocumentWrite) {
  m_loader = ScriptLoader::create(this, parserInserted, alreadyStarted,
                                  createdDuringDocumentWrite);
}

DEFINE_TRACE(ScriptElementBase) {
  visitor->trace(m_loader);
}

}  // namespace blink
