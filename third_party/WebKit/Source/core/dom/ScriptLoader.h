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

#ifndef ScriptLoader_h
#define ScriptLoader_h

#include "core/CoreExport.h"
#include "core/dom/PendingScript.h"
#include "core/dom/Script.h"
#include "core/dom/ScriptRunner.h"
#include "core/html/CrossOriginAttribute.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/loader/fetch/IntegrityMetadata.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/ScriptFetchOptions.h"
#include "platform/wtf/text/TextEncoding.h"
#include "platform/wtf/text/TextPosition.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebURLRequest.h"

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
  virtual void Trace(blink::Visitor*);
  void TraceWrappers(const ScriptWrappableVisitor*) const;

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

  // https://html.spec.whatwg.org/#prepare-a-script
  bool PrepareScript(const TextPosition& script_start_position =
                         TextPosition::MinimumPosition(),
                     LegacyTypeSupport = kDisallowLegacyTypeInTypeAttribute);

  // https://html.spec.whatwg.org/#execute-the-script-block
  // The single entry point of script execution.
  // PendingScript::Dispose() is called in ExecuteScriptBlock().
  void ExecuteScriptBlock(PendingScript*, const KURL&);

  // Gets a PendingScript for external script whose fetch is started in
  // FetchClassicScript()/FetchModuleScriptTree().
  // This should be called only once.
  PendingScript* TakePendingScript();

  // The entry point only for ScriptRunner that wraps ExecuteScriptBlock().
  virtual void Execute();

  bool IsScriptTypeSupported(LegacyTypeSupport,
                             ScriptType& out_script_type) const;

  bool HaveFiredLoadEvent() const { return have_fired_load_; }
  bool WillBeParserExecuted() const { return will_be_parser_executed_; }
  bool ReadyToBeParserExecuted() const { return ready_to_be_parser_executed_; }
  bool WillExecuteWhenDocumentFinishedParsing() const {
    return will_execute_when_document_finished_parsing_;
  }
  void SetHaveFiredLoadEvent(bool have_fired_load) {
    have_fired_load_ = have_fired_load;
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

  virtual bool IsReady() const {
    return pending_script_ && pending_script_->IsReady();
  }

  void SetFetchDocWrittenScriptDeferIdle();

  // To support script streaming, the ScriptRunner may need to access the
  // PendingScript. This breaks the intended layering, so please use with
  // care. (Method is virtual to support testing.)
  virtual PendingScript* GetPendingScriptIfScriptIsAsync();

 protected:
  ScriptLoader(ScriptElementBase*,
               bool created_by_parser,
               bool is_evaluated,
               bool created_during_document_write);

 private:
  bool IgnoresLoadRequest() const;
  bool IsScriptForEventSupported() const;

  // FetchClassicScript corresponds to Step 21.6 of
  // https://html.spec.whatwg.org/#prepare-a-script
  // and must NOT be called from outside of PendingScript().
  //
  // https://html.spec.whatwg.org/#fetch-a-classic-script
  bool FetchClassicScript(const KURL&,
                          Document&,
                          const ScriptFetchOptions&,
                          const WTF::TextEncoding&);
  // https://html.spec.whatwg.org/#fetch-a-module-script-tree
  void FetchModuleScriptTree(const KURL&,
                             Modulator*,
                             const ScriptFetchOptions&);

  enum class ExecuteScriptResult {
    kShouldFireErrorEvent,
    kShouldFireLoadEvent,
    kShouldFireNone
  };
  ExecuteScriptResult DoExecuteScript(const Script*);
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

  // https://html.spec.whatwg.org/#script-processing-model
  // "A script element has several associated pieces of state.":

  // https://html.spec.whatwg.org/#already-started
  // "Initially, script elements must have this flag unset"
  bool already_started_ = false;

  // https://html.spec.whatwg.org/#parser-inserted
  // "Initially, script elements must have this flag unset."
  bool parser_inserted_ = false;

  // https://html.spec.whatwg.org/#non-blocking
  // "Initially, script elements must have this flag set."
  bool non_blocking_ = true;

  // https://html.spec.whatwg.org/#ready-to-be-parser-executed
  // "Initially, script elements must have this flag unset"
  bool ready_to_be_parser_executed_ = false;

  // https://html.spec.whatwg.org/#concept-script-type
  // "It is determined when the script is prepared"
  ScriptType script_type_ = ScriptType::kClassic;

  // https://html.spec.whatwg.org/#concept-script-external
  // "It is determined when the script is prepared"
  bool is_external_script_ = false;

  bool have_fired_load_;

  // Same as "The parser will handle executing the script."
  bool will_be_parser_executed_;

  bool will_execute_when_document_finished_parsing_;

  const bool created_during_document_write_;

  ScriptRunner::AsyncExecutionType async_exec_type_;

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
  Member<ScriptResource> resource_keep_alive_;

  // The context document at the time when PrepareScript() is executed.
  // This is only used to check whether the script element is moved between
  // documents and thus doesn't retain a strong reference.
  WeakMember<Document> original_document_;
};

}  // namespace blink

#endif  // ScriptLoader_h
