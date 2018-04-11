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

#include "third_party/blink/renderer/core/script/script_loader.h"

#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_parser_timing.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/imports/html_import.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetch_request.h"
#include "third_party/blink/renderer/core/loader/subresource_integrity_helper.h"
#include "third_party/blink/renderer/core/script/classic_pending_script.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/script/ignore_destructive_write_count_incrementer.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/script/module_pending_script.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/script/script_element_base.h"
#include "third_party/blink/renderer/core/script/script_runner.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/access_control_status.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

ScriptLoader::ScriptLoader(ScriptElementBase* element,
                           bool parser_inserted,
                           bool already_started,
                           bool created_during_document_write)
    : element_(element),
      start_line_number_(WTF::OrdinalNumber::BeforeFirst()),
      have_fired_load_(false),
      will_be_parser_executed_(false),
      will_execute_when_document_finished_parsing_(false),
      created_during_document_write_(created_during_document_write),
      async_exec_type_(ScriptRunner::kNone) {
  // https://html.spec.whatwg.org/multipage/scripting.html#already-started
  //
  // Spec: ... The cloning steps for script elements must set the "already
  // started" flag on the copy if it is set on the element being cloned. [spec
  // text]
  //
  // TODO(hiroshige): Cloning is implemented together with
  // {HTML,SVG}ScriptElement::cloneElementWithoutAttributesAndChildren().
  // Clean up these later.
  if (already_started)
    already_started_ = true;

  if (parser_inserted) {
    // https://html.spec.whatwg.org/multipage/scripting.html#parser-inserted
    //
    // Spec: ... It is set by the HTML parser and the XML parser on script
    // elements they insert ... [spec text]
    parser_inserted_ = true;

    // https://html.spec.whatwg.org/multipage/scripting.html#non-blocking
    //
    // Spec: ... It is unset by the HTML parser and the XML parser on script
    // elements they insert. ... [spec text]
    non_blocking_ = false;
  }

  if (parser_inserted &&
      element_->GetDocument().GetScriptableDocumentParser() &&
      !element_->GetDocument().IsInDocumentWrite()) {
    start_line_number_ =
        element_->GetDocument().GetScriptableDocumentParser()->LineNumber();
  }
}

ScriptLoader::~ScriptLoader() {}

void ScriptLoader::Trace(blink::Visitor* visitor) {
  visitor->Trace(element_);
  visitor->Trace(pending_script_);
  visitor->Trace(prepared_pending_script_);
  visitor->Trace(original_document_);
  visitor->Trace(resource_keep_alive_);
  PendingScriptClient::Trace(visitor);
}

void ScriptLoader::TraceWrappers(const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(pending_script_);
  visitor->TraceWrappers(prepared_pending_script_);
}

void ScriptLoader::DidNotifySubtreeInsertionsToDocument() {
  if (!parser_inserted_)
    PrepareScript();  // FIXME: Provide a real starting line number here.
}

void ScriptLoader::ChildrenChanged() {
  if (!parser_inserted_ && element_->IsConnected())
    PrepareScript();  // FIXME: Provide a real starting line number here.
}

void ScriptLoader::HandleSourceAttribute(const String& source_url) {
  if (IgnoresLoadRequest() || source_url.IsEmpty())
    return;

  PrepareScript();  // FIXME: Provide a real starting line number here.
}

void ScriptLoader::HandleAsyncAttribute() {
  // https://html.spec.whatwg.org/multipage/scripting.html#non-blocking
  //
  // Spec: ... In addition, whenever a script element whose "non-blocking" flag
  // is set has an async content attribute added, the element's "non-blocking"
  // flag must be unset. [spec text]
  non_blocking_ = false;
}

void ScriptLoader::DetachPendingScript() {
  if (!pending_script_)
    return;
  pending_script_->Dispose();
  pending_script_ = nullptr;
}

void ScriptLoader::DispatchErrorEvent() {
  element_->DispatchErrorEvent();
}

void ScriptLoader::DispatchLoadEvent() {
  element_->DispatchLoadEvent();
  SetHaveFiredLoadEvent(true);
}

namespace {

bool IsValidClassicScriptTypeAndLanguage(
    const String& type,
    const String& language,
    ScriptLoader::LegacyTypeSupport support_legacy_types) {
  // FIXME: isLegacySupportedJavaScriptLanguage() is not valid HTML5. It is used
  // here to maintain backwards compatibility with existing layout tests. The
  // specific violations are:
  // - Allowing type=javascript. type= should only support MIME types, such as
  //   text/javascript.
  // - Allowing a different set of languages for language= and type=. language=
  //   supports Javascript 1.1 and 1.4-1.6, but type= does not.
  if (type.IsEmpty()) {
    return language.IsEmpty() ||  // assume text/javascript.
           MIMETypeRegistry::IsSupportedJavaScriptMIMEType("text/" +
                                                           language) ||
           MIMETypeRegistry::IsLegacySupportedJavaScriptLanguage(language);
  } else if (MIMETypeRegistry::IsSupportedJavaScriptMIMEType(
                 type.StripWhiteSpace()) ||
             (support_legacy_types ==
                  ScriptLoader::kAllowLegacyTypeInTypeAttribute &&
              MIMETypeRegistry::IsLegacySupportedJavaScriptLanguage(type))) {
    return true;
  }

  return false;
}

}  // namespace

// https://html.spec.whatwg.org/multipage/scripting.html#prepare-a-script
bool ScriptLoader::IsValidScriptTypeAndLanguage(
    const String& type,
    const String& language,
    LegacyTypeSupport support_legacy_types,
    ScriptType& out_script_type) {
  if (IsValidClassicScriptTypeAndLanguage(type, language,
                                          support_legacy_types)) {
    // Step 7. ... If the script block's type string is a JavaScript MIME type
    // essence match, the script's type is "classic". ... [spec text]
    //
    // TODO(hiroshige): Annotate and/or cleanup this step.
    out_script_type = ScriptType::kClassic;
    return true;
  }

  if (type == "module") {
    // Step 7. ... If the script block's type string is an ASCII
    // case-insensitive match for the string "module", the script's type is
    // "module". ... [spec text]
    out_script_type = ScriptType::kModule;
    return true;
  }

  // Step 7. ... If neither of the above conditions are true, then return. No
  // script is executed. ... [spec text]
  return false;
}

bool ScriptLoader::BlockForNoModule(ScriptType script_type, bool nomodule) {
  return nomodule && script_type == ScriptType::kClassic;
}

// Step 16 of
// https://html.spec.whatwg.org/multipage/scripting.html#prepare-a-script
network::mojom::FetchCredentialsMode ScriptLoader::ModuleScriptCredentialsMode(
    CrossOriginAttributeValue cross_origin) {
  switch (cross_origin) {
    case kCrossOriginAttributeNotSet:
      return network::mojom::FetchCredentialsMode::kOmit;
    case kCrossOriginAttributeAnonymous:
      return network::mojom::FetchCredentialsMode::kSameOrigin;
    case kCrossOriginAttributeUseCredentials:
      return network::mojom::FetchCredentialsMode::kInclude;
  }
  NOTREACHED();
  return network::mojom::FetchCredentialsMode::kOmit;
}

bool ScriptLoader::IsScriptTypeSupported(LegacyTypeSupport support_legacy_types,
                                         ScriptType& out_script_type) const {
  return IsValidScriptTypeAndLanguage(element_->TypeAttributeValue(),
                                      element_->LanguageAttributeValue(),
                                      support_legacy_types, out_script_type);
}

// https://html.spec.whatwg.org/multipage/scripting.html#prepare-a-script
bool ScriptLoader::PrepareScript(const TextPosition& script_start_position,
                                 LegacyTypeSupport support_legacy_types) {
  // Step 1. If the script element is marked as having "already started", then
  // return. The script is not executed. [spec text]
  if (already_started_)
    return false;

  // Step 2. If the element has its "parser-inserted" flag set, then set
  // was-parser-inserted to true and unset the element's "parser-inserted" flag.
  // Otherwise, set was-parser-inserted to false. [spec text]
  bool was_parser_inserted;
  if (parser_inserted_) {
    was_parser_inserted = true;
    parser_inserted_ = false;
  } else {
    was_parser_inserted = false;
  }

  // Step 3. If was-parser-inserted is true and the element does not have an
  // async attribute, then set the element's "non-blocking" flag to true. [spec
  // text]
  if (was_parser_inserted && !element_->AsyncAttributeValue())
    non_blocking_ = true;

  // Step 4. Let source text be the element's child text content.
  //
  // Step 5. If the element has no src attribute, and source text is the empty
  // string, then return. The script is not executed.
  //
  // TODO(hiroshige): Update the behavior according to the spec.
  if (!element_->HasSourceAttribute() && !element_->HasChildren())
    return false;

  // Step 6. If the element is not connected, then return. The script is not
  // executed. [spec text]
  if (!element_->IsConnected())
    return false;

  // Step 7. ... Determine the script's type as follows: ... [spec text]
  //
  // |script_type_| is set here.
  if (!IsScriptTypeSupported(support_legacy_types, script_type_))
    return false;

  // Step 8. If was-parser-inserted is true, then flag the element as
  // "parser-inserted" again, and set the element's "non-blocking" flag to
  // false. [spec text]
  if (was_parser_inserted) {
    parser_inserted_ = true;
    non_blocking_ = false;
  }

  // Step 9. Set the element's "already started" flag. [spec text]
  already_started_ = true;

  // Step 10. If the element is flagged as "parser-inserted", but the element's
  // node document is not the Document of the parser that created the element,
  // then return. [spec text]
  //
  // FIXME: If script is parser inserted, verify it's still in the original
  // document.
  Document& element_document = element_->GetDocument();
  Document* context_document = element_document.ContextDocument();
  original_document_ = context_document;
  if (!element_document.ExecutingFrame())
    return false;
  if (!context_document || !context_document->ExecutingFrame())
    return false;

  // Step 11. If scripting is disabled for the script element, then return. The
  // script is not executed. [spec text]
  if (!context_document->CanExecuteScripts(kAboutToExecuteScript))
    return false;

  // Step 12. If the script element has a nomodule content attribute and the
  // script's type is "classic", then return. The script is not executed. [spec
  // text]
  if (BlockForNoModule(script_type_, element_->NomoduleAttributeValue()))
    return false;

  // 13.
  if (!IsScriptForEventSupported())
    return false;

  // 14. is handled below.

  // Step 16. Let classic script CORS setting be the current state of the
  // element's crossorigin content attribute. [spec text]
  CrossOriginAttributeValue cross_origin =
      GetCrossOriginAttributeValue(element_->CrossOriginAttributeValue());

  // Step 17. Let module script credentials mode be the module script
  // credentials mode for the element's crossorigin content attribute. [spec
  // text]
  network::mojom::FetchCredentialsMode credentials_mode =
      ModuleScriptCredentialsMode(cross_origin);

  // Step 18. Let cryptographic nonce be the element's [[CryptographicNonce]]
  // internal slot's value. [spec text]
  String nonce = element_->GetNonceForElement();

  // Step 19. If the script element has an integrity attribute, then let
  // integrity metadata be that attribute's value. Otherwise, let integrity
  // metadata be the empty string. [spec text]
  String integrity_attr = element_->IntegrityAttributeValue();
  IntegrityMetadataSet integrity_metadata;
  if (!integrity_attr.IsEmpty()) {
    SubresourceIntegrity::IntegrityFeatures integrity_features =
        SubresourceIntegrityHelper::GetFeatures(&element_document);
    SubresourceIntegrity::ReportInfo report_info;
    SubresourceIntegrity::ParseIntegrityAttribute(
        integrity_attr, integrity_features, integrity_metadata, &report_info);
    SubresourceIntegrityHelper::DoReport(element_document, report_info);
  }

  // Step 20. Let parser metadata be "parser-inserted" if the script element has
  // been flagged as "parser-inserted", and "not-parser-inserted" otherwise.
  // [spec text]
  ParserDisposition parser_state =
      IsParserInserted() ? kParserInserted : kNotParserInserted;

  if (GetScriptType() == ScriptType::kModule)
    UseCounter::Count(*context_document, WebFeature::kPrepareModuleScript);

  DCHECK(!prepared_pending_script_);

  // Reset line numbering for nested writes.
  TextPosition position = element_document.IsInDocumentWrite()
                              ? TextPosition()
                              : script_start_position;

  // Step 21. Let options be a script fetch options whose cryptographic nonce is
  // cryptographic nonce, integrity metadata is integrity metadata, parser
  // metadata is parser metadata, credentials mode is module script credentials
  // mode, and referrer policy is the empty string. [spec text]
  ScriptFetchOptions options(nonce, integrity_metadata, integrity_attr,
                             parser_state, credentials_mode);

  // Step 22. Let settings object be the element's node document's Window
  // object's environment settings object. [spec text]
  //
  // Note: We use |element_document| as "settings object" in the steps below.

  // Step 23. If the element has a src content attribute, then: [spec text]
  if (element_->HasSourceAttribute()) {
    // Step 23.1. Let src be the value of the element's src attribute. [spec
    // text]
    String src =
        StripLeadingAndTrailingHTMLSpaces(element_->SourceAttributeValue());

    // Step 23.2. If src is the empty string, queue a task to fire an event
    // named error at the element, and return. [spec text]
    if (src.IsEmpty()) {
      // TODO(hiroshige): Make this asynchronous. Currently we fire the error
      // event synchronously to keep the existing behavior.
      DispatchErrorEvent();
      return false;
    }

    // Step 23.3. Set the element's from an external file flag. [spec text]
    is_external_script_ = true;

    // Step 23.4. Parse src relative to the element's node document. [spec text]
    KURL url = element_document.CompleteURL(src);

    // Step 23.5. If the previous step failed, queue a task to fire an event
    // named error at the element, and return. Otherwise, let url be the
    // resulting URL record. [spec text]
    if (!url.IsValid()) {
      // TODO(hiroshige): Make this asynchronous. Currently we fire the error
      // event synchronously to keep the existing behavior.
      DispatchErrorEvent();
      return false;
    }

    // Step 23.6. Switch on the script's type: [spec text]
    if (GetScriptType() == ScriptType::kClassic) {
      // - "classic":

      // Step 15. If the script element has a charset attribute, then let
      // encoding be the result of getting an encoding from the value of the
      // charset attribute. If the script element does not have a charset
      // attribute, or if getting an encoding failed, let encoding be the same
      // as the encoding of the script element's node document. [spec text]
      //
      // TODO(hiroshige): Should we handle failure in getting an encoding?
      WTF::TextEncoding encoding;
      if (!element_->CharsetAttributeValue().IsEmpty())
        encoding = WTF::TextEncoding(element_->CharsetAttributeValue());
      else
        encoding = element_document.Encoding();

      // Step 23.6.A. "classic"
      //
      // Fetch a classic script given url, settings object, options, classic
      // script CORS setting, and encoding. [spec text]
      FetchClassicScript(url, element_document, options, encoding);
    } else {
      // - "module":

      // Step 15 is skipped because they are not used in module
      // scripts.

      // STEP 23.6.B. "module"
      //
      // Fetch a module script graph given url, settings object, "script", and
      // options. [spec text]
      Modulator* modulator = Modulator::From(
          ToScriptStateForMainWorld(context_document->GetFrame()));
      FetchModuleScriptTree(url, modulator, options);
    }
    // Step 23.6. When the chosen algorithm asynchronously completes, set the
    // script's script to the result. At that time, the script is ready. [spec
    // text]
    //
    // When the script is ready, PendingScriptClient::pendingScriptFinished()
    // is used as the notification, and the action to take when
    // the script is ready is specified later, in
    // - ScriptLoader::PrepareScript(), or
    // - HTMLParserScriptRunner,
    // depending on the conditions in Step 25 of "prepare a script".
  }

  // Step 24. If the element does not have a src content attribute, run these
  // substeps: [spec text]
  if (!element_->HasSourceAttribute()) {
    // Step 23.1. Let src be the value of the element's src attribute. [spec
    // text]
    //
    // This step is done later as ScriptElementBase::TextFromChildren():
    // - in ScriptLoader::PrepareScript() (Step 25, 6th Clause),
    // - in HTMLParserScriptRunner::ProcessScriptElementInternal()
    //   (Duplicated code of Step 25, 6th Clause),
    // - in XMLDocumentParser::EndElementNs() (Step 25, 5th Clause), or
    // - in PendingScript::GetSource() (Indirectly used via
    //   HTMLParserScriptRunner::ProcessScriptElementInternal(),
    //   Step 25, 5th Clause).

    // Step 24.1. Let base URL be the script element's node document's document
    // base URL. [spec text]
    KURL base_url = element_document.BaseURL();

    // Step 24.2. Switch on the script's type: [spec text]
    switch (GetScriptType()) {
      // Step 24.2.A. "classic" [spec text]
      case ScriptType::kClassic: {
        // Step 24.2.A.1. Let script be the result of creating a classic script
        // using source text, settings object, base URL, and options. [spec
        // text]

        ScriptSourceLocationType script_location_type =
            ScriptSourceLocationType::kInline;
        if (!parser_inserted_) {
          script_location_type =
              ScriptSourceLocationType::kInlineInsideGeneratedElement;
        } else if (element_->GetDocument().IsInDocumentWrite()) {
          script_location_type =
              ScriptSourceLocationType::kInlineInsideDocumentWrite;
        }

        prepared_pending_script_ = ClassicPendingScript::CreateInline(
            element_, position, script_location_type, options);

        // Step 24.2.A.2. Set the script's script to script. [spec text]
        //
        // Step 24.2.A.3. The script is ready. [spec text]
        //
        // Implemented by ClassicPendingScript.
        break;
      }

      // Step 24.2.B. "module" [spec text]
      case ScriptType::kModule: {
        // Step 24.2.B.1. Let script be the result of creating a module script
        // using source text, settings object, base URL, and options. [spec
        // text]
        const KURL& source_url = element_document.Url();
        Modulator* modulator = Modulator::From(
            ToScriptStateForMainWorld(context_document->GetFrame()));
        ModuleScript* module_script = ModuleScript::Create(
            element_->TextFromChildren(), modulator, source_url, base_url,
            options, kSharableCrossOrigin, position);

        // Step 24.2.B.2. If this returns null, set the script's script to null
        // and return; the script is ready. [spec text]
        if (!module_script)
          return false;

        // Step 24.2.B.3. Fetch the descendants of and instantiate script, given
        // the destination "script". When this asynchronously completes, set the
        // script's script to the result. At that time, the script is ready.
        // [spec text]
        auto* module_tree_client = ModulePendingScriptTreeClient::Create();
        modulator->FetchDescendantsForInlineScript(module_script,
                                                   module_tree_client);
        prepared_pending_script_ = ModulePendingScript::Create(
            element_, module_tree_client, is_external_script_);
        break;
      }
    }
  }

  DCHECK(prepared_pending_script_);

  // Step 25. Then, follow the first of the following options that describes the
  // situation: [spec text]

  // Three flags are used to instruct the caller of prepareScript() to execute
  // a part of Step 24, when |m_willBeParserExecuted| is true:
  // - |m_willBeParserExecuted|
  // - |m_willExecuteWhenDocumentFinishedParsing|
  // - |m_readyToBeParserExecuted|
  // TODO(hiroshige): Clean up the dependency.

  // Step 25.A. If the script's type is "classic", and the element has a src
  // attribute, and the element has a defer attribute, and the element has been
  // flagged as "parser-inserted", and the element does not have an async
  // attribute
  //
  // If the script's type is "module", and the element has been flagged as
  // "parser-inserted", and the element does not have an async attribute ...
  // [spec text]
  if ((GetScriptType() == ScriptType::kClassic &&
       element_->HasSourceAttribute() && element_->DeferAttributeValue() &&
       parser_inserted_ && !element_->AsyncAttributeValue()) ||
      (GetScriptType() == ScriptType::kModule && parser_inserted_ &&
       !element_->AsyncAttributeValue())) {
    // This clause is implemented by the caller-side of prepareScript():
    // - HTMLParserScriptRunner::requestDeferredScript(), and
    // - TODO(hiroshige): Investigate XMLDocumentParser::endElementNs()
    will_execute_when_document_finished_parsing_ = true;
    will_be_parser_executed_ = true;

    return true;
  }

  // Step 25.B. If the script's type is "classic", and the element has a src
  // attribute, and the element has been flagged as "parser-inserted", and the
  // element does not have an async attribute ... [spec text]
  if (GetScriptType() == ScriptType::kClassic &&
      element_->HasSourceAttribute() && parser_inserted_ &&
      !element_->AsyncAttributeValue()) {
    // This clause is implemented by the caller-side of prepareScript():
    // - HTMLParserScriptRunner::requestParsingBlockingScript()
    // - TODO(hiroshige): Investigate XMLDocumentParser::endElementNs()
    will_be_parser_executed_ = true;

    return true;
  }

  // Step 25.C. If the script's type is "classic", and the element has a src
  // attribute, and the element does not have an async attribute, and the
  // element does not have the "non-blocking" flag set
  //
  // If the script's type is "module", and the element does not have an async
  // attribute, and the element does not have the "non-blocking" flag set ...
  // [spec text]
  if ((GetScriptType() == ScriptType::kClassic &&
       element_->HasSourceAttribute() && !element_->AsyncAttributeValue() &&
       !non_blocking_) ||
      (GetScriptType() == ScriptType::kModule &&
       !element_->AsyncAttributeValue() && !non_blocking_)) {
    // Step 25.C. ... Add the element to the end of the list of scripts that
    // will execute in order as soon as possible associated with the node
    // document of the script element at the time the prepare a script algorithm
    // started. ... [spec text]
    pending_script_ = TakePendingScript();
    async_exec_type_ = ScriptRunner::kInOrder;
    // TODO(hiroshige): Here |contextDocument| is used as "node document"
    // while Step 14 uses |elementDocument| as "node document". Fix this.
    context_document->GetScriptRunner()->QueueScriptForExecution(
        this, async_exec_type_);
    // Note that watchForLoad can immediately call pendingScriptFinished.
    pending_script_->WatchForLoad(this);
    // The part "When the script is ready..." is implemented in
    // ScriptRunner::notifyScriptReady().
    // TODO(hiroshige): Annotate it.

    return true;
  }

  // Step 25.D. If the script's type is "classic", and the element has a src
  // attribute
  //
  // If the script's type is "module" ... [spec text]
  if ((GetScriptType() == ScriptType::kClassic &&
       element_->HasSourceAttribute()) ||
      GetScriptType() == ScriptType::kModule) {
    // Step 25.D. ... The element must be added to the set of scripts that will
    // execute as soon as possible of the node document of the script element at
    // the time the prepare a script algorithm started. When the script is
    // ready, execute the script block and then remove the element from the set
    // of scripts that will execute as soon as possible. [spec text]
    pending_script_ = TakePendingScript();
    async_exec_type_ = ScriptRunner::kAsync;
    // TODO(hiroshige): Here |contextDocument| is used as "node document"
    // while Step 14 uses |elementDocument| as "node document". Fix this.
    context_document->GetScriptRunner()->QueueScriptForExecution(
        this, async_exec_type_);
    // Note that watchForLoad can immediately call pendingScriptFinished.
    pending_script_->WatchForLoad(this);
    // The part "When the script is ready..." is implemented in
    // ScriptRunner::notifyScriptReady().
    // TODO(hiroshige): Annotate it.

    return true;
  }

  // The following clauses are executed only if the script's type is "classic"
  // and the element doesn't have a src attribute.
  DCHECK_EQ(GetScriptType(), ScriptType::kClassic);
  DCHECK(!is_external_script_);

  // Step 25.E. If the element does not have a src attribute, and the element
  // has been flagged as "parser-inserted", and either the parser that created
  // the script is an XML parser or it's an HTML parser whose script nesting
  // level is not greater than one, and the Document of the HTML parser or XML
  // parser that created the script element has a style sheet that is blocking
  // scripts ... [spec text]
  //
  // The last part "... has a style sheet that is blocking scripts"
  // is implemented in Document::isScriptExecutionReady().
  // Part of the condition check is done in
  // HTMLParserScriptRunner::processScriptElementInternal().
  // TODO(hiroshige): Clean up the split condition check.
  if (!element_->HasSourceAttribute() && parser_inserted_ &&
      !element_document.IsScriptExecutionReady()) {
    // The former part of this clause is
    // implemented by the caller-side of prepareScript():
    // - HTMLParserScriptRunner::requestParsingBlockingScript()
    // - TODO(hiroshige): Investigate XMLDocumentParser::endElementNs()
    will_be_parser_executed_ = true;
    // Step 25.E. ... Set the element's "ready to be parser-executed" flag. ...
    // [spec text]
    ready_to_be_parser_executed_ = true;

    return true;
  }

  // Step 25.F. Otherwise
  //
  // Immediately execute the script block, even if other scripts are already
  // executing. [spec text]
  //
  // Note: this block is also duplicated in
  // HTMLParserScriptRunner::processScriptElementInternal().
  // TODO(hiroshige): Merge the duplicated code.
  KURL script_url = (!element_document.IsInDocumentWrite() && parser_inserted_)
                        ? element_document.Url()
                        : KURL();
  ExecuteScriptBlock(TakePendingScript(), script_url);
  return true;
}

// https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-classic-script
void ScriptLoader::FetchClassicScript(const KURL& url,
                                      Document& element_document,
                                      const ScriptFetchOptions& options,
                                      const WTF::TextEncoding& encoding) {
  FetchParameters::DeferOption defer = FetchParameters::kNoDefer;
  if (!parser_inserted_ || element_->AsyncAttributeValue() ||
      element_->DeferAttributeValue())
    defer = FetchParameters::kLazyLoad;

  ClassicPendingScript* pending_script = ClassicPendingScript::Fetch(
      url, element_document, options, encoding, element_, defer);
  prepared_pending_script_ = pending_script;
  resource_keep_alive_ = pending_script->GetResource();
}

void ScriptLoader::FetchModuleScriptTree(const KURL& url,
                                         Modulator* modulator,
                                         const ScriptFetchOptions& options) {
  // https://html.spec.whatwg.org/multipage/scripting.html#prepare-a-script
  //
  // Step 23.6.B. "module"
  //
  // Fetch a module script graph given url, settings object, "script", and
  // options. [spec text]
  auto* module_tree_client = ModulePendingScriptTreeClient::Create();
  modulator->FetchTree(
      ModuleScriptFetchRequest(url, modulator->GetReferrerPolicy(), options),
      module_tree_client);
  prepared_pending_script_ = ModulePendingScript::Create(
      element_, module_tree_client, is_external_script_);
}

PendingScript* ScriptLoader::TakePendingScript() {
  CHECK(prepared_pending_script_);
  PendingScript* pending_script = prepared_pending_script_;
  prepared_pending_script_ = nullptr;
  return pending_script;
}

void ScriptLoader::Execute() {
  DCHECK(!will_be_parser_executed_);
  DCHECK(async_exec_type_ != ScriptRunner::kNone);
  DCHECK(pending_script_->IsExternalOrModule());
  PendingScript* pending_script = pending_script_;
  pending_script_ = nullptr;
  ExecuteScriptBlock(pending_script, NullURL());
  resource_keep_alive_ = nullptr;
}

// https://html.spec.whatwg.org/multipage/scripting.html#execute-the-script-block
void ScriptLoader::ExecuteScriptBlock(PendingScript* pending_script,
                                      const KURL& document_url) {
  DCHECK(pending_script);
  DCHECK_EQ(pending_script->IsExternal(), is_external_script_);
  DCHECK(already_started_);

  Document* element_document = &(element_->GetDocument());
  Document* context_document = element_document->ContextDocument();
  if (!context_document) {
    pending_script->Dispose();
    return;
  }

  LocalFrame* frame = context_document->GetFrame();
  if (!frame) {
    pending_script->Dispose();
    return;
  }

  // Do not execute module scripts if they are moved between documents.
  // TODO(hiroshige): Also do not execute classic scripts. crbug.com/721914
  if (original_document_ != context_document &&
      GetScriptType() == ScriptType::kModule) {
    pending_script->Dispose();
    return;
  }

  bool error_occurred = false;
  Script* script = pending_script->GetSource(document_url, error_occurred);

  // Consider as if "the script's script is null" retrospectively,
  // if the MIME check fails, which is considered as load failure.
  if (!pending_script->CheckMIMETypeBeforeRunScript(context_document))
    error_occurred = true;

  if (!error_occurred && !is_external_script_) {
    bool should_bypass_main_world_csp =
        frame->GetScriptController().ShouldBypassMainWorldCSP();

    AtomicString nonce = element_->GetNonceForElement();
    if (!should_bypass_main_world_csp &&
        !element_->AllowInlineScriptForCSP(
            nonce, start_line_number_, script->InlineSourceTextForCSP(),
            ContentSecurityPolicy::InlineType::kBlock)) {
      // Consider as if "the script's script is null" retrospectively,
      // if the CSP check fails, which is considered as load failure.
      error_occurred = true;
    }
  }

  const bool was_canceled = pending_script->WasCanceled();
  const bool is_external = pending_script->IsExternal();
  const double parser_blocking_load_start_time =
      pending_script->ParserBlockingLoadStartTime();
  pending_script->Dispose();

  // Step 2. If the script's script is null, fire an event named error at the
  // element, and return. [spec text]
  if (error_occurred) {
    DispatchErrorEvent();
    return;
  }

  if (parser_blocking_load_start_time > 0.0) {
    DocumentParserTiming::From(element_->GetDocument())
        .RecordParserBlockedOnScriptLoadDuration(
            CurrentTimeTicksInSeconds() - parser_blocking_load_start_time,
            WasCreatedDuringDocumentWrite());
  }

  if (was_canceled)
    return;

  double script_exec_start_time = CurrentTimeTicksInSeconds();

  {
    CHECK_EQ(script->GetScriptType(), GetScriptType());

    if (element_->ElementHasDuplicateAttributes()) {
      UseCounter::Count(element_->GetDocument(),
                        WebFeature::kDuplicatedAttributeForExecutedScript);
    }

    const bool is_imported_script = context_document != element_document;

    // Step 3. If the script is from an external file, or the script's type is
    // "module", ... [spec text]
    const bool needs_increment =
        is_external_script_ || script->GetScriptType() == ScriptType::kModule ||
        is_imported_script;
    // Step 3. ... then increment the ignore-destructive-writes counter of the
    // script element's node document. Let neutralized doc be that Document.
    // [spec text]
    IgnoreDestructiveWriteCountIncrementer incrementer(
        needs_increment ? context_document : nullptr);

    // Step 4. Let old script element be the value to which the script element's
    // node document's currentScript object was most recently set. [spec text]
    //
    // This is implemented as push/popCurrentScript().

    // Step 5. Switch on the script's type: [spec text]
    //
    // Step 5.A. "classic" [spec text]
    //
    // Step 5.A.1. If the script element's root is not a shadow root, then set
    // the script element's node document's currentScript attribute to the
    // script element. Otherwise, set it to null. [spec text]
    //
    // Step 5.B. "module" [spec text]
    //
    // Step 5.B.1. Set the script element's node document's currentScript
    // attribute to null. [spec text]
    ScriptElementBase* current_script = nullptr;
    if (script->GetScriptType() == ScriptType::kClassic)
      current_script = element_;
    context_document->PushCurrentScript(current_script);

    // Step 5.A. "classic" [spec text]
    //
    // Step 5.A.2. Run the classic script given by the script's script. [spec
    // text]
    //
    // Note: This is where the script is compiled and actually executed.
    //
    // Step 5.B. "module" [spec text]
    //
    // Step 5.B.2. Run the module script given by the script's script. [spec
    // text]
    script->RunScript(frame, element_->GetDocument().GetSecurityOrigin());

    // Step 6. Set the script element's node document's currentScript attribute
    // to old script element. [spec text]
    context_document->PopCurrentScript(current_script);

    // Step 7. Decrement the ignore-destructive-writes counter of neutralized
    // doc, if it was incremented in the earlier step. [spec text]
    //
    // Implemented as the scope out of IgnoreDestructiveWriteCountIncrementer.
  }

  // NOTE: we do not check m_willBeParserExecuted here, since
  // m_willBeParserExecuted is false for inline scripts, and we want to
  // include inline script execution time as part of parser blocked script
  // execution time.
  if (async_exec_type_ == ScriptRunner::kNone) {
    DocumentParserTiming::From(element_->GetDocument())
        .RecordParserBlockedOnScriptExecutionDuration(
            CurrentTimeTicksInSeconds() - script_exec_start_time,
            WasCreatedDuringDocumentWrite());
  }

  // Step 8. If the script is from an external file, then fire an event named
  // load at the script element. [spec text]
  if (is_external)
    DispatchLoadEvent();
}

void ScriptLoader::PendingScriptFinished(PendingScript* pending_script) {
  DCHECK(!will_be_parser_executed_);
  DCHECK_EQ(pending_script_, pending_script);
  DCHECK_EQ(pending_script_->GetScriptType(), GetScriptType());
  DCHECK(async_exec_type_ != ScriptRunner::kNone);

  Document* context_document = element_->GetDocument().ContextDocument();
  if (!context_document) {
    DetachPendingScript();
    return;
  }

  context_document->GetScriptRunner()->NotifyScriptReady(this,
                                                         async_exec_type_);
  pending_script_->StopWatchingForLoad();
}

bool ScriptLoader::IgnoresLoadRequest() const {
  return already_started_ || is_external_script_ || parser_inserted_ ||
         !element_->IsConnected();
}

// https://html.spec.whatwg.org/multipage/scripting.html#prepare-a-script
bool ScriptLoader::IsScriptForEventSupported() const {
  // Step 14.1. Let for be the value of the for attribute. [spec text]
  String event_attribute = element_->EventAttributeValue();
  // Step 14.2. Let event be the value of the event attribute. [spec text]
  String for_attribute = element_->ForAttributeValue();

  // Step 14. If the script element has an event attribute and a for attribute,
  // and the script's type is "classic", then: [spec text]
  if (GetScriptType() != ScriptType::kClassic || event_attribute.IsNull() ||
      for_attribute.IsNull())
    return true;

  // Step 14.3. Strip leading and trailing ASCII whitespace from event and for.
  // [spec text]
  for_attribute = for_attribute.StripWhiteSpace();
  // Step 14.4. If for is not an ASCII case-insensitive match for the string
  // "window", then return. The script is not executed. [spec text]
  if (!DeprecatedEqualIgnoringCase(for_attribute, "window"))
    return false;
  event_attribute = event_attribute.StripWhiteSpace();
  // Step 14.5. If event is not an ASCII case-insensitive match for either the
  // string "onload" or the string "onload()", then return. The script is not
  // executed. [spec text]
  return DeprecatedEqualIgnoringCase(event_attribute, "onload") ||
         DeprecatedEqualIgnoringCase(event_attribute, "onload()");
}

PendingScript* ScriptLoader::GetPendingScriptIfScriptIsAsync() {
  if (pending_script_ && async_exec_type_ == ScriptRunner::kAsync)
    return pending_script_;
  return nullptr;
}

}  // namespace blink
