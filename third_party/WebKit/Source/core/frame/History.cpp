/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/frame/History.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/HistoryItem.h"
#include "core/loader/NavigationScheduler.h"
#include "core/page/Page.h"
#include "platform/bindings/ScriptState.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/text/StringView.h"

namespace blink {

namespace {

bool EqualIgnoringPathQueryAndFragment(const KURL& a, const KURL& b) {
  return StringView(a.GetString(), 0, a.PathStart()) ==
         StringView(b.GetString(), 0, b.PathStart());
}

bool EqualIgnoringQueryAndFragment(const KURL& a, const KURL& b) {
  return StringView(a.GetString(), 0, a.PathEnd()) ==
         StringView(b.GetString(), 0, b.PathEnd());
}

}  // namespace

History::History(LocalFrame* frame)
    : DOMWindowClient(frame), last_state_object_requested_(nullptr) {}

DEFINE_TRACE(History) {
  DOMWindowClient::Trace(visitor);
}

unsigned History::length(ExceptionState& exception_state) const {
  if (!GetFrame() || !GetFrame()->Client()) {
    exception_state.ThrowSecurityError(
        "May not use a History object associated with a Document that is not "
        "fully active");
    return 0;
  }
  return GetFrame()->Client()->BackForwardLength();
}

SerializedScriptValue* History::state(ExceptionState& exception_state) {
  if (!GetFrame()) {
    exception_state.ThrowSecurityError(
        "May not use a History object associated with a Document that is not "
        "fully active");
    return 0;
  }
  last_state_object_requested_ = StateInternal();
  return last_state_object_requested_.Get();
}

SerializedScriptValue* History::StateInternal() const {
  if (!GetFrame())
    return 0;

  if (HistoryItem* history_item =
          GetFrame()->Loader().GetDocumentLoader()->GetHistoryItem()) {
    return history_item->StateObject();
  }

  return 0;
}

void History::setScrollRestoration(const String& value,
                                   ExceptionState& exception_state) {
  DCHECK(value == "manual" || value == "auto");
  if (!GetFrame() || !GetFrame()->Client()) {
    exception_state.ThrowSecurityError(
        "May not use a History object associated with a Document that is not "
        "fully active");
    return;
  }

  HistoryScrollRestorationType scroll_restoration =
      value == "manual" ? kScrollRestorationManual : kScrollRestorationAuto;
  if (scroll_restoration == ScrollRestorationInternal())
    return;

  if (HistoryItem* history_item =
          GetFrame()->Loader().GetDocumentLoader()->GetHistoryItem()) {
    history_item->SetScrollRestorationType(scroll_restoration);
    GetFrame()->Client()->DidUpdateCurrentHistoryItem();
  }
}

String History::scrollRestoration(ExceptionState& exception_state) {
  if (!GetFrame() || !GetFrame()->Client()) {
    exception_state.ThrowSecurityError(
        "May not use a History object associated with a Document that is not "
        "fully active");
    return "auto";
  }
  return ScrollRestorationInternal() == kScrollRestorationManual ? "manual"
                                                                 : "auto";
}

HistoryScrollRestorationType History::ScrollRestorationInternal() const {
  HistoryItem* history_item =
      GetFrame() ? GetFrame()->Loader().GetDocumentLoader()->GetHistoryItem()
                 : nullptr;
  return history_item ? history_item->ScrollRestorationType()
                      : kScrollRestorationAuto;
}

// TODO(crbug.com/394296): This is not the long-term fix to IPC flooding that we
// need. However, it does somewhat mitigate the immediate concern of |pushState|
// and |replaceState| DoS (assuming the renderer has not been compromised).
bool History::ShouldThrottleStateObjectChanges() {
  const int kStateUpdateLimit = 50;

  if (state_flood_guard.count > kStateUpdateLimit) {
    static constexpr auto kStateUpdateLimitResetInterval =
        TimeDelta::FromSeconds(10);
    const auto now = TimeTicks::Now();
    if (now - state_flood_guard.last_updated > kStateUpdateLimitResetInterval) {
      state_flood_guard.count = 0;
      state_flood_guard.last_updated = now;
      return false;
    }
    return true;
  }

  state_flood_guard.count++;
  return false;
}

bool History::stateChanged() const {
  return last_state_object_requested_ != StateInternal();
}

bool History::IsSameAsCurrentState(SerializedScriptValue* state) const {
  return state == StateInternal();
}

void History::back(ScriptState* script_state, ExceptionState& exception_state) {
  go(script_state, -1, exception_state);
}

void History::forward(ScriptState* script_state,
                      ExceptionState& exception_state) {
  go(script_state, 1, exception_state);
}

void History::go(ScriptState* script_state,
                 int delta,
                 ExceptionState& exception_state) {
  if (!GetFrame() || !GetFrame()->Client()) {
    exception_state.ThrowSecurityError(
        "May not use a History object associated with a Document that is not "
        "fully active");
    return;
  }

  DCHECK(IsMainThread());
  Document* active_document = ToDocument(ExecutionContext::From(script_state));
  if (!active_document)
    return;

  if (!active_document->GetFrame() ||
      !active_document->GetFrame()->CanNavigate(*GetFrame()) ||
      !active_document->GetFrame()->IsNavigationAllowed() ||
      !NavigationDisablerForBeforeUnload::IsNavigationAllowed()) {
    return;
  }

  if (delta) {
    GetFrame()->Client()->NavigateBackForward(delta);
  } else {
    // We intentionally call reload() for the current frame if delta is zero.
    // Otherwise, navigation happens on the root frame.
    // This behavior is designed in the following spec.
    // https://html.spec.whatwg.org/multipage/browsers.html#dom-history-go
    GetFrame()->Reload(kFrameLoadTypeReload,
                       ClientRedirectPolicy::kClientRedirect);
  }
}

void History::pushState(RefPtr<SerializedScriptValue> data,
                        const String& title,
                        const String& url,
                        ExceptionState& exception_state) {
  StateObjectAdded(std::move(data), title, url, ScrollRestorationInternal(),
                   kFrameLoadTypeStandard, exception_state);
}

KURL History::UrlForState(const String& url_string) {
  Document* document = GetFrame()->GetDocument();

  if (url_string.IsNull())
    return document->Url();
  if (url_string.IsEmpty())
    return document->BaseURL();

  return KURL(document->BaseURL(), url_string);
}

bool History::CanChangeToUrl(const KURL& url,
                             SecurityOrigin* document_origin,
                             const KURL& document_url) {
  if (!url.IsValid())
    return false;

  if (document_origin->IsGrantedUniversalAccess())
    return true;

  // We allow sandboxed documents, `data:`/`file:` URLs, etc. to use
  // 'pushState'/'replaceState' to modify the URL fragment: see
  // https://crbug.com/528681 for the compatibility concerns.
  if (document_origin->IsUnique() || document_origin->IsLocal())
    return EqualIgnoringQueryAndFragment(url, document_url);

  if (!EqualIgnoringPathQueryAndFragment(url, document_url))
    return false;

  RefPtr<SecurityOrigin> requested_origin = SecurityOrigin::Create(url);
  if (requested_origin->IsUnique() ||
      !requested_origin->IsSameSchemeHostPort(document_origin)) {
    return false;
  }

  return true;
}

void History::StateObjectAdded(RefPtr<SerializedScriptValue> data,
                               const String& /* title */,
                               const String& url_string,
                               HistoryScrollRestorationType restoration_type,
                               FrameLoadType type,
                               ExceptionState& exception_state) {
  if (!GetFrame() || !GetFrame()->GetPage() ||
      !GetFrame()->Loader().GetDocumentLoader()) {
    exception_state.ThrowSecurityError(
        "May not use a History object associated with a Document that is not "
        "fully active");
    return;
  }

  KURL full_url = UrlForState(url_string);
  if (!CanChangeToUrl(full_url, GetFrame()->GetDocument()->GetSecurityOrigin(),
                      GetFrame()->GetDocument()->Url())) {
    // We can safely expose the URL to JavaScript, as a) no redirection takes
    // place: JavaScript already had this URL, b) JavaScript can only access a
    // same-origin History object.
    exception_state.ThrowSecurityError(
        "A history state object with URL '" + full_url.ElidedString() +
        "' cannot be created in a document with origin '" +
        GetFrame()->GetDocument()->GetSecurityOrigin()->ToString() +
        "' and URL '" + GetFrame()->GetDocument()->Url().ElidedString() + "'.");
    return;
  }

  if (ShouldThrottleStateObjectChanges())
    return;

  GetFrame()->Loader().UpdateForSameDocumentNavigation(
      full_url, kSameDocumentNavigationHistoryApi, std::move(data),
      restoration_type, type, GetFrame()->GetDocument());
}

}  // namespace blink
