// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/xml/parser/XMLParserScriptRunner.h"

#include "core/dom/ClassicPendingScript.h"
#include "core/dom/Document.h"
#include "core/dom/ScriptLoader.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/xml/parser/XMLParserScriptRunnerHost.h"

namespace blink {

XMLParserScriptRunner::XMLParserScriptRunner(XMLParserScriptRunnerHost* host)
    : host_(host) {}

XMLParserScriptRunner::~XMLParserScriptRunner() {
  DCHECK(!parser_blocking_script_);
}

DEFINE_TRACE(XMLParserScriptRunner) {
  visitor->Trace(parser_blocking_script_);
  visitor->Trace(script_element_);
  visitor->Trace(host_);
  PendingScriptClient::Trace(visitor);
}

void XMLParserScriptRunner::Detach() {
  if (parser_blocking_script_) {
    parser_blocking_script_->StopWatchingForLoad();
    parser_blocking_script_ = nullptr;
  }
}

void XMLParserScriptRunner::PendingScriptFinished(
    PendingScript* unused_pending_script) {
  DCHECK_EQ(unused_pending_script, parser_blocking_script_);
  PendingScript* pending_script = parser_blocking_script_;
  parser_blocking_script_ = nullptr;

  pending_script->StopWatchingForLoad();

  ScriptLoader* script_loader = script_element_->Loader();
  script_element_ = nullptr;

  DCHECK(script_loader);
  CHECK_EQ(script_loader->GetScriptType(), ScriptType::kClassic);

  script_loader->ExecuteScriptBlock(pending_script, NullURL());

  script_element_ = nullptr;

  host_->NotifyScriptExecuted();
}

void XMLParserScriptRunner::ProcessScriptElement(
    Document& document,
    Element* element,
    TextPosition script_start_position) {
  DCHECK(element);
  DCHECK(!parser_blocking_script_);

  ScriptElementBase* script_element_base =
      ScriptElementBase::FromElementIfPossible(element);
  CHECK(script_element_base);

  ScriptLoader* script_loader = script_element_base->Loader();
  DCHECK(script_loader);

  bool success = script_loader->PrepareScript(
      script_start_position, ScriptLoader::kAllowLegacyTypeInTypeAttribute);

  if (script_loader->GetScriptType() != ScriptType::kClassic) {
    // XMLDocumentParser does not support a module script, and thus ignores it.
    success = false;
    document.AddConsoleMessage(
        ConsoleMessage::Create(kJSMessageSource, kErrorMessageLevel,
                               "Module scripts in XML documents are currently "
                               "not supported. See crbug.com/717643"));
  }

  if (!success)
    return;

  if (script_loader->ReadyToBeParserExecuted()) {
    // 5th Clause, Step 23 of https://html.spec.whatwg.org/#prepare-a-script
    script_loader->ExecuteScriptBlock(
        ClassicPendingScript::Create(script_element_base,
                                     script_start_position),
        document.Url());
  } else if (script_loader->WillBeParserExecuted()) {
    // 1st/2nd Clauses, Step 23 of
    // https://html.spec.whatwg.org/#prepare-a-script
    parser_blocking_script_ = script_loader->CreatePendingScript();
    parser_blocking_script_->MarkParserBlockingLoadStartTime();
    script_element_ = script_element_base;
    parser_blocking_script_->WatchForLoad(this);
  } else {
    script_element_ = nullptr;
  }
}

}  // namespace blink
