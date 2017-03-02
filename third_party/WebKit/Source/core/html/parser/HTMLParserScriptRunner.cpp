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

#include "bindings/core/v8/Microtask.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8PerIsolateData.h"
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
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/instrumentation/tracing/TracedValue.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "public/platform/Platform.h"
#include <inttypes.h>
#include <memory>

namespace blink {

namespace {

// TODO(bmcquade): move this to a shared location if we find ourselves wanting
// to trace similar data elsewhere in the codebase.
std::unique_ptr<TracedValue> getTraceArgsForScriptElement(
    Element* element,
    const TextPosition& textPosition) {
  std::unique_ptr<TracedValue> value = TracedValue::create();
  ScriptLoader* scriptLoader = toScriptLoaderIfPossible(element);
  if (scriptLoader && scriptLoader->resource())
    value->setString("url", scriptLoader->resource()->url().getString());
  if (element->ownerDocument() && element->ownerDocument()->frame()) {
    value->setString(
        "frame",
        String::format("0x%" PRIx64,
                       static_cast<uint64_t>(reinterpret_cast<intptr_t>(
                           element->ownerDocument()->frame()))));
  }
  if (textPosition.m_line.zeroBasedInt() > 0 ||
      textPosition.m_column.zeroBasedInt() > 0) {
    value->setInteger("lineNumber", textPosition.m_line.oneBasedInt());
    value->setInteger("columnNumber", textPosition.m_column.oneBasedInt());
  }
  return value;
}

bool doExecuteScript(Element* scriptElement,
                     const ScriptSourceCode& sourceCode,
                     const TextPosition& textPosition) {
  ScriptLoader* scriptLoader = toScriptLoaderIfPossible(scriptElement);
  DCHECK(scriptLoader);
  TRACE_EVENT_WITH_FLOW1(
      "blink", "HTMLParserScriptRunner ExecuteScript", scriptElement,
      TRACE_EVENT_FLAG_FLOW_IN, "data",
      getTraceArgsForScriptElement(scriptElement, textPosition));
  return scriptLoader->executeScript(sourceCode);
}

void traceParserBlockingScript(const PendingScript* pendingScript,
                               bool waitingForResources) {
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
  Element* element = pendingScript->element();
  if (!element)
    return;
  TextPosition scriptStartPosition = pendingScript->startingPosition();
  if (!pendingScript->isReady()) {
    if (waitingForResources) {
      TRACE_EVENT_WITH_FLOW1(
          "blink", "YieldParserForScriptLoadAndBlockingResources", element,
          TRACE_EVENT_FLAG_FLOW_OUT, "data",
          getTraceArgsForScriptElement(element, scriptStartPosition));
    } else {
      TRACE_EVENT_WITH_FLOW1(
          "blink", "YieldParserForScriptLoad", element,
          TRACE_EVENT_FLAG_FLOW_OUT, "data",
          getTraceArgsForScriptElement(element, scriptStartPosition));
    }
  } else if (waitingForResources) {
    TRACE_EVENT_WITH_FLOW1(
        "blink", "YieldParserForScriptBlockingResources", element,
        TRACE_EVENT_FLAG_FLOW_OUT, "data",
        getTraceArgsForScriptElement(element, scriptStartPosition));
  }
}

static KURL documentURLForScriptExecution(Document* document) {
  if (!document)
    return KURL();

  if (!document->frame()) {
    if (document->importsController())
      return document->url();
    return KURL();
  }

  // Use the URL of the currently active document for this frame.
  return document->frame()->document()->url();
}

}  // namespace

using namespace HTMLNames;

HTMLParserScriptRunner::HTMLParserScriptRunner(
    HTMLParserReentryPermit* reentryPermit,
    Document* document,
    HTMLParserScriptRunnerHost* host)
    : m_reentryPermit(reentryPermit),
      m_document(document),
      m_host(host),
      m_parserBlockingScript(nullptr) {
  DCHECK(m_host);
}

HTMLParserScriptRunner::~HTMLParserScriptRunner() {
  // Verify that detach() has been called.
  DCHECK(!m_document);
}

void HTMLParserScriptRunner::detach() {
  if (!m_document)
    return;

  if (m_parserBlockingScript)
    m_parserBlockingScript->dispose();
  m_parserBlockingScript = nullptr;

  while (!m_scriptsToExecuteAfterParsing.isEmpty()) {
    PendingScript* pendingScript = m_scriptsToExecuteAfterParsing.takeFirst();
    pendingScript->dispose();
  }
  m_document = nullptr;
  // m_reentryPermit is not cleared here, because the script runner
  // may continue to run pending scripts after the parser has
  // detached.
}

bool HTMLParserScriptRunner::isParserBlockingScriptReady() {
  DCHECK(parserBlockingScript());
  if (!m_document->isScriptExecutionReady())
    return false;
  return parserBlockingScript()->isReady();
}

// This has two callers and corresponds to different concepts in the spec:
// - When called from executeParsingBlockingScripts(), this corresponds to some
//   steps of the "Otherwise" Clause of 'An end tag whose tag name is "script"'
//   https://html.spec.whatwg.org/#scriptEndTag
// - When called from executeScriptsWaitingForParsing(), this corresponds
//   https://html.spec.whatwg.org/#execute-the-script-block
//   and thus currently this function does more than specced.
// TODO(hiroshige): Make the spec and implementation consistent.
void HTMLParserScriptRunner::executePendingScriptAndDispatchEvent(
    PendingScript* pendingScript,
    ScriptStreamer::Type pendingScriptType) {
  bool errorOccurred = false;
  ScriptSourceCode sourceCode = pendingScript->getSource(
      documentURLForScriptExecution(m_document), errorOccurred);

  // Stop watching loads before executeScript to prevent recursion if the script
  // reloads itself.
  // TODO(kouhei): Consider merging this w/ pendingScript->dispose() after the
  // if block.
  pendingScript->stopWatchingForLoad();

  if (!isExecutingScript()) {
    Microtask::performCheckpoint(V8PerIsolateData::mainThreadIsolate());
    if (pendingScriptType == ScriptStreamer::ParsingBlocking) {
      // The parser cannot be unblocked as a microtask requested another
      // resource
      if (!m_document->isScriptExecutionReady())
        return;
    }
  }

  TextPosition scriptStartPosition = pendingScript->startingPosition();
  double scriptParserBlockingTime =
      pendingScript->parserBlockingLoadStartTime();
  Element* element = pendingScript->element();

  // 1. "Let the script be the pending parsing-blocking script.
  //     There is no longer a pending parsing-blocking script."
  // Clear the pending script before possible re-entrancy from executeScript()
  pendingScript->dispose();
  pendingScript = nullptr;

  if (pendingScriptType == ScriptStreamer::ParsingBlocking) {
    m_parserBlockingScript = nullptr;
  }

  if (ScriptLoader* scriptLoader = toScriptLoaderIfPossible(element)) {
    // 7. "Increment the parser's script nesting level by one (it should be
    //     zero before this step, so this sets it to one)."
    HTMLParserReentryPermit::ScriptNestingLevelIncrementer
        nestingLevelIncrementer =
            m_reentryPermit->incrementScriptNestingLevel();

    IgnoreDestructiveWriteCountIncrementer
        ignoreDestructiveWriteCountIncrementer(m_document);

    // 8. "Execute the script."
    if (errorOccurred) {
      TRACE_EVENT_WITH_FLOW1(
          "blink", "HTMLParserScriptRunner ExecuteScriptFailed", element,
          TRACE_EVENT_FLAG_FLOW_IN, "data",
          getTraceArgsForScriptElement(element, scriptStartPosition));
      scriptLoader->dispatchErrorEvent();
    } else {
      DCHECK(isExecutingScript());
      if (scriptParserBlockingTime > 0.0) {
        DocumentParserTiming::from(*m_document)
            .recordParserBlockedOnScriptLoadDuration(
                monotonicallyIncreasingTime() - scriptParserBlockingTime,
                scriptLoader->wasCreatedDuringDocumentWrite());
      }
      if (!doExecuteScript(element, sourceCode, scriptStartPosition)) {
        scriptLoader->dispatchErrorEvent();
      } else {
        element->dispatchEvent(Event::create(EventTypeNames::load));
      }
    }

    // 9. "Decrement the parser's script nesting level by one.
    //     If the parser's script nesting level is zero
    //     (which it always should be at this point),
    //     then set the parser pause flag to false."
    // This is implemented by ~ScriptNestingLevelIncrementer().
  }

  DCHECK(!isExecutingScript());
}

void fetchBlockedDocWriteScript(Element* script,
                                bool isParserInserted,
                                const TextPosition& scriptStartPosition) {
  DCHECK(script);

  ScriptLoader* scriptLoader =
      ScriptLoader::create(script, isParserInserted, false, false);
  DCHECK(scriptLoader);
  scriptLoader->setFetchDocWrittenScriptDeferIdle();
  scriptLoader->prepareScript(scriptStartPosition);
}

void emitWarningForDocWriteScripts(const String& url, Document& document) {
  String message =
      "The Parser-blocking, cross site (i.e. different eTLD+1) "
      "script, " +
      url +
      ", invoked via document.write was NOT BLOCKED on this page load, but MAY "
      "be blocked by the browser in future page loads with poor network "
      "connectivity.";
  document.addConsoleMessage(
      ConsoleMessage::create(JSMessageSource, WarningMessageLevel, message));
  WTFLogAlways("%s", message.utf8().data());
}

void emitErrorForDocWriteScripts(const String& url, Document& document) {
  String message =
      "Network request for the parser-blocking, cross site "
      "(i.e. different eTLD+1) script, " +
      url +
      ", invoked via document.write was BLOCKED by the browser due to poor "
      "network connectivity. ";
  document.addConsoleMessage(
      ConsoleMessage::create(JSMessageSource, ErrorMessageLevel, message));
  WTFLogAlways("%s", message.utf8().data());
}

void HTMLParserScriptRunner::possiblyFetchBlockedDocWriteScript(
    PendingScript* pendingScript) {
  // If the script was blocked as part of document.write intervention,
  // then send an asynchronous GET request with an interventions header.

  if (!parserBlockingScript())
    return;

  if (parserBlockingScript() != pendingScript)
    return;

  Element* element = parserBlockingScript()->element();

  ScriptLoader* scriptLoader = toScriptLoaderIfPossible(element);
  if (!scriptLoader || !scriptLoader->disallowedFetchForDocWrittenScript())
    return;

  if (!pendingScript->errorOccurred()) {
    emitWarningForDocWriteScripts(pendingScript->resource()->url().getString(),
                                  *m_document);
    return;
  }

  // Due to dependency violation, not able to check the exact error to be
  // ERR_CACHE_MISS but other errors are rare with
  // WebCachePolicy::ReturnCacheDataDontLoad.

  emitErrorForDocWriteScripts(pendingScript->resource()->url().getString(),
                              *m_document);
  TextPosition startingPosition = parserBlockingScript()->startingPosition();
  bool isParserInserted = scriptLoader->isParserInserted();
  // Remove this resource entry from memory cache as the new request
  // should not join onto this existing entry.
  memoryCache()->remove(pendingScript->resource());
  fetchBlockedDocWriteScript(element, isParserInserted, startingPosition);
}

void HTMLParserScriptRunner::pendingScriptFinished(
    PendingScript* pendingScript) {
  // Handle cancellations of parser-blocking script loads without
  // notifying the host (i.e., parser) if these were initiated by nested
  // document.write()s. The cancellation may have been triggered by
  // script execution to signal an abrupt stop (e.g., window.close().)
  //
  // The parser is unprepared to be told, and doesn't need to be.
  if (isExecutingScript() && pendingScript->resource()->wasCanceled()) {
    pendingScript->dispose();

    if (pendingScript == parserBlockingScript()) {
      m_parserBlockingScript = nullptr;
    } else {
      CHECK_EQ(pendingScript, m_scriptsToExecuteAfterParsing.first());

      // TODO(hiroshige): Remove this CHECK() before going to beta.
      // This is only to make clusterfuzz to find a test case that executes
      // this code path.
      CHECK(false);

      m_scriptsToExecuteAfterParsing.removeFirst();
      // TODO(hiroshige): executeScriptsWaitingForParsing() should be
      // called later at the appropriate time. https://crbug.com/696775
    }

    return;
  }

  // If the script was blocked as part of document.write intervention,
  // then send an asynchronous GET request with an interventions header.
  possiblyFetchBlockedDocWriteScript(pendingScript);

  m_host->notifyScriptLoaded(pendingScript);
}

// 'An end tag whose tag name is "script"'
// https://html.spec.whatwg.org/#scriptEndTag
//
// Script handling lives outside the tree builder to keep each class simple.
void HTMLParserScriptRunner::processScriptElement(
    Element* scriptElement,
    const TextPosition& scriptStartPosition) {
  DCHECK(scriptElement);
  TRACE_EVENT1(
      "blink", "HTMLParserScriptRunner::execute", "data",
      getTraceArgsForScriptElement(scriptElement, scriptStartPosition));
  // FIXME: If scripting is disabled, always just return.

  bool hadPreloadScanner = m_host->hasPreloadScanner();

  // Initial steps of 'An end tag whose tag name is "script"'.
  // Try to execute the script given to us.
  processScriptElementInternal(scriptElement, scriptStartPosition);

  // "At this stage, if there is a pending parsing-blocking script, then:"
  if (hasParserBlockingScript()) {
    // - "If the script nesting level is not zero:"
    if (isExecutingScript()) {
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

    traceParserBlockingScript(parserBlockingScript(),
                              !m_document->isScriptExecutionReady());
    m_parserBlockingScript->markParserBlockingLoadStartTime();

    // If preload scanner got created, it is missing the source after the
    // current insertion point. Append it and scan.
    if (!hadPreloadScanner && m_host->hasPreloadScanner())
      m_host->appendCurrentInputStreamToPreloadScannerAndScan();

    executeParsingBlockingScripts();
  }
}

bool HTMLParserScriptRunner::hasParserBlockingScript() const {
  return parserBlockingScript();
}

// The "Otherwise" Clause of 'An end tag whose tag name is "script"'
// https://html.spec.whatwg.org/#scriptEndTag
void HTMLParserScriptRunner::executeParsingBlockingScripts() {
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
  while (hasParserBlockingScript() && isParserBlockingScriptReady()) {
    DCHECK(m_document);
    DCHECK(!isExecutingScript());
    DCHECK(m_document->isScriptExecutionReady());

    // 6. "Let the insertion point be just before the next input character."
    InsertionPointRecord insertionPointRecord(m_host->inputStream());

    // 1., 7.--9.
    executePendingScriptAndDispatchEvent(m_parserBlockingScript,
                                         ScriptStreamer::ParsingBlocking);

    // 10. "Let the insertion point be undefined again."
    // Implemented as ~InsertionPointRecord().

    // 11. "If there is once again a pending parsing-blocking script, then
    //      repeat these steps from step 1."
  }
}

void HTMLParserScriptRunner::executeScriptsWaitingForLoad(
    PendingScript* pendingScript) {
  TRACE_EVENT0("blink", "HTMLParserScriptRunner::executeScriptsWaitingForLoad");
  DCHECK(!isExecutingScript());
  DCHECK(hasParserBlockingScript());
  DCHECK_EQ(pendingScript, parserBlockingScript());
  DCHECK(parserBlockingScript()->isReady());
  executeParsingBlockingScripts();
}

void HTMLParserScriptRunner::executeScriptsWaitingForResources() {
  TRACE_EVENT0("blink",
               "HTMLParserScriptRunner::executeScriptsWaitingForResources");
  DCHECK(m_document);
  DCHECK(!isExecutingScript());
  DCHECK(m_document->isScriptExecutionReady());
  executeParsingBlockingScripts();
}

// Step 3 of https://html.spec.whatwg.org/#the-end:
// "If the list of scripts that will execute when the document has
//  finished parsing is not empty, run these substeps:"
bool HTMLParserScriptRunner::executeScriptsWaitingForParsing() {
  TRACE_EVENT0("blink",
               "HTMLParserScriptRunner::executeScriptsWaitingForParsing");

  while (!m_scriptsToExecuteAfterParsing.isEmpty()) {
    DCHECK(!isExecutingScript());
    DCHECK(!hasParserBlockingScript());
    DCHECK(m_scriptsToExecuteAfterParsing.first()->resource());

    // 1. "Spin the event loop until the first script in the list of scripts
    //     that will execute when the document has finished parsing
    //     has its "ready to be parser-executed" flag set and
    //     the parser's Document has no style sheet that is blocking scripts."
    // TODO(hiroshige): Is the latter part checked anywhere?
    if (!m_scriptsToExecuteAfterParsing.first()->isReady()) {
      m_scriptsToExecuteAfterParsing.first()->watchForLoad(this);
      traceParserBlockingScript(m_scriptsToExecuteAfterParsing.first().get(),
                                !m_document->isScriptExecutionReady());
      m_scriptsToExecuteAfterParsing.first()->markParserBlockingLoadStartTime();
      return false;
    }

    // 3. "Remove the first script element from the list of scripts that will
    //     execute when the document has finished parsing (i.e. shift out the
    //     first entry in the list)."
    PendingScript* first = m_scriptsToExecuteAfterParsing.takeFirst();

    // 2. "Execute the first script in the list of scripts that will execute
    //     when the document has finished parsing."
    executePendingScriptAndDispatchEvent(first, ScriptStreamer::Deferred);

    // FIXME: What is this m_document check for?
    if (!m_document)
      return false;

    // 4. "If the list of scripts that will execute when the document has
    //     finished parsing is still not empty, repeat these substeps again
    //     from substep 1."
  }
  return true;
}

// 2nd Clause, Step 23 of https://html.spec.whatwg.org/#prepare-a-script
void HTMLParserScriptRunner::requestParsingBlockingScript(Element* element) {
  // "The element is the pending parsing-blocking script of the Document of
  //  the parser that created the element.
  //  (There can only be one such script per Document at a time.)"
  CHECK(!parserBlockingScript());
  m_parserBlockingScript = requestPendingScript(element);
  if (!parserBlockingScript())
    return;

  DCHECK(parserBlockingScript()->resource());

  // We only care about a load callback if resource is not already in the cache.
  // Callers will attempt to run the m_parserBlockingScript if possible before
  // returning control to the parser.
  if (!parserBlockingScript()->isReady()) {
    if (m_document->frame()) {
      ScriptState* scriptState = ScriptState::forMainWorld(m_document->frame());
      if (scriptState) {
        ScriptStreamer::startStreaming(
            m_parserBlockingScript, ScriptStreamer::ParsingBlocking,
            m_document->frame()->settings(), scriptState,
            TaskRunnerHelper::get(TaskType::Networking, m_document));
      }
    }

    m_parserBlockingScript->watchForLoad(this);
  }
}

// 1st Clause, Step 23 of https://html.spec.whatwg.org/#prepare-a-script
void HTMLParserScriptRunner::requestDeferredScript(Element* element) {
  PendingScript* pendingScript = requestPendingScript(element);
  if (!pendingScript)
    return;

  if (m_document->frame() && !pendingScript->isReady()) {
    ScriptState* scriptState = ScriptState::forMainWorld(m_document->frame());
    if (scriptState) {
      ScriptStreamer::startStreaming(
          pendingScript, ScriptStreamer::Deferred,
          m_document->frame()->settings(), scriptState,
          TaskRunnerHelper::get(TaskType::Networking, m_document));
    }
  }

  DCHECK(pendingScript->resource());

  // "Add the element to the end of the list of scripts that will execute
  //  when the document has finished parsing associated with the Document
  //  of the parser that created the element."
  m_scriptsToExecuteAfterParsing.append(pendingScript);
}

PendingScript* HTMLParserScriptRunner::requestPendingScript(
    Element* element) const {
  ScriptResource* resource = toScriptLoaderIfPossible(element)->resource();
  // Here |resource| should be non-null. If it were nullptr,
  // ScriptLoader::fetchScript() should have returned false and
  // thus the control shouldn't have reached here.
  CHECK(resource);
  return PendingScript::create(element, resource);
}

// The initial steps for 'An end tag whose tag name is "script"'
// https://html.spec.whatwg.org/#scriptEndTag
void HTMLParserScriptRunner::processScriptElementInternal(
    Element* script,
    const TextPosition& scriptStartPosition) {
  DCHECK(m_document);
  DCHECK(!hasParserBlockingScript());
  {
    ScriptLoader* scriptLoader = toScriptLoaderIfPossible(script);

    // This contains both a DCHECK and a null check since we should not
    // be getting into the case of a null script element, but seem to be from
    // time to time. The assertion is left in to help find those cases and
    // is being tracked by <https://bugs.webkit.org/show_bug.cgi?id=60559>.
    DCHECK(scriptLoader);
    if (!scriptLoader)
      return;

    DCHECK(scriptLoader->isParserInserted());

    if (!isExecutingScript())
      Microtask::performCheckpoint(V8PerIsolateData::mainThreadIsolate());

    // "Let the old insertion point have the same value as the current
    //  insertion point.
    //  Let the insertion point be just before the next input character."
    InsertionPointRecord insertionPointRecord(m_host->inputStream());

    // "Increment the parser's script nesting level by one."
    HTMLParserReentryPermit::ScriptNestingLevelIncrementer
        nestingLevelIncrementer =
            m_reentryPermit->incrementScriptNestingLevel();

    // "Prepare the script. This might cause some script to execute, which
    //  might cause new characters to be inserted into the tokenizer, and
    //  might cause the tokenizer to output more tokens, resulting in a
    //  reentrant invocation of the parser."
    scriptLoader->prepareScript(scriptStartPosition);

    // A part of Step 23 of https://html.spec.whatwg.org/#prepare-a-script:
    if (!scriptLoader->willBeParserExecuted())
      return;

    if (scriptLoader->willExecuteWhenDocumentFinishedParsing()) {
      // 1st Clause of Step 23.
      requestDeferredScript(script);
    } else if (scriptLoader->readyToBeParserExecuted()) {
      // 5th Clause of Step 23.
      // "If ... it's an HTML parser
      //  whose script nesting level is not greater than one"
      if (m_reentryPermit->scriptNestingLevel() == 1u) {
        // "The element is the pending parsing-blocking script of the
        //  Document of the parser that created the element.
        //  (There can only be one such script per Document at a time.)"
        CHECK(!m_parserBlockingScript);
        m_parserBlockingScript =
            PendingScript::create(script, scriptStartPosition);
      } else {
        // 6th Clause of Step 23.
        // "Immediately execute the script block,
        //  even if other scripts are already executing."
        // TODO(hiroshige): Merge the block into ScriptLoader::prepareScript().
        DCHECK_GT(m_reentryPermit->scriptNestingLevel(), 1u);
        if (m_parserBlockingScript)
          m_parserBlockingScript->dispose();
        m_parserBlockingScript = nullptr;
        ScriptSourceCode sourceCode(script->textContent(),
                                    documentURLForScriptExecution(m_document),
                                    scriptStartPosition);
        doExecuteScript(script, sourceCode, scriptStartPosition);
      }
    } else {
      // 2nd Clause of Step 23.
      requestParsingBlockingScript(script);
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
  visitor->trace(m_document);
  visitor->trace(m_host);
  visitor->trace(m_parserBlockingScript);
  visitor->trace(m_scriptsToExecuteAfterParsing);
  PendingScriptClient::trace(visitor);
}

}  // namespace blink
