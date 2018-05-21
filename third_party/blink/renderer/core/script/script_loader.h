/*
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
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_SCRIPT_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_SCRIPT_LOADER_H_

#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_location_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/cross_origin_attribute.h"
#include "third_party/blink/renderer/core/script/pending_script.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/script/script_runner.h"
#include "third_party/blink/renderer/core/script/script_scheduling_type.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_fetch_options.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ScriptElementBase;
class Script;

class ScriptResource;

class Modulator;

class CORE_EXPORT ScriptLoader : public GarbageCollectedFinalized<ScriptLoader>,
                                 public PendingScriptClient,
                                 public TraceWrapperBase {
  USING_GARBAGE_COLLECTED_MIXIN(ScriptLoader);

 public:
  static ScriptLoader* Create(ScriptElementBase* element,
                              bool created_by_parser,
                              bool is_evaluated,
                              bool created_during_document_write) {
    return new ScriptLoader(element, created_by_parser, is_evaluated,
                            created_during_document_write);
  }

  ~ScriptLoader() override;
  void Trace(blink::Visitor*) override;
  void TraceWrappers(ScriptWrappableVisitor*) const override;
  const char* NameInHeapSnapshot() const override { return "ScriptLoader"; }

  enum LegacyTypeSupport {
    kDisallowLegacyTypeInTypeAttribute,
    kAllowLegacyTypeInTypeAttribute
  };
  static bool IsValidScriptTypeAndLanguage(
      const String& type_attribute_value,
      const String& language_attribute_value,
      LegacyTypeSupport support_legacy_types,
      ScriptType& out_script_type);

  static bool BlockForNoModule(ScriptType, bool nomodule);

  static network::mojom::FetchCredentialsMode ModuleScriptCredentialsMode(
      CrossOriginAttributeValue);

  // https://html.spec.whatwg.org/multipage/scripting.html#prepare-a-script
  bool PrepareScript(const TextPosition& script_start_position =
                         TextPosition::MinimumPosition(),
                     LegacyTypeSupport = kDisallowLegacyTypeInTypeAttribute);

  // https://html.spec.whatwg.org/multipage/scripting.html#execute-the-script-block
  // The single entry point of script execution.
  // PendingScript::Dispose() is called in ExecuteScriptBlock().
  void ExecuteScriptBlock(PendingScript*, const KURL&);

  // Gets a PendingScript for external script whose fetch is started in
  // FetchClassicScript()/FetchModuleScriptTree().
  // This should be called only once.
  PendingScript* TakePendingScript(ScriptSchedulingType);

  // The entry point only for ScriptRunner that wraps ExecuteScriptBlock().
  virtual void Execute();

  bool WillBeParserExecuted() const { return will_be_parser_executed_; }
  bool ReadyToBeParserExecuted() const { return ready_to_be_parser_executed_; }
  bool WillExecuteWhenDocumentFinishedParsing() const {
    return will_execute_when_document_finished_parsing_;
  }
  bool IsParserInserted() const { return parser_inserted_; }
  bool AlreadyStarted() const { return already_started_; }
  bool IsNonBlocking() const { return non_blocking_; }
  ScriptType GetScriptType() const { return script_type_; }

  // Helper functions used by our parent classes.
  void DidNotifySubtreeInsertionsToDocument();
  void ChildrenChanged();
  void HandleSourceAttribute(const String& source_url);
  void HandleAsyncAttribute();

  void SetFetchDocWrittenScriptDeferIdle();

  // Only makes sense for scripts controlled by ScriptRunner.
  // To support script streaming, the ScriptRunner
  // may need to access the PendingScript. This breaks the intended layering, so
  // please use with care. (Method is virtual to support testing.)
  virtual PendingScript* GetPendingScriptIfControlledByScriptRunner();

 protected:
  ScriptLoader(ScriptElementBase*,
               bool created_by_parser,
               bool is_evaluated,
               bool created_during_document_write);

 private:
  bool IgnoresLoadRequest() const;
  bool IsScriptForEventSupported() const;

  // FetchClassicScript corresponds to Step 21.6 of
  // https://html.spec.whatwg.org/multipage/scripting.html#prepare-a-script
  // and must NOT be called from outside of PendingScript().
  //
  // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-classic-script
  void FetchClassicScript(const KURL&,
                          Document&,
                          const ScriptFetchOptions&,
                          const WTF::TextEncoding&);
  // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-module-script-tree
  void FetchModuleScriptTree(const KURL&,
                             Modulator*,
                             const ScriptFetchOptions&);

  void DispatchLoadEvent();
  void DispatchErrorEvent();

  // Clears the connection to the PendingScript.
  void DetachPendingScript();

  // PendingScriptClient
  void PendingScriptFinished(PendingScript*) override;

  bool WasCreatedDuringDocumentWrite() {
    return created_during_document_write_;
  }

  Member<ScriptElementBase> element_;
  WTF::OrdinalNumber start_line_number_;

  // https://html.spec.whatwg.org/multipage/scripting.html#script-processing-model
  // "A script element has several associated pieces of state.":

  // <spec
  // href="https://html.spec.whatwg.org/multipage/scripting.html#already-started">
  // ... Initially, script elements must have this flag unset ...</spec>
  bool already_started_ = false;

  // <spec
  // href="https://html.spec.whatwg.org/multipage/scripting.html#parser-inserted">
  // ... Initially, script elements must have this flag unset. ...</spec>
  bool parser_inserted_ = false;

  // <spec
  // href="https://html.spec.whatwg.org/multipage/scripting.html#non-blocking">
  // ... Initially, script elements must have this flag set. ...</spec>
  bool non_blocking_ = true;

  // <spec
  // href="https://html.spec.whatwg.org/multipage/scripting.html#ready-to-be-parser-executed">
  // ... Initially, script elements must have this flag unset ...</spec>
  bool ready_to_be_parser_executed_ = false;

  // <spec
  // href="https://html.spec.whatwg.org/multipage/scripting.html#concept-script-type">
  // ... It is determined when the script is prepared, ...</spec>
  ScriptType script_type_ = ScriptType::kClassic;

  // <spec
  // href="https://html.spec.whatwg.org/multipage/scripting.html#concept-script-external">
  // ... It is determined when the script is prepared, ...</spec>
  bool is_external_script_ = false;

  // Same as "The parser will handle executing the script."
  bool will_be_parser_executed_;

  bool will_execute_when_document_finished_parsing_;

  const bool created_during_document_write_;

  // A PendingScript is first created in PrepareScript() and stored in
  // |prepared_pending_script_|.
  // Later, TakePendingScript() is called, and its caller holds a reference
  // to the PendingScript instead and |prepared_pending_script_| is cleared.
  TraceWrapperMember<PendingScript> prepared_pending_script_;

  // If the script is controlled by ScriptRunner, then
  // ScriptLoader::pending_script_ holds a reference to the PendingScript and
  // ScriptLoader is its client.
  // Otherwise, HTMLParserScriptRunner or XMLParserScriptRunner holds the
  // reference and |pending_script_| here is null.
  TraceWrapperMember<PendingScript> pending_script_;

  // This is used only to keep the ScriptResource of a classic script alive
  // and thus to keep it on MemoryCache, even after script execution, as long
  // as ScriptLoader is alive. crbug.com/778799
  Member<Resource> resource_keep_alive_;

  // The context document at the time when PrepareScript() is executed.
  // This is only used to check whether the script element is moved between
  // documents and thus doesn't retain a strong reference.
  WeakMember<Document> original_document_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_SCRIPT_LOADER_H_
