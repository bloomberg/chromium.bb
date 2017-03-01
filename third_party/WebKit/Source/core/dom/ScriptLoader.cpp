/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/dom/ScriptLoader.h"

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "core/HTMLNames.h"
#include "core/SVGNames.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentParserTiming.h"
#include "core/dom/IgnoreDestructiveWriteCountIncrementer.h"
#include "core/dom/ScriptLoaderClient.h"
#include "core/dom/ScriptRunner.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/dom/Text.h"
#include "core/events/Event.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/SubresourceIntegrity.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/CrossOriginAttribute.h"
#include "core/html/HTMLScriptElement.h"
#include "core/html/imports/HTMLImport.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/svg/SVGScriptElement.h"
#include "platform/WebFrameScheduler.h"
#include "platform/loader/fetch/AccessControlStatus.h"
#include "platform/loader/fetch/FetchRequest.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/network/mime/MIMETypeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebCachePolicy.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringHash.h"

namespace blink {

ScriptLoader::ScriptLoader(Element* element,
                           bool parserInserted,
                           bool alreadyStarted,
                           bool createdDuringDocumentWrite)
    : m_element(element),
      m_startLineNumber(WTF::OrdinalNumber::beforeFirst()),
      m_haveFiredLoad(false),
      m_willBeParserExecuted(false),
      m_willExecuteWhenDocumentFinishedParsing(false),
      m_createdDuringDocumentWrite(createdDuringDocumentWrite),
      m_asyncExecType(ScriptRunner::None),
      m_documentWriteIntervention(
          DocumentWriteIntervention::DocumentWriteInterventionNone) {
  DCHECK(m_element);

  // https://html.spec.whatwg.org/#already-started
  // "The cloning steps for script elements must set the "already started"
  //  flag on the copy if it is set on the element being cloned."
  // TODO(hiroshige): Cloning is implemented together with
  // {HTML,SVG}ScriptElement::cloneElementWithoutAttributesAndChildren().
  // Clean up these later.
  if (alreadyStarted)
    m_alreadyStarted = true;

  if (parserInserted) {
    // https://html.spec.whatwg.org/#parser-inserted
    // "It is set by the HTML parser and the XML parser
    //  on script elements they insert"
    m_parserInserted = true;

    // https://html.spec.whatwg.org/#non-blocking
    // "It is unset by the HTML parser and the XML parser
    //  on script elements they insert."
    m_nonBlocking = false;
  }

  if (parserInserted && element->document().scriptableDocumentParser() &&
      !element->document().isInDocumentWrite())
    m_startLineNumber =
        element->document().scriptableDocumentParser()->lineNumber();
}

ScriptLoader::~ScriptLoader() {}

DEFINE_TRACE(ScriptLoader) {
  visitor->trace(m_element);
  visitor->trace(m_resource);
  visitor->trace(m_pendingScript);
  PendingScriptClient::trace(visitor);
}

void ScriptLoader::setFetchDocWrittenScriptDeferIdle() {
  DCHECK(!m_createdDuringDocumentWrite);
  m_documentWriteIntervention =
      DocumentWriteIntervention::FetchDocWrittenScriptDeferIdle;
}

void ScriptLoader::didNotifySubtreeInsertionsToDocument() {
  if (!m_parserInserted)
    prepareScript();  // FIXME: Provide a real starting line number here.
}

void ScriptLoader::childrenChanged() {
  if (!m_parserInserted && m_element->isConnected())
    prepareScript();  // FIXME: Provide a real starting line number here.
}

void ScriptLoader::handleSourceAttribute(const String& sourceUrl) {
  if (ignoresLoadRequest() || sourceUrl.isEmpty())
    return;

  prepareScript();  // FIXME: Provide a real starting line number here.
}

void ScriptLoader::handleAsyncAttribute() {
  // https://html.spec.whatwg.org/#non-blocking
  // "In addition, whenever a script element whose "non-blocking" flag is set
  //  has an async content attribute added, the element's "non-blocking" flag
  //  must be unset."
  m_nonBlocking = false;
}

void ScriptLoader::detachPendingScript() {
  if (!m_pendingScript)
    return;
  m_pendingScript->dispose();
  m_pendingScript = nullptr;
}

static bool isLegacySupportedJavaScriptLanguage(const String& language) {
  // Mozilla 1.8 accepts javascript1.0 - javascript1.7, but WinIE 7 accepts only
  // javascript1.1 - javascript1.3.
  // Mozilla 1.8 and WinIE 7 both accept javascript and livescript.
  // WinIE 7 accepts ecmascript and jscript, but Mozilla 1.8 doesn't.
  // Neither Mozilla 1.8 nor WinIE 7 accept leading or trailing whitespace.
  // We want to accept all the values that either of these browsers accept, but
  // not other values.

  // FIXME: This function is not HTML5 compliant. These belong in the MIME
  // registry as "text/javascript<version>" entries.
  return equalIgnoringASCIICase(language, "javascript") ||
         equalIgnoringASCIICase(language, "javascript1.0") ||
         equalIgnoringASCIICase(language, "javascript1.1") ||
         equalIgnoringASCIICase(language, "javascript1.2") ||
         equalIgnoringASCIICase(language, "javascript1.3") ||
         equalIgnoringASCIICase(language, "javascript1.4") ||
         equalIgnoringASCIICase(language, "javascript1.5") ||
         equalIgnoringASCIICase(language, "javascript1.6") ||
         equalIgnoringASCIICase(language, "javascript1.7") ||
         equalIgnoringASCIICase(language, "livescript") ||
         equalIgnoringASCIICase(language, "ecmascript") ||
         equalIgnoringASCIICase(language, "jscript");
}

void ScriptLoader::dispatchErrorEvent() {
  m_element->dispatchEvent(Event::create(EventTypeNames::error));
}

void ScriptLoader::dispatchLoadEvent() {
  if (ScriptLoaderClient* client = this->client())
    client->dispatchLoadEvent();
  setHaveFiredLoadEvent(true);
}

bool ScriptLoader::isValidScriptTypeAndLanguage(
    const String& type,
    const String& language,
    LegacyTypeSupport supportLegacyTypes) {
  // FIXME: isLegacySupportedJavaScriptLanguage() is not valid HTML5. It is used
  // here to maintain backwards compatibility with existing layout tests. The
  // specific violations are:
  // - Allowing type=javascript. type= should only support MIME types, such as
  //   text/javascript.
  // - Allowing a different set of languages for language= and type=. language=
  //   supports Javascript 1.1 and 1.4-1.6, but type= does not.
  if (type.isEmpty()) {
    return language.isEmpty() ||  // assume text/javascript.
           MIMETypeRegistry::isSupportedJavaScriptMIMEType("text/" +
                                                           language) ||
           isLegacySupportedJavaScriptLanguage(language);
  } else if (RuntimeEnabledFeatures::moduleScriptsEnabled() &&
             type == "module") {
    return true;
  } else if (MIMETypeRegistry::isSupportedJavaScriptMIMEType(
                 type.stripWhiteSpace()) ||
             (supportLegacyTypes == AllowLegacyTypeInTypeAttribute &&
              isLegacySupportedJavaScriptLanguage(type))) {
    return true;
  }

  return false;
}

bool ScriptLoader::isScriptTypeSupported(
    LegacyTypeSupport supportLegacyTypes) const {
  return isValidScriptTypeAndLanguage(client()->typeAttributeValue(),
                                      client()->languageAttributeValue(),
                                      supportLegacyTypes);
}

// https://html.spec.whatwg.org/#prepare-a-script
bool ScriptLoader::prepareScript(const TextPosition& scriptStartPosition,
                                 LegacyTypeSupport supportLegacyTypes) {
  // 1. "If the script element is marked as having "already started", then
  //     abort these steps at this point. The script is not executed."
  if (m_alreadyStarted)
    return false;

  ScriptLoaderClient* client = this->client();

  // 2. "If the element has its "parser-inserted" flag set, then
  //     set was-parser-inserted to true and unset the element's
  //     "parser-inserted" flag.
  //     Otherwise, set was-parser-inserted to false."
  bool wasParserInserted;
  if (m_parserInserted) {
    wasParserInserted = true;
    m_parserInserted = false;
  } else {
    wasParserInserted = false;
  }

  // 3. "If was-parser-inserted is true and the element does not have an
  //     async attribute, then set the element's "non-blocking" flag to true."
  if (wasParserInserted && !client->asyncAttributeValue())
    m_nonBlocking = true;

  // 4. "If the element has no src attribute, and its child nodes, if any,
  //     consist only of comment nodes and empty Text nodes,
  //     then abort these steps at this point. The script is not executed."
  // FIXME: HTML5 spec says we should check that all children are either
  // comments or empty text nodes.
  if (!client->hasSourceAttribute() && !m_element->hasChildren())
    return false;

  // 5. "If the element is not connected, then abort these steps.
  //     The script is not executed."
  if (!m_element->isConnected())
    return false;

  // 6.
  // TODO(hiroshige): Annotate and/or cleanup this step.
  if (!isScriptTypeSupported(supportLegacyTypes))
    return false;

  // 7. "If was-parser-inserted is true,
  //     then flag the element as "parser-inserted" again,
  //     and set the element's "non-blocking" flag to false."
  if (wasParserInserted) {
    m_parserInserted = true;
    m_nonBlocking = false;
  }

  // 8. "Set the element's "already started" flag."
  m_alreadyStarted = true;

  // 9. "If the element is flagged as "parser-inserted", but the element's
  // node document is not the Document of the parser that created the element,
  // then abort these steps."
  // FIXME: If script is parser inserted, verify it's still in the original
  // document.
  Document& elementDocument = m_element->document();
  Document* contextDocument = elementDocument.contextDocument();
  if (!contextDocument)
    return false;

  // 10. "If scripting is disabled for the script element, then abort these
  //      steps at this point. The script is not executed."
  if (!contextDocument->allowExecutingScripts(m_element))
    return false;

  // 13.
  if (!isScriptForEventSupported())
    return false;

  // 14. "If the script element has a charset attribute,
  //      then let encoding be the result of
  //      getting an encoding from the value of the charset attribute."
  //     "If the script element does not have a charset attribute,
  //      or if getting an encoding failed, let encoding
  //      be the same as the encoding of the script element's node document."
  // TODO(hiroshige): Should we handle failure in getting an encoding?
  String encoding;
  if (!client->charsetAttributeValue().isEmpty())
    encoding = client->charsetAttributeValue();
  else
    encoding = elementDocument.characterSet();

  // Steps 15--20 are handled in fetchScript().

  // 21. "If the element has a src content attribute, run these substeps:"
  if (client->hasSourceAttribute()) {
    FetchRequest::DeferOption defer = FetchRequest::NoDefer;
    if (!m_parserInserted || client->asyncAttributeValue() ||
        client->deferAttributeValue())
      defer = FetchRequest::LazyLoad;
    if (m_documentWriteIntervention ==
        DocumentWriteIntervention::FetchDocWrittenScriptDeferIdle)
      defer = FetchRequest::IdleLoad;
    if (!fetchScript(client->sourceAttributeValue(), encoding, defer))
      return false;
  }

  // 22. "If the element does not have a src content attribute,
  //      run these substeps:"

  // 22.1. "Let source text be the value of the text IDL attribute."
  // This step is done later:
  // - in ScriptLoader::pendingScript() (Step 23, 6th Clause),
  //   as Element::textFromChildren() in ScriptLoader::scriptContent(),
  // - in HTMLParserScriptRunner::processScriptElementInternal()
  //   (Duplicated code of Step 23, 6th Clause),
  //   as Element::textContent(),
  // - in XMLDocumentParser::endElementNs() (Step 23, 5th Clause),
  //   as Element::textFromChildren() in ScriptLoader::scriptContent(),
  // - PendingScript::getSource() (Indirectly used via
  //   HTMLParserScriptRunner::processScriptElementInternal(),
  //   Step 23, 5th Clause),
  //   as Element::textContent().
  // TODO(hiroshige): Make them merged or consistent.

  // 22.2. "Switch on the script's type:"
  // TODO(hiroshige): Clarify how Step 22.2 is implemented for "classic".
  // TODO(hiroshige): Implement Step 22.2 for "module".

  // [Intervention]
  // Since the asynchronous, low priority fetch for doc.written blocked
  // script is not for execution, return early from here. Watch for its
  // completion to be able to remove it from the memory cache.
  if (m_documentWriteIntervention ==
      DocumentWriteIntervention::FetchDocWrittenScriptDeferIdle) {
    m_pendingScript = PendingScript::create(m_element, m_resource.get());
    m_pendingScript->watchForLoad(this);
    return true;
  }

  // 23. "Then, follow the first of the following options that describes the
  //      situation:"

  // Three flags are used to instruct the caller of prepareScript() to execute
  // a part of Step 23, when |m_willBeParserExecuted| is true:
  // - |m_willBeParserExecuted|
  // - |m_willExecuteWhenDocumentFinishedParsing|
  // - |m_readyToBeParserExecuted|
  // TODO(hiroshige): Clean up the dependency.

  // 1st Clause:
  // - "If the script's type is "classic", and
  //    the element has a src attribute, and the element has a defer attribute,
  //    and the element has been flagged as "parser-inserted",
  //    and the element does not have an async attribute"
  // TODO(hiroshige): Check the script's type and implement "module" case.
  if (client->hasSourceAttribute() && client->deferAttributeValue() &&
      m_parserInserted && !client->asyncAttributeValue()) {
    // This clause is implemented by the caller-side of prepareScript():
    // - HTMLParserScriptRunner::requestDeferredScript(), and
    // - TODO(hiroshige): Investigate XMLDocumentParser::endElementNs()
    m_willExecuteWhenDocumentFinishedParsing = true;
    m_willBeParserExecuted = true;

    return true;
  }

  // 2nd Clause:
  // - "If the script's type is "classic",
  //    and the element has a src attribute,
  //    and the element has been flagged as "parser-inserted",
  //    and the element does not have an async attribute"
  // TODO(hiroshige): Check the script's type.
  if (client->hasSourceAttribute() && m_parserInserted &&
      !client->asyncAttributeValue()) {
    // This clause is implemented by the caller-side of prepareScript():
    // - HTMLParserScriptRunner::requestParsingBlockingScript()
    // - TODO(hiroshige): Investigate XMLDocumentParser::endElementNs()
    m_willBeParserExecuted = true;

    return true;
  }

  // 5th Clause:
  // TODO(hiroshige): Reorder the clauses to match the spec.
  // - "If the element does not have a src attribute,
  //    and the element has been flagged as "parser-inserted",
  //    and either the parser that created the script is an XML parser
  //      or it's an HTML parser whose script nesting level is not greater than
  //      one,
  //    and the Document of the HTML parser or XML parser that created
  //      the script element has a style sheet that is blocking scripts"
  // The last part "... has a style sheet that is blocking scripts"
  // is implemented in Document::isScriptExecutionReady().
  // Part of the condition check is done in
  // HTMLParserScriptRunner::processScriptElementInternal().
  // TODO(hiroshige): Clean up the split condition check.
  if (!client->hasSourceAttribute() && m_parserInserted &&
      !elementDocument.isScriptExecutionReady()) {
    // The former part of this clause is
    // implemented by the caller-side of prepareScript():
    // - HTMLParserScriptRunner::requestParsingBlockingScript()
    // - TODO(hiroshige): Investigate XMLDocumentParser::endElementNs()
    m_willBeParserExecuted = true;
    // "Set the element's "ready to be parser-executed" flag."
    m_readyToBeParserExecuted = true;

    return true;
  }

  // 3rd Clause:
  // - "If the script's type is "classic",
  //    and the element has a src attribute,
  //    and the element does not have an async attribute,
  //    and the element does not have the "non-blocking" flag set"
  // TODO(hiroshige): Check the script's type and implement "module" case.
  if (client->hasSourceAttribute() && !client->asyncAttributeValue() &&
      !m_nonBlocking) {
    // "Add the element to the end of the list of scripts that will execute
    // in order as soon as possible associated with the node document of the
    // script element at the time the prepare a script algorithm started."
    m_pendingScript = PendingScript::create(m_element, m_resource.get());
    m_asyncExecType = ScriptRunner::InOrder;
    // TODO(hiroshige): Here |contextDocument| is used as "node document"
    // while Step 14 uses |elementDocument| as "node document". Fix this.
    contextDocument->scriptRunner()->queueScriptForExecution(this,
                                                             m_asyncExecType);
    // Note that watchForLoad can immediately call pendingScriptFinished.
    m_pendingScript->watchForLoad(this);
    // The part "When the script is ready..." is implemented in
    // ScriptRunner::notifyScriptReady().
    // TODO(hiroshige): Annotate it.

    return true;
  }

  // 4th Clause:
  // - "If the script's type is "classic", and the element has a src attribute"
  // TODO(hiroshige): Check the script's type and implement "module" case.
  if (client->hasSourceAttribute()) {
    // "The element must be added to the set of scripts that will execute
    //  as soon as possible of the node document of the script element at the
    //  time the prepare a script algorithm started."
    m_pendingScript = PendingScript::create(m_element, m_resource.get());
    m_asyncExecType = ScriptRunner::Async;
    LocalFrame* frame = m_element->document().frame();
    if (frame) {
      ScriptState* scriptState = ScriptState::forMainWorld(frame);
      if (scriptState)
        ScriptStreamer::startStreaming(
            m_pendingScript.get(), ScriptStreamer::Async, frame->settings(),
            scriptState, frame->frameScheduler()->loadingTaskRunner());
    }
    // TODO(hiroshige): Here |contextDocument| is used as "node document"
    // while Step 14 uses |elementDocument| as "node document". Fix this.
    contextDocument->scriptRunner()->queueScriptForExecution(this,
                                                             m_asyncExecType);
    // Note that watchForLoad can immediately call pendingScriptFinished.
    m_pendingScript->watchForLoad(this);
    // The part "When the script is ready..." is implemented in
    // ScriptRunner::notifyScriptReady().
    // TODO(hiroshige): Annotate it.

    return true;
  }

  // 6th Clause:
  // - "Otherwise"
  // "Immediately execute the script block,
  //  even if other scripts are already executing."
  // Note: this block is also duplicated in
  // HTMLParserScriptRunner::processScriptElementInternal().
  // TODO(hiroshige): Merge the duplicated code.

  // This clause is executed only if the script's type is "classic"
  // and the element doesn't have a src attribute.

  // Reset line numbering for nested writes.
  TextPosition position = elementDocument.isInDocumentWrite()
                              ? TextPosition()
                              : scriptStartPosition;
  KURL scriptURL = (!elementDocument.isInDocumentWrite() && m_parserInserted)
                       ? elementDocument.url()
                       : KURL();

  if (!executeScript(ScriptSourceCode(scriptContent(), scriptURL, position))) {
    dispatchErrorEvent();
    return false;
  }

  return true;
}

// Steps 15--21 of https://html.spec.whatwg.org/#prepare-a-script
bool ScriptLoader::fetchScript(const String& sourceUrl,
                               const String& encoding,
                               FetchRequest::DeferOption defer) {
  DCHECK(m_element);

  Document* elementDocument = &(m_element->document());
  if (!m_element->isConnected() || m_element->document() != elementDocument)
    return false;

  DCHECK(!m_resource);
  // 21. "If the element has a src content attribute, run these substeps:"
  if (!stripLeadingAndTrailingHTMLSpaces(sourceUrl).isEmpty()) {
    // 21.4. "Parse src relative to the element's node document."
    FetchRequest request(
        ResourceRequest(elementDocument->completeURL(sourceUrl)),
        m_element->localName());

    // 15. "Let CORS setting be the current state of the element's
    //      crossorigin content attribute."
    CrossOriginAttributeValue crossOrigin = crossOriginAttributeValue(
        m_element->fastGetAttribute(HTMLNames::crossoriginAttr));

    // 16. "Let module script credentials mode be determined by switching
    //      on CORS setting:"
    // TODO(hiroshige): Implement this step for "module".

    // 21.6, "classic": "Fetch a classic script given ... CORS setting
    //                   ... and encoding."
    if (crossOrigin != CrossOriginAttributeNotSet)
      request.setCrossOriginAccessControl(elementDocument->getSecurityOrigin(),
                                          crossOrigin);

    request.setCharset(encoding);

    // 17. "If the script element has a nonce attribute,
    //      then let cryptographic nonce be that attribute's value.
    //      Otherwise, let cryptographic nonce be the empty string."
    if (ContentSecurityPolicy::isNonceableElement(m_element.get()))
      request.setContentSecurityPolicyNonce(client()->nonce());

    // 19. "Let parser state be "parser-inserted"
    //      if the script element has been flagged as "parser-inserted",
    //      and "not parser-inserted" otherwise."
    request.setParserDisposition(isParserInserted() ? ParserInserted
                                                    : NotParserInserted);

    request.setDefer(defer);

    // 18. "If the script element has an integrity attribute,
    //      then let integrity metadata be that attribute's value.
    //      Otherwise, let integrity metadata be the empty string."
    String integrityAttr =
        m_element->fastGetAttribute(HTMLNames::integrityAttr);
    if (!integrityAttr.isEmpty()) {
      IntegrityMetadataSet metadataSet;
      SubresourceIntegrity::parseIntegrityAttribute(integrityAttr, metadataSet,
                                                    elementDocument);
      request.setIntegrityMetadata(metadataSet);
    }

    // [Intervention]
    if (m_documentWriteIntervention ==
        DocumentWriteIntervention::FetchDocWrittenScriptDeferIdle) {
      request.mutableResourceRequest().setHTTPHeaderField(
          "Intervention",
          "<https://www.chromestatus.com/feature/5718547946799104>");
    }

    // 21.6. "Switch on the script's type:"

    // - "classic":
    //   "Fetch a classic script given url, settings, cryptographic nonce,
    //    integrity metadata, parser state, CORS setting, and encoding."
    m_resource = ScriptResource::fetch(request, elementDocument->fetcher());

    // - "module":
    //   "Fetch a module script graph given url, settings, "script",
    //    cryptographic nonce, parser state, and
    //    module script credentials mode."
    // TODO(kouhei, hiroshige): Implement this.

    // "When the chosen algorithm asynchronously completes, set
    //  the script's script to the result. At that time, the script is ready."
    // When the script is ready, PendingScriptClient::pendingScriptFinished()
    // is used as the notification, and the action to take when
    // the script is ready is specified later, in
    // - ScriptLoader::prepareScript(), or
    // - HTMLParserScriptRunner,
    // depending on the conditions in Step 23 of "prepare a script".

    // 21.3. "Set the element's from an external file flag."
    m_isExternalScript = true;
  }

  if (!m_resource) {
    // 21.2. "If src is the empty string, queue a task to
    //        fire an event named error at the element, and abort these steps."
    // 21.5. "If the previous step failed, queue a task to
    //        fire an event named error at the element, and abort these steps."
    // TODO(hiroshige): Make this asynchronous.
    dispatchErrorEvent();
    return false;
  }

  // [Intervention]
  if (m_createdDuringDocumentWrite &&
      m_resource->resourceRequest().getCachePolicy() ==
          WebCachePolicy::ReturnCacheDataDontLoad) {
    m_documentWriteIntervention =
        DocumentWriteIntervention::DoNotFetchDocWrittenScript;
  }

  return true;
}

bool isHTMLScriptLoader(Element* element) {
  DCHECK(element);
  return isHTMLScriptElement(*element);
}

bool isSVGScriptLoader(Element* element) {
  DCHECK(element);
  return isSVGScriptElement(*element);
}

void ScriptLoader::logScriptMIMEType(LocalFrame* frame,
                                     ScriptResource* resource,
                                     const String& mimeType) {
  if (MIMETypeRegistry::isSupportedJavaScriptMIMEType(mimeType))
    return;
  bool isText = mimeType.startsWith("text/", TextCaseASCIIInsensitive);
  if (isText && isLegacySupportedJavaScriptLanguage(mimeType.substring(5)))
    return;
  bool isSameOrigin =
      m_element->document().getSecurityOrigin()->canRequest(resource->url());
  bool isApplication =
      !isText && mimeType.startsWith("application/", TextCaseASCIIInsensitive);

  UseCounter::Feature feature =
      isSameOrigin
          ? (isText ? UseCounter::SameOriginTextScript
                    : isApplication ? UseCounter::SameOriginApplicationScript
                                    : UseCounter::SameOriginOtherScript)
          : (isText ? UseCounter::CrossOriginTextScript
                    : isApplication ? UseCounter::CrossOriginApplicationScript
                                    : UseCounter::CrossOriginOtherScript);

  UseCounter::count(frame, feature);
}

bool ScriptLoader::executeScript(const ScriptSourceCode& sourceCode) {
  double scriptExecStartTime = monotonicallyIncreasingTime();
  bool result = doExecuteScript(sourceCode);

  // NOTE: we do not check m_willBeParserExecuted here, since
  // m_willBeParserExecuted is false for inline scripts, and we want to
  // include inline script execution time as part of parser blocked script
  // execution time.
  if (m_asyncExecType == ScriptRunner::None)
    DocumentParserTiming::from(m_element->document())
        .recordParserBlockedOnScriptExecutionDuration(
            monotonicallyIncreasingTime() - scriptExecStartTime,
            wasCreatedDuringDocumentWrite());
  return result;
}

// https://html.spec.whatwg.org/#execute-the-script-block
// with additional support for HTML imports.
// Note that Steps 2 and 8 must be handled by the caller of doExecuteScript(),
// i.e. load/error events are dispatched by the caller.
// Steps 3--7 are implemented here in doExecuteScript().
// TODO(hiroshige): Move event dispatching code to doExecuteScript().
bool ScriptLoader::doExecuteScript(const ScriptSourceCode& sourceCode) {
  DCHECK(m_alreadyStarted);

  if (sourceCode.isEmpty())
    return true;

  Document* elementDocument = &(m_element->document());
  Document* contextDocument = elementDocument->contextDocument();
  if (!contextDocument)
    return true;

  LocalFrame* frame = contextDocument->frame();
  if (!frame)
    return true;

  const ContentSecurityPolicy* csp = elementDocument->contentSecurityPolicy();
  bool shouldBypassMainWorldCSP =
      (frame->script().shouldBypassMainWorldCSP()) ||
      csp->allowScriptWithHash(sourceCode.source(),
                               ContentSecurityPolicy::InlineType::Block);

  AtomicString nonce =
      ContentSecurityPolicy::isNonceableElement(m_element.get())
          ? client()->nonce()
          : nullAtom;
  if (!m_isExternalScript &&
      (!shouldBypassMainWorldCSP &&
       !csp->allowInlineScript(m_element, elementDocument->url(), nonce,
                               m_startLineNumber, sourceCode.source()))) {
    return false;
  }

  if (m_isExternalScript) {
    ScriptResource* resource = sourceCode.resource();
    CHECK_EQ(resource, m_resource);
    CHECK(resource);
    if (!ScriptResource::mimeTypeAllowedByNosniff(resource->response())) {
      contextDocument->addConsoleMessage(ConsoleMessage::create(
          SecurityMessageSource, ErrorMessageLevel,
          "Refused to execute script from '" + resource->url().elidedString() +
              "' because its MIME type ('" + resource->httpContentType() +
              "') is not executable, and "
              "strict MIME type checking is "
              "enabled."));
      return false;
    }

    String mimeType = resource->httpContentType();
    if (mimeType.startsWith("image/") || mimeType == "text/csv" ||
        mimeType.startsWith("audio/") || mimeType.startsWith("video/")) {
      contextDocument->addConsoleMessage(ConsoleMessage::create(
          SecurityMessageSource, ErrorMessageLevel,
          "Refused to execute script from '" + resource->url().elidedString() +
              "' because its MIME type ('" + mimeType +
              "') is not executable."));
      if (mimeType.startsWith("image/"))
        UseCounter::count(frame, UseCounter::BlockedSniffingImageToScript);
      else if (mimeType.startsWith("audio/"))
        UseCounter::count(frame, UseCounter::BlockedSniffingAudioToScript);
      else if (mimeType.startsWith("video/"))
        UseCounter::count(frame, UseCounter::BlockedSniffingVideoToScript);
      else if (mimeType == "text/csv")
        UseCounter::count(frame, UseCounter::BlockedSniffingCSVToScript);
      return false;
    }

    logScriptMIMEType(frame, resource, mimeType);
  }

  AccessControlStatus accessControlStatus = NotSharableCrossOrigin;
  if (!m_isExternalScript) {
    accessControlStatus = SharableCrossOrigin;
  } else if (sourceCode.resource()) {
    if (sourceCode.resource()->response().wasFetchedViaServiceWorker()) {
      if (sourceCode.resource()->response().serviceWorkerResponseType() ==
          WebServiceWorkerResponseTypeOpaque)
        accessControlStatus = OpaqueResource;
      else
        accessControlStatus = SharableCrossOrigin;
    } else if (sourceCode.resource()->passesAccessControlCheck(
                   m_element->document().getSecurityOrigin())) {
      accessControlStatus = SharableCrossOrigin;
    }
  }

  const bool isImportedScript = contextDocument != elementDocument;

  // 3. "If the script is from an external file,
  //     or the script's type is module",
  //     then increment the ignore-destructive-writes counter of the
  //     script element's node document. Let neutralized doc be that Document."
  // TODO(hiroshige): Implement "module" case.
  IgnoreDestructiveWriteCountIncrementer ignoreDestructiveWriteCountIncrementer(
      m_isExternalScript || isImportedScript ? contextDocument : 0);

  // 4. "Let old script element be the value to which the script element's
  //     node document's currentScript object was most recently set."
  // This is implemented as push/popCurrentScript().

  // 5. "Switch on the script's type:"
  //    - "classic":
  //    1. "If the script element's root is not a shadow root,
  //        then set the script element's node document's currentScript
  //        attribute to the script element. Otherwise, set it to null."
  if (isHTMLScriptLoader(m_element) || isSVGScriptLoader(m_element))
    contextDocument->pushCurrentScript(m_element);

  //    2. "Run the classic script given by the script's script."
  // Note: This is where the script is compiled and actually executed.
  frame->script().executeScriptInMainWorld(sourceCode, accessControlStatus);

  //    - "module":
  // TODO(hiroshige): Implement this.

  // 6. "Set the script element's node document's currentScript attribute
  //     to old script element."
  if (isHTMLScriptLoader(m_element) || isSVGScriptLoader(m_element)) {
    DCHECK(contextDocument->currentScript() == m_element);
    contextDocument->popCurrentScript();
  }

  return true;

  // 7. "Decrement the ignore-destructive-writes counter of neutralized doc,
  //     if it was incremented in the earlier step."
  // Implemented as the scope out of IgnoreDestructiveWriteCountIncrementer.
}

void ScriptLoader::execute() {
  DCHECK(!m_willBeParserExecuted);
  DCHECK(m_asyncExecType != ScriptRunner::None);
  DCHECK(m_pendingScript->resource());
  bool errorOccurred = false;
  ScriptSourceCode source = m_pendingScript->getSource(KURL(), errorOccurred);
  detachPendingScript();
  if (errorOccurred) {
    dispatchErrorEvent();
  } else if (!m_resource->wasCanceled()) {
    if (executeScript(source))
      dispatchLoadEvent();
    else
      dispatchErrorEvent();
  }
  m_resource = nullptr;
}

void ScriptLoader::pendingScriptFinished(PendingScript* pendingScript) {
  DCHECK(!m_willBeParserExecuted);
  DCHECK_EQ(m_pendingScript, pendingScript);
  DCHECK_EQ(pendingScript->resource(), m_resource);

  // We do not need this script in the memory cache. The primary goals of
  // sending this fetch request are to let the third party server know
  // about the document.write scripts intervention and populate the http
  // cache for subsequent uses.
  if (m_documentWriteIntervention ==
      DocumentWriteIntervention::FetchDocWrittenScriptDeferIdle) {
    memoryCache()->remove(m_pendingScript->resource());
    m_pendingScript->stopWatchingForLoad();
    return;
  }

  DCHECK(m_asyncExecType != ScriptRunner::None);

  Document* contextDocument = m_element->document().contextDocument();
  if (!contextDocument) {
    detachPendingScript();
    return;
  }

  if (errorOccurred()) {
    contextDocument->scriptRunner()->notifyScriptLoadError(this,
                                                           m_asyncExecType);
    detachPendingScript();
    dispatchErrorEvent();
    return;
  }
  contextDocument->scriptRunner()->notifyScriptReady(this, m_asyncExecType);
  m_pendingScript->stopWatchingForLoad();
}

bool ScriptLoader::ignoresLoadRequest() const {
  return m_alreadyStarted || m_isExternalScript || m_parserInserted ||
         !element() || !element()->isConnected();
}

// Step 13 of https://html.spec.whatwg.org/#prepare-a-script
bool ScriptLoader::isScriptForEventSupported() const {
  // 1. "Let for be the value of the for attribute."
  String eventAttribute = client()->eventAttributeValue();
  // 2. "Let event be the value of the event attribute."
  String forAttribute = client()->forAttributeValue();

  // "If the script element has an event attribute and a for attribute, and
  //  the script's type is "classic", then run these substeps:"
  // TODO(hiroshige): Check the script's type.
  if (eventAttribute.isNull() || forAttribute.isNull())
    return true;

  // 3. "Strip leading and trailing ASCII whitespace from event and for."
  forAttribute = forAttribute.stripWhiteSpace();
  // 4. "If for is not an ASCII case-insensitive match for the string
  //     "window",
  //     then abort these steps at this point. The script is not executed."
  if (!equalIgnoringCase(forAttribute, "window"))
    return false;
  eventAttribute = eventAttribute.stripWhiteSpace();
  // 5. "If event is not an ASCII case-insensitive match for either the
  //     string "onload" or the string "onload()",
  //     then abort these steps at this point. The script is not executed.
  return equalIgnoringCase(eventAttribute, "onload") ||
         equalIgnoringCase(eventAttribute, "onload()");
}

String ScriptLoader::scriptContent() const {
  return m_element->textFromChildren();
}

ScriptLoaderClient* ScriptLoader::client() const {
  if (isHTMLScriptLoader(m_element))
    return toHTMLScriptElement(m_element);

  if (isSVGScriptLoader(m_element))
    return toSVGScriptElement(m_element);

  NOTREACHED();
  return 0;
}

ScriptLoader* toScriptLoaderIfPossible(Element* element) {
  if (isHTMLScriptLoader(element))
    return toHTMLScriptElement(element)->loader();

  if (isSVGScriptLoader(element))
    return toSVGScriptElement(element)->loader();

  return 0;
}

}  // namespace blink
