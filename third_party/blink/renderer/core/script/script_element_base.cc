// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/script_element_base.h"

#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/svg/svg_script_element.h"

namespace blink {

ScriptElementBase* ScriptElementBase::FromElementIfPossible(Element* element) {
  if (auto* html_script = ToHTMLScriptElementOrNull(*element))
    return html_script;
  if (auto* svg_script = ToSVGScriptElementOrNull(*element))
    return svg_script;
  return nullptr;
}

ScriptLoader* ScriptElementBase::InitializeScriptLoader(bool parser_inserted,
                                                        bool already_started) {
  return ScriptLoader::Create(this, parser_inserted, already_started);
}

}  // namespace blink
