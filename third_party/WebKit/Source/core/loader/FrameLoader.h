/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) Research In Motion Limited 2009. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FrameLoader_h
#define FrameLoader_h

#include "core/CoreExport.h"
#include "core/dom/IconURL.h"
#include "core/dom/SandboxFlags.h"
#include "core/dom/SecurityContext.h"
#include "core/frame/FrameTypes.h"
#include "core/loader/FrameLoaderStateMachine.h"
#include "core/loader/FrameLoaderTypes.h"
#include "core/loader/HistoryItem.h"
#include "core/loader/NavigationPolicy.h"
#include "platform/heap/Handle.h"
#include "platform/instrumentation/tracing/TracedValue.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/HashSet.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include "public/web/WebTriggeringEventInfo.h"

#include <memory>

namespace blink {

class Document;
class DocumentLoader;
class HTMLFormElement;
class LocalFrame;
class Frame;
class LocalFrameClient;
class ProgressTracker;
class ResourceError;
class SerializedScriptValue;
class SubstituteData;
struct FrameLoadRequest;

CORE_EXPORT bool IsBackForwardLoadType(FrameLoadType);
CORE_EXPORT bool IsReloadLoadType(FrameLoadType);

class CORE_EXPORT FrameLoader final {
  WTF_MAKE_NONCOPYABLE(FrameLoader);
  DISALLOW_NEW();

 public:
  explicit FrameLoader(LocalFrame*);
  ~FrameLoader();

  void Init();

  ResourceRequest ResourceRequestForReload(
      FrameLoadType,
      const KURL& override_url = KURL(),
      ClientRedirectPolicy = ClientRedirectPolicy::kNotClientRedirect);

  ProgressTracker& Progress() const { return *progress_tracker_; }

  // Starts a load. It will eventually call StartLoad() or LoadInSameDocument().
  // For history navigations or reloads, an appropriate FrameLoadType should be
  // given. Otherwise, FrameLoadTypeStandard should be used (and the final
  // FrameLoadType will be computed). For history navigations, a history item
  // and a HistoryLoadType should also be provided.
  void Load(const FrameLoadRequest&,
            FrameLoadType = kFrameLoadTypeStandard,
            HistoryItem* = nullptr,
            HistoryLoadType = kHistoryDifferentDocumentLoad);

  // Warning: stopAllLoaders can and will detach the LocalFrame out from under
  // you. All callers need to either protect the LocalFrame or guarantee they
  // won't in any way access the LocalFrame after stopAllLoaders returns.
  void StopAllLoaders();

  void ReplaceDocumentWhileExecutingJavaScriptURL(const String& source,
                                                  Document* owner_document);

  // Notifies the client that the initial empty document has been accessed, and
  // thus it is no longer safe to show a provisional URL above the document
  // without risking a URL spoof. The client must not call back into JavaScript.
  void DidAccessInitialDocument();

  DocumentLoader* GetDocumentLoader() const { return document_loader_.Get(); }
  DocumentLoader* GetProvisionalDocumentLoader() const {
    return provisional_document_loader_.Get();
  }

  void LoadFailed(DocumentLoader*, const ResourceError&);

  bool IsLoadingMainFrame() const;

  bool ShouldTreatURLAsSameAsCurrent(const KURL&) const;
  bool ShouldTreatURLAsSrcdocDocument(const KURL&) const;

  void SetDefersLoading(bool);

  void DidExplicitOpen();

  String UserAgent() const;

  void DispatchDidClearWindowObjectInMainWorld();
  void DispatchDidClearDocumentOfWindowObject();
  void DispatchDocumentElementAvailable();
  void RunScriptsAtDocumentElementAvailable();

  // The following sandbox flags will be forced, regardless of changes to the
  // sandbox attribute of any parent frames.
  void ForceSandboxFlags(SandboxFlags flags) { forced_sandbox_flags_ |= flags; }
  SandboxFlags EffectiveSandboxFlags() const;

  WebInsecureRequestPolicy GetInsecureRequestPolicy() const;
  SecurityContext::InsecureNavigationsSet* InsecureNavigationsToUpgrade() const;
  void ModifyRequestForCSP(ResourceRequest&, Document*) const;

  Frame* Opener();
  void SetOpener(LocalFrame*);

  const AtomicString& RequiredCSP() const { return required_csp_; }
  void RecordLatestRequiredCSP();

  void Detach();

  void FinishedParsing();
  void DidFinishNavigation();

  // This prepares the FrameLoader for the next commit. It will dispatch unload
  // events, abort XHR requests and detach the document. Returns true if the
  // frame is ready to receive the next commit, or false otherwise.
  bool PrepareForCommit();

  void CommitProvisionalLoad();

  FrameLoaderStateMachine* StateMachine() const { return &state_machine_; }

  bool AllAncestorsAreComplete() const;  // including this

  bool ShouldClose(bool is_reload = false);
  void DispatchUnloadEvent();

  bool AllowPlugins(ReasonForCallingAllowPlugins);

  void UpdateForSameDocumentNavigation(const KURL&,
                                       SameDocumentNavigationSource,
                                       RefPtr<SerializedScriptValue>,
                                       HistoryScrollRestorationType,
                                       FrameLoadType,
                                       Document*);

  void SaveScrollState();
  void RestoreScrollPositionAndViewState();

  // The navigation should only be continued immediately in this frame if this
  // returns NavigationPolicyCurrentTab.
  NavigationPolicy ShouldContinueForNavigationPolicy(
      const ResourceRequest&,
      Document* origin_document,
      const SubstituteData&,
      DocumentLoader*,
      ContentSecurityPolicyDisposition,
      NavigationType,
      NavigationPolicy,
      FrameLoadType,
      bool is_client_redirect,
      WebTriggeringEventInfo,
      HTMLFormElement*);

  // Like ShouldContinueForNavigationPolicy, but should be used when following
  // redirects.
  NavigationPolicy ShouldContinueForRedirectNavigationPolicy(
      const ResourceRequest&,
      const SubstituteData&,
      DocumentLoader*,
      ContentSecurityPolicyDisposition,
      NavigationType,
      NavigationPolicy,
      FrameLoadType,
      bool is_client_redirect,
      HTMLFormElement*);

  // Note: When a PlzNavigtate navigation is handled by the client, we will
  // have created a dummy provisional DocumentLoader, so this will return true
  // while the client handles the navigation.
  bool HasProvisionalNavigation() const {
    return GetProvisionalDocumentLoader();
  }

  void DetachProvisionalDocumentLoader(DocumentLoader*);

  void Trace(blink::Visitor*);

  static void SetReferrerForFrameRequest(FrameLoadRequest&);

  void ClientDroppedNavigation();

 private:
  bool PrepareRequestForThisFrame(FrameLoadRequest&);
  FrameLoadType DetermineFrameLoadType(const FrameLoadRequest&);

  SubstituteData DefaultSubstituteDataForURL(const KURL&);

  bool ShouldPerformFragmentNavigation(bool is_form_submission,
                                       const String& http_method,
                                       FrameLoadType,
                                       const KURL&);
  void ProcessFragment(const KURL&, FrameLoadType, LoadStartType);

  NavigationPolicy CheckLoadCanStart(FrameLoadRequest&,
                                     FrameLoadType,
                                     NavigationPolicy,
                                     NavigationType);
  void StartLoad(FrameLoadRequest&,
                 FrameLoadType,
                 NavigationPolicy,
                 HistoryItem*);

  void LoadInSameDocument(const KURL&,
                          RefPtr<SerializedScriptValue> state_object,
                          FrameLoadType,
                          HistoryItem*,
                          ClientRedirectPolicy,
                          Document*);
  void RestoreScrollPositionAndViewState(FrameLoadType,
                                         HistoryLoadType,
                                         HistoryItem::ViewState*,
                                         HistoryScrollRestorationType);

  void ScheduleCheckCompleted();

  void DetachDocumentLoader(Member<DocumentLoader>&);

  void UpgradeInsecureRequest(ResourceRequest&, Document*) const;

  std::unique_ptr<TracedValue> ToTracedValue() const;
  void TakeObjectSnapshot() const;

  DocumentLoader* CreateDocumentLoader(const ResourceRequest&,
                                       const FrameLoadRequest&,
                                       FrameLoadType,
                                       NavigationType);

  LocalFrameClient* Client() const;

  Member<LocalFrame> frame_;
  AtomicString required_csp_;

  // FIXME: These should be std::unique_ptr<T> to reduce build times and
  // simplify header dependencies unless performance testing proves otherwise.
  // Some of these could be lazily created for memory savings on devices.
  mutable FrameLoaderStateMachine state_machine_;

  Member<ProgressTracker> progress_tracker_;

  // Document loaders for the three phases of frame loading. Note that while a
  // new request is being loaded, the old document loader may still be
  // referenced. E.g. while a new request is in the "policy" state, the old
  // document loader may be consulted in particular as it makes sense to imply
  // certain settings on the new loader.
  Member<DocumentLoader> document_loader_;
  Member<DocumentLoader> provisional_document_loader_;

  bool in_stop_all_loaders_;

  SandboxFlags forced_sandbox_flags_;

  bool dispatching_did_clear_window_object_in_main_world_;
  bool protect_provisional_loader_;
  bool detached_;
};

}  // namespace blink

#endif  // FrameLoader_h
