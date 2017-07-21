/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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

#include "core/html/parser/HTMLParserScriptRunner.h"

#include <inttypes.h>
#include <memory>
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/ClassicPendingScript.h"
#include "core/dom/ClassicScript.h"
#include "core/dom/DocumentParserTiming.h"
#include "core/dom/Element.h"
#include "core/dom/IgnoreDestructiveWriteCountIncrementer.h"
#include "core/dom/ScriptLoader.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/events/Event.h"
#include "core/frame/LocalFrame.h"
#include "core/html/parser/HTMLInputStream.h"
#include "core/html/parser/HTMLParserScriptRunnerHost.h"
#include "core/html/parser/NestingLevelIncrementer.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/resource/ScriptResource.h"
#include "platform/Histogram.h"
#include "platform/WebFrameScheduler.h"
#include "platform/bindings/Microtask.h"
#include "platform/bindings/V8PerIsolateData.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/instrumentation/tracing/TracedValue.h"
#include "public/platform/Platform.h"

namespace blink {

namespace {

// TODO(bmcquade): move this to a shared location if we find ourselves wanting
// to trace similar data elsewhere in the codebase.
std::unique_ptr<TracedValue> GetTraceArgsForScriptElement(
    ScriptElementBase* element,
    const TextPosition& text_position) {
  std::unique_ptr<TracedValue> value = TracedValue::Create();
  ScriptLoader* script_loader = element->Loader();
  if (script_loader && script_loader->GetResource())
    value->SetString("url", script_loader->GetResource()->Url().GetString());
  if (element->GetDocument().GetFrame()) {
    value->SetString(
        "frame",
        String::Format("0x%" PRIx64,
                       static_cast<uint64_t>(reinterpret_cast<intptr_t>(
                           element->GetDocument().GetFrame()))));
  }
  if (text_position.line_.ZeroBasedInt() > 0 ||
      text_position.column_.ZeroBasedInt() > 0) {
    value->SetInteger("lineNumber", text_position.line_.OneBasedInt());
    value->SetInteger("columnNumber", text_position.column_.OneBasedInt());
  }
  return value;
}

void DoExecuteScript(PendingScript* pending_script, const KURL& document_url) {
  ScriptElementBase* element = pending_script->GetElement();
  ScriptLoader* script_loader = element->Loader();
  DCHECK(script_loader);
  const char* const trace_event_name =
      pending_script->ErrorOccurred()
          ? "HTMLParserScriptRunner ExecuteScriptFailed"
          : "HTMLParserScriptRunner ExecuteScript";
  TRACE_EVENT_WITH_FLOW1("blink", trace_event_name, element,
                         TRACE_EVENT_FLAG_FLOW_IN, "data",
                         GetTraceArgsForScriptElement(
                             element, pending_script->StartingPosition()));
  script_loader->ExecuteScriptBlock(pending_script, document_url);
}

void TraceParserBlockingScript(const PendingScript* pending_script,
                               bool waiting_for_resources) {
  // The HTML parser must yield before executing script in the following
  // cases:
  // * the script's execution is blocked on the completed load of the script
  //   resource
  //   (https://html.spec.whatwg.org/multipage/scripting.html#pending-parsing-blocking-script)
  // * the script's execution is blocked on the load of a style sheet or other
  //   resources that are blocking scripts
  //   (https://html.spec.whatwg.org/multipage/semantics.html#a-style-sheet-that-is-blocking-scripts)
  //
  // Both of these cases can introduce significant latency when loading a
  // web page, especially for users on slow connections, since the HTML parser
  // must yield until the blocking resources finish loading.
  //
  // We trace these parser yields here using flow events, so we can track
  // both when these yields occur, as well as how long the parser had
  // to yield. The connecting flow events are traced once the parser becomes
  // unblocked when the script actually executes, in doExecuteScript.
  ScriptElementBase* element = pending_script->GetElement();
  if (!element)
    return;
  TextPosition script_start_position = pending_script->StartingPosition();
  if (!pending_script->IsReady()) {
    if (waiting_for_resources) {
      TRACE_EVENT_WITH_FLOW1(
          "blink", "YieldParserForScriptLoadAndBlockingResources", element,
          TRACE_EVENT_FLAG_FLOW_OUT, "data",
          GetTraceArgsForScriptElement(element, script_start_position));
    } else {
      TRACE_EVENT_WITH_FLOW1(
          "blink", "YieldParserForScriptLoad", element,
          TRACE_EVENT_FLAG_FLOW_OUT, "data",
          GetTraceArgsForScriptElement(element, script_start_position));
    }
  } else if (waiting_for_resources) {
    TRACE_EVENT_WITH_FLOW1(
        "blink", "YieldParserForScriptBlockingResources", element,
        TRACE_EVENT_FLAG_FLOW_OUT, "data",
        GetTraceArgsForScriptElement(element, script_start_position));
  }
}

static KURL DocumentURLForScriptExecution(Document* document) {
  if (!document)
    return KURL();

  if (!document->GetFrame()) {
    if (document->ImportsController())
      return document->Url();
    return KURL();
  }

  // Use the URL of the currently active document for this frame.
  return document->GetFrame()->GetDocument()->Url();
}

}  // namespace

using namespace HTMLNames;

HTMLParserScriptRunner::HTMLParserScriptRunner(
    HTMLParserReentryPermit* reentry_permit,
    Document* document,
    HTMLParserScriptRunnerHost* host)
    : reentry_permit_(reentry_permit),
      document_(document),
      host_(host),
      parser_blocking_script_(this, nullptr) {
  DCHECK(host_);
}

HTMLParserScriptRunner::~HTMLParserScriptRunner() {}

void HTMLParserScriptRunner::Detach() {
  if (!document_)
    return;

  if (parser_blocking_script_)
    parser_blocking_script_->Dispose();
  parser_blocking_script_ = nullptr;

  while (!scripts_to_execute_after_parsing_.IsEmpty()) {
    PendingScript* pending_script =
        scripts_to_execute_after_parsing_.TakeFirst();
    pending_script->Dispose();
  }
  document_ = nullptr;
  // m_reentryPermit is not cleared here, because the script runner
  // may continue to run pending scripts after the parser has
  // detached.
}

bool HTMLParserScriptRunner::IsParserBlockingScriptReady() {
  DCHECK(ParserBlockingScript());
  if (!document_->IsScriptExecutionReady())
    return false;
  return ParserBlockingScript()->IsReady();
}

// This has two callers and corresponds to different concepts in the spec:
// - When called from executeParsingBlockingScripts(), this corresponds to some
//   steps of the "Otherwise" Clause of 'An end tag whose tag name is "script"'
//   https://html.spec.whatwg.org/#scriptEndTag
// - When called from executeScriptsWaitingForParsing(), this corresponds
//   https://html.spec.whatwg.org/#execute-the-script-block
//   and thus currently this function does more than specced.
// TODO(hiroshige): Make the spec and implementation consistent.
void HTMLParserScriptRunner::ExecutePendingScriptAndDispatchEvent(
    PendingScript* pending_script,
    ScriptStreamer::Type pending_script_type) {
  // Stop watching loads before executeScript to prevent recursion if the script
  // reloads itself.
  // TODO(kouhei): Consider merging this w/ pendingScript->dispose() after the
  // if block.
  pending_script->StopWatchingForLoad();

  if (!IsExecutingScript()) {
    Microtask::PerformCheckpoint(V8PerIsolateData::MainThreadIsolate());
    if (pending_script_type == ScriptStreamer::kParsingBlocking) {
      // The parser cannot be unblocked as a microtask requested another
      // resource
      if (!document_->IsScriptExecutionReady())
        return;
    }
  }

  double script_parser_blocking_time =
      pending_script->ParserBlockingLoadStartTime();
  ScriptElementBase* element = pending_script->GetElement();

  // 1. "Let the script be the pending parsing-blocking script.
  //     There is no longer a pending parsing-blocking script."
  if (pending_script_type == ScriptStreamer::kParsingBlocking) {
    parser_blocking_script_ = nullptr;
  }

  if (ScriptLoader* script_loader = element->Loader()) {
    // 7. "Increment the parser's script nesting level by one (it should be
    //     zero before this step, so this sets it to one)."
    HTMLParserReentryPermit::ScriptNestingLevelIncrementer
        nesting_level_incrementer =
            reentry_permit_->IncrementScriptNestingLevel();

    IgnoreDestructiveWriteCountIncrementer
        ignore_destructive_write_count_incrementer(document_);

    // 8. "Execute the script."
    DCHECK(IsExecutingScript());
    if (!pending_script->ErrorOccurred() && script_parser_blocking_time > 0.0) {
      DocumentParserTiming::From(*document_)
          .RecordParserBlockedOnScriptLoadDuration(
              MonotonicallyIncreasingTime() - script_parser_blocking_time,
              script_loader->WasCreatedDuringDocumentWrite());
    }
    DoExecuteScript(pending_script, DocumentURLForScriptExecution(document_));

    // 9. "Decrement the parser's script nesting level by one.
    //     If the parser's script nesting level is zero
    //     (which it always should be at this point),
    //     then set the parser pause flag to false."
    // This is implemented by ~ScriptNestingLevelIncrementer().
  }

  DCHECK(!IsExecutingScript());
}

void FetchBlockedDocWriteScript(ScriptElementBase* element,
                                bool is_parser_inserted,
                                const TextPosition& script_start_position) {
  DCHECK(element);

  ScriptLoader* script_loader =
      ScriptLoader::Create(element, is_parser_inserted, false, false);
  DCHECK(script_loader);
  script_loader->SetFetchDocWrittenScriptDeferIdle();
  script_loader->PrepareScript(script_start_position);
  CHECK_EQ(script_loader->GetScriptType(), ScriptType::kClassic);
}

void EmitWarningForDocWriteScripts(const String& url, Document& document) {
  String message =
      "The Parser-blocking, cross site (i.e. different eTLD+1) "
      "script, " +
      url +
      ", invoked via document.write was NOT BLOCKED on this page load, but MAY "
      "be blocked by the browser in future page loads with poor network "
      "connectivity.";
  document.AddConsoleMessage(
      ConsoleMessage::Create(kJSMessageSource, kWarningMessageLevel, message));
}

void EmitErrorForDocWriteScripts(const String& url, Document& document) {
  String message =
      "Network request for the parser-blocking, cross site "
      "(i.e. different eTLD+1) script, " +
      url +
      ", invoked via document.write was BLOCKED by the browser due to poor "
      "network connectivity. ";
  document.AddConsoleMessage(
      ConsoleMessage::Create(kJSMessageSource, kErrorMessageLevel, message));
}

void HTMLParserScriptRunner::PossiblyFetchBlockedDocWriteScript(
    PendingScript* pending_script) {
  // If the script was blocked as part of document.write intervention,
  // then send an asynchronous GET request with an interventions header.

  if (!ParserBlockingScript())
    return;

  if (ParserBlockingScript() != pending_script)
    return;

  ScriptElementBase* element = ParserBlockingScript()->GetElement();

  ScriptLoader* script_loader = element->Loader();
  if (!script_loader || !script_loader->DisallowedFetchForDocWrittenScript())
    return;

  // We don't allow document.write() and its intervention with module scripts.
  CHECK_EQ(pending_script->GetScriptType(), ScriptType::kClassic);

  if (!pending_script->ErrorOccurred()) {
    EmitWarningForDocWriteScripts(
        pending_script->UrlForClassicScript().GetString(), *document_);
    return;
  }

  // Due to dependency violation, not able to check the exact error to be
  // ERR_CACHE_MISS but other errors are rare with
  // WebCachePolicy::ReturnCacheDataDontLoad.

  EmitErrorForDocWriteScripts(pending_script->UrlForClassicScript().GetString(),
                              *document_);
  TextPosition starting_position = ParserBlockingScript()->StartingPosition();
  bool is_parser_inserted = script_loader->IsParserInserted();
  // Remove this resource entry from memory cache as the new request
  // should not join onto this existing entry.
  pending_script->RemoveFromMemoryCache();
  FetchBlockedDocWriteScript(element, is_parser_inserted, starting_position);
}

void HTMLParserScriptRunner::PendingScriptFinished(
    PendingScript* pending_script) {
  // Handle cancellations of parser-blocking script loads without
  // notifying the host (i.e., parser) if these were initiated by nested
  // document.write()s. The cancellation may have been triggered by
  // script execution to signal an abrupt stop (e.g., window.close().)
  //
  // The parser is unprepared to be told, and doesn't need to be.
  if (IsExecutingScript() && pending_script->WasCanceled()) {
    pending_script->Dispose();

    if (pending_script == ParserBlockingScript()) {
      parser_blocking_script_ = nullptr;
    } else {
      CHECK_EQ(pending_script, scripts_to_execute_after_parsing_.front());

      // TODO(hiroshige): Remove this CHECK() before going to beta.
      // This is only to make clusterfuzz to find a test case that executes
      // this code path.
      CHECK(false);

      scripts_to_execute_after_parsing_.pop_front();
      // TODO(hiroshige): executeScriptsWaitingForParsing() should be
      // called later at the appropriate time. https://crbug.com/696775
    }

    return;
  }

  // If the script was blocked as part of document.write intervention,
  // then send an asynchronous GET request with an interventions header.
  PossiblyFetchBlockedDocWriteScript(pending_script);

  host_->NotifyScriptLoaded(pending_script);
}

// 'An end tag whose tag name is "script"'
// https://html.spec.whatwg.org/#scriptEndTag
//
// Script handling lives outside the tree builder to keep each class simple.
void HTMLParserScriptRunner::ProcessScriptElement(
    Element* script_element,
    const TextPosition& script_start_position) {
  DCHECK(script_element);

  // FIXME: If scripting is disabled, always just return.

  bool had_preload_scanner = host_->HasPreloadScanner();

  // Initial steps of 'An end tag whose tag name is "script"'.
  // Try to execute the script given to us.
  ProcessScriptElementInternal(script_element, script_start_position);

  // "At this stage, if there is a pending parsing-blocking script, then:"
  if (HasParserBlockingScript()) {
    // - "If the script nesting level is not zero:"
    if (IsExecutingScript()) {
      // "Set the parser pause flag to true, and abort the processing of any
      //  nested invocations of the tokenizer, yielding control back to the
      //  caller. (Tokenization will resume when the caller returns to the
      //  "outer" tree construction stage.)"
      // TODO(hiroshige): set the parser pause flag to true here.

      // Unwind to the outermost HTMLParserScriptRunner::processScriptElement
      // before continuing parsing.
      return;
    }

    // - "Otherwise":

    TraceParserBlockingScript(ParserBlockingScript(),
                              !document_->IsScriptExecutionReady());
    parser_blocking_script_->MarkParserBlockingLoadStartTime();

    // If preload scanner got created, it is missing the source after the
    // current insertion point. Append it and scan.
    if (!had_preload_scanner && host_->HasPreloadScanner())
      host_->AppendCurrentInputStreamToPreloadScannerAndScan();

    ExecuteParsingBlockingScripts();
  }
}

bool HTMLParserScriptRunner::HasParserBlockingScript() const {
  return ParserBlockingScript();
}

// The "Otherwise" Clause of 'An end tag whose tag name is "script"'
// https://html.spec.whatwg.org/#scriptEndTag
void HTMLParserScriptRunner::ExecuteParsingBlockingScripts() {
  // 3. "If (1) the parser's Document has a style sheet that is blocking scripts
  //     or (2) the script's "ready to be parser-executed" flag is not set:
  //     spin the event loop
  //     until the parser's Document has no style sheet that is blocking scripts
  //     and the script's "ready to be parser-executed" flag is set."
  //
  // These conditions correspond to isParserBlockingScriptReady() and
  // if it is false, executeParsingBlockingScripts() will be called later
  // when isParserBlockingScriptReady() becomes true:
  // (1) from HTMLParserScriptRunner::executeScriptsWaitingForResources(), or
  // (2) from HTMLParserScriptRunner::executeScriptsWaitingForLoad().
  while (HasParserBlockingScript() && IsParserBlockingScriptReady()) {
    DCHECK(document_);
    DCHECK(!IsExecutingScript());
    DCHECK(document_->IsScriptExecutionReady());

    // 6. "Let the insertion point be just before the next input character."
    InsertionPointRecord insertion_point_record(host_->InputStream());

    // 1., 7.--9.
    ExecutePendingScriptAndDispatchEvent(parser_blocking_script_,
                                         ScriptStreamer::kParsingBlocking);

    // 10. "Let the insertion point be undefined again."
    // Implemented as ~InsertionPointRecord().

    // 11. "If there is once again a pending parsing-blocking script, then
    //      repeat these steps from step 1."
  }
}

void HTMLParserScriptRunner::ExecuteScriptsWaitingForLoad(
    PendingScript* pending_script) {
  TRACE_EVENT0("blink", "HTMLParserScriptRunner::executeScriptsWaitingForLoad");
  DCHECK(!IsExecutingScript());
  DCHECK(HasParserBlockingScript());
  DCHECK_EQ(pending_script, ParserBlockingScript());
  DCHECK(ParserBlockingScript()->IsReady());
  ExecuteParsingBlockingScripts();
}

void HTMLParserScriptRunner::ExecuteScriptsWaitingForResources() {
  TRACE_EVENT0("blink",
               "HTMLParserScriptRunner::executeScriptsWaitingForResources");
  DCHECK(document_);
  DCHECK(!IsExecutingScript());
  DCHECK(document_->IsScriptExecutionReady());
  ExecuteParsingBlockingScripts();
}

// Step 3 of https://html.spec.whatwg.org/#the-end:
// "If the list of scripts that will execute when the document has
//  finished parsing is not empty, run these substeps:"
bool HTMLParserScriptRunner::ExecuteScriptsWaitingForParsing() {
  TRACE_EVENT0("blink",
               "HTMLParserScriptRunner::executeScriptsWaitingForParsing");

  while (!scripts_to_execute_after_parsing_.IsEmpty()) {
    DCHECK(!IsExecutingScript());
    DCHECK(!HasParserBlockingScript());
    DCHECK(scripts_to_execute_after_parsing_.front()->IsExternalOrModule());

    // 1. "Spin the event loop until the first script in the list of scripts
    //     that will execute when the document has finished parsing
    //     has its "ready to be parser-executed" flag set and
    //     the parser's Document has no style sheet that is blocking scripts."
    // TODO(hiroshige): Is the latter part checked anywhere?
    if (!scripts_to_execute_after_parsing_.front()->IsReady()) {
      scripts_to_execute_after_parsing_.front()->WatchForLoad(this);
      TraceParserBlockingScript(scripts_to_execute_after_parsing_.front().Get(),
                                !document_->IsScriptExecutionReady());
      scripts_to_execute_after_parsing_.front()
          ->MarkParserBlockingLoadStartTime();
      return false;
    }

    // 3. "Remove the first script element from the list of scripts that will
    //     execute when the document has finished parsing (i.e. shift out the
    //     first entry in the list)."
    PendingScript* first = scripts_to_execute_after_parsing_.TakeFirst();

    // 2. "Execute the first script in the list of scripts that will execute
    //     when the document has finished parsing."
    ExecutePendingScriptAndDispatchEvent(first, ScriptStreamer::kDeferred);

    // FIXME: What is this m_document check for?
    if (!document_)
      return false;

    // 4. "If the list of scripts that will execute when the document has
    //     finished parsing is still not empty, repeat these substeps again
    //     from substep 1."
  }
  return true;
}

// 2nd Clause, Step 23 of https://html.spec.whatwg.org/#prepare-a-script
void HTMLParserScriptRunner::RequestParsingBlockingScript(Element* element) {
  // "The element is the pending parsing-blocking script of the Document of
  //  the parser that created the element.
  //  (There can only be one such script per Document at a time.)"
  CHECK(!ParserBlockingScript());
  parser_blocking_script_ = RequestPendingScript(element);
  if (!ParserBlockingScript())
    return;

  DCHECK(ParserBlockingScript()->IsExternal());

  // We only care about a load callback if resource is not already in the cache.
  // Callers will attempt to run the m_parserBlockingScript if possible before
  // returning control to the parser.
  if (!ParserBlockingScript()->IsReady()) {
    parser_blocking_script_->StartStreamingIfPossible(
        ScriptStreamer::kParsingBlocking, nullptr);
    parser_blocking_script_->WatchForLoad(this);
  }
}

// 1st Clause, Step 23 of https://html.spec.whatwg.org/#prepare-a-script
void HTMLParserScriptRunner::RequestDeferredScript(Element* element) {
  PendingScript* pending_script = RequestPendingScript(element);
  if (!pending_script)
    return;

  if (!pending_script->IsReady()) {
    pending_script->StartStreamingIfPossible(ScriptStreamer::kDeferred,
                                             nullptr);
  }

  DCHECK(pending_script->IsExternalOrModule());

  // "Add the element to the end of the list of scripts that will execute
  //  when the document has finished parsing associated with the Document
  //  of the parser that created the element."
  scripts_to_execute_after_parsing_.push_back(
      TraceWrapperMember<PendingScript>(this, pending_script));
}

PendingScript* HTMLParserScriptRunner::RequestPendingScript(
    Element* element) const {
  ScriptElementBase* script_element =
      ScriptElementBase::FromElementIfPossible(element);
  return script_element->Loader()->CreatePendingScript();
}

// The initial steps for 'An end tag whose tag name is "script"'
// https://html.spec.whatwg.org/#scriptEndTag
void HTMLParserScriptRunner::ProcessScriptElementInternal(
    Element* script,
    const TextPosition& script_start_position) {
  DCHECK(document_);
  DCHECK(!HasParserBlockingScript());
  {
    ScriptElementBase* element =
        ScriptElementBase::FromElementIfPossible(script);
    DCHECK(element);
    ScriptLoader* script_loader = element->Loader();
    DCHECK(script_loader);

    // FIXME: Align trace event name and function name.
    TRACE_EVENT1("blink", "HTMLParserScriptRunner::execute", "data",
                 GetTraceArgsForScriptElement(element, script_start_position));
    DCHECK(script_loader->IsParserInserted());

    if (!IsExecutingScript())
      Microtask::PerformCheckpoint(V8PerIsolateData::MainThreadIsolate());

    // "Let the old insertion point have the same value as the current
    //  insertion point.
    //  Let the insertion point be just before the next input character."
    InsertionPointRecord insertion_point_record(host_->InputStream());

    // "Increment the parser's script nesting level by one."
    HTMLParserReentryPermit::ScriptNestingLevelIncrementer
        nesting_level_incrementer =
            reentry_permit_->IncrementScriptNestingLevel();

    // "Prepare the script. This might cause some script to execute, which
    //  might cause new characters to be inserted into the tokenizer, and
    //  might cause the tokenizer to output more tokens, resulting in a
    //  reentrant invocation of the parser."
    script_loader->PrepareScript(script_start_position);

    // A part of Step 23 of https://html.spec.whatwg.org/#prepare-a-script:
    if (!script_loader->WillBeParserExecuted())
      return;

    if (script_loader->WillExecuteWhenDocumentFinishedParsing()) {
      // 1st Clause of Step 23.
      RequestDeferredScript(script);
    } else if (script_loader->ReadyToBeParserExecuted()) {
      // 5th Clause of Step 23.
      // "If ... it's an HTML parser
      //  whose script nesting level is not greater than one"
      if (reentry_permit_->ScriptNestingLevel() == 1u) {
        // "The element is the pending parsing-blocking script of the
        //  Document of the parser that created the element.
        //  (There can only be one such script per Document at a time.)"
        CHECK(!parser_blocking_script_);
        parser_blocking_script_ =
            ClassicPendingScript::Create(element, script_start_position);
      } else {
        // 6th Clause of Step 23.
        // "Immediately execute the script block,
        //  even if other scripts are already executing."
        // TODO(hiroshige): Merge the block into ScriptLoader::prepareScript().
        DCHECK_GT(reentry_permit_->ScriptNestingLevel(), 1u);
        if (parser_blocking_script_)
          parser_blocking_script_->Dispose();
        parser_blocking_script_ = nullptr;
        DoExecuteScript(
            ClassicPendingScript::Create(element, script_start_position),
            DocumentURLForScriptExecution(document_));
      }
    } else {
      // 2nd Clause of Step 23.
      RequestParsingBlockingScript(script);
    }

    // "Decrement the parser's script nesting level by one.
    //  If the parser's script nesting level is zero, then set the parser
    //  pause flag to false."
    // Implemented by ~ScriptNestingLevelIncrementer().

    // "Let the insertion point have the value of the old insertion point."
    // Implemented by ~InsertionPointRecord().
  }
}

DEFINE_TRACE(HTMLParserScriptRunner) {
  visitor->Trace(document_);
  visitor->Trace(host_);
  visitor->Trace(parser_blocking_script_);
  visitor->Trace(scripts_to_execute_after_parsing_);
  PendingScriptClient::Trace(visitor);
}
DEFINE_TRACE_WRAPPERS(HTMLParserScriptRunner) {
  visitor->TraceWrappers(parser_blocking_script_);
  for (const auto& member : scripts_to_execute_after_parsing_)
    visitor->TraceWrappers(member);
}

}  // namespace blink
