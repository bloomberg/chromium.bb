/*
 * Copyright (C) 2008, 2010 Apple Inc. All rights reserved.
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

#include "core/frame/Location.h"

#include "bindings/core/v8/BindingSecurity.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/usv_string_or_trusted_url.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/trustedtypes/TrustedURL.h"
#include "core/frame/DOMWindow.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/FrameLoader.h"
#include "core/url/DOMURLUtilsReadOnly.h"
#include "platform/bindings/V8DOMActivityLogger.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

Location::Location(DOMWindow* dom_window) : dom_window_(dom_window) {}

void Location::Trace(blink::Visitor* visitor) {
  visitor->Trace(dom_window_);
  ScriptWrappable::Trace(visitor);
}

inline const KURL& Location::Url() const {
  const KURL& url = GetDocument()->Url();
  if (!url.IsValid()) {
    // Use "about:blank" while the page is still loading (before we have a
    // frame).
    return BlankURL();
  }

  return url;
}

void Location::href(USVStringOrTrustedURL& result) const {
  result.SetUSVString(Url().StrippedForUseAsHref());
}

String Location::protocol() const {
  return DOMURLUtilsReadOnly::protocol(Url());
}

String Location::host() const {
  return DOMURLUtilsReadOnly::host(Url());
}

String Location::hostname() const {
  return DOMURLUtilsReadOnly::hostname(Url());
}

String Location::port() const {
  return DOMURLUtilsReadOnly::port(Url());
}

String Location::pathname() const {
  return DOMURLUtilsReadOnly::pathname(Url());
}

String Location::search() const {
  return DOMURLUtilsReadOnly::search(Url());
}

String Location::origin() const {
  return DOMURLUtilsReadOnly::origin(Url());
}

DOMStringList* Location::ancestorOrigins() const {
  DOMStringList* origins = DOMStringList::Create();
  if (!IsAttached())
    return origins;
  for (Frame* frame = dom_window_->GetFrame()->Tree().Parent(); frame;
       frame = frame->Tree().Parent()) {
    origins->Append(
        frame->GetSecurityContext()->GetSecurityOrigin()->ToString());
  }
  return origins;
}

String Location::toString() const {
  USVStringOrTrustedURL result;
  href(result);
  DCHECK(result.IsUSVString());
  return result.GetAsUSVString();
}

String Location::hash() const {
  return DOMURLUtilsReadOnly::hash(Url());
}

void Location::setHref(LocalDOMWindow* current_window,
                       LocalDOMWindow* entered_window,
                       const USVStringOrTrustedURL& stringOrUrl,
                       ExceptionState& exception_state) {
  DCHECK(stringOrUrl.IsUSVString() ||
         RuntimeEnabledFeatures::TrustedDOMTypesEnabled());

  if (stringOrUrl.IsUSVString() &&
      current_window->document()->RequireTrustedTypes()) {
    exception_state.ThrowTypeError(
        "This document requires `TrustedURL` assignment.");
    return;
  }

  String url = stringOrUrl.IsUSVString()
                   ? stringOrUrl.GetAsUSVString()
                   : stringOrUrl.GetAsTrustedURL()->toString();
  SetLocation(url, current_window, entered_window, &exception_state);
}

void Location::setProtocol(LocalDOMWindow* current_window,
                           LocalDOMWindow* entered_window,
                           const String& protocol,
                           ExceptionState& exception_state) {
  KURL url = GetDocument()->Url();
  if (!url.SetProtocol(protocol)) {
    exception_state.ThrowDOMException(
        kSyntaxError, "'" + protocol + "' is an invalid protocol.");
    return;
  }
  SetLocation(url.GetString(), current_window, entered_window,
              &exception_state);
}

void Location::setHost(LocalDOMWindow* current_window,
                       LocalDOMWindow* entered_window,
                       const String& host,
                       ExceptionState& exception_state) {
  KURL url = GetDocument()->Url();
  url.SetHostAndPort(host);
  SetLocation(url.GetString(), current_window, entered_window,
              &exception_state);
}

void Location::setHostname(LocalDOMWindow* current_window,
                           LocalDOMWindow* entered_window,
                           const String& hostname,
                           ExceptionState& exception_state) {
  KURL url = GetDocument()->Url();
  url.SetHost(hostname);
  SetLocation(url.GetString(), current_window, entered_window,
              &exception_state);
}

void Location::setPort(LocalDOMWindow* current_window,
                       LocalDOMWindow* entered_window,
                       const String& port_string,
                       ExceptionState& exception_state) {
  KURL url = GetDocument()->Url();
  url.SetPort(port_string);
  SetLocation(url.GetString(), current_window, entered_window,
              &exception_state);
}

void Location::setPathname(LocalDOMWindow* current_window,
                           LocalDOMWindow* entered_window,
                           const String& pathname,
                           ExceptionState& exception_state) {
  KURL url = GetDocument()->Url();
  url.SetPath(pathname);
  SetLocation(url.GetString(), current_window, entered_window,
              &exception_state);
}

void Location::setSearch(LocalDOMWindow* current_window,
                         LocalDOMWindow* entered_window,
                         const String& search,
                         ExceptionState& exception_state) {
  KURL url = GetDocument()->Url();
  url.SetQuery(search);
  SetLocation(url.GetString(), current_window, entered_window,
              &exception_state);
}

void Location::setHash(LocalDOMWindow* current_window,
                       LocalDOMWindow* entered_window,
                       const String& hash,
                       ExceptionState& exception_state) {
  KURL url = GetDocument()->Url();
  String old_fragment_identifier = url.FragmentIdentifier();
  String new_fragment_identifier = hash;
  if (hash[0] == '#')
    new_fragment_identifier = hash.Substring(1);
  url.SetFragmentIdentifier(new_fragment_identifier);
  // Note that by parsing the URL and *then* comparing fragments, we are
  // comparing fragments post-canonicalization, and so this handles the
  // cases where fragment identifiers are ignored or invalid.
  if (EqualIgnoringNullity(old_fragment_identifier, url.FragmentIdentifier()))
    return;
  SetLocation(url.GetString(), current_window, entered_window,
              &exception_state);
}

void Location::assign(LocalDOMWindow* current_window,
                      LocalDOMWindow* entered_window,
                      const String& url,
                      ExceptionState& exception_state) {
  // TODO(yukishiino): Remove this check once we remove [CrossOrigin] from
  // the |assign| DOM operation's definition in Location.idl.  See the comment
  // in Location.idl for details.
  if (!BindingSecurity::ShouldAllowAccessTo(current_window, this,
                                            exception_state)) {
    return;
  }

  SetLocation(url, current_window, entered_window, &exception_state);
}

void Location::replace(LocalDOMWindow* current_window,
                       LocalDOMWindow* entered_window,
                       const String& url,
                       ExceptionState& exception_state) {
  SetLocation(url, current_window, entered_window, &exception_state,
              SetLocationPolicy::kReplaceThisFrame);
}

void Location::reload(LocalDOMWindow* current_window) {
  if (!IsAttached())
    return;
  if (GetDocument()->Url().ProtocolIsJavaScript())
    return;
  dom_window_->GetFrame()->Reload(kFrameLoadTypeReload,
                                  ClientRedirectPolicy::kClientRedirect);
}

void Location::SetLocation(const String& url,
                           LocalDOMWindow* current_window,
                           LocalDOMWindow* entered_window,
                           ExceptionState* exception_state,
                           SetLocationPolicy set_location_policy) {
  if (!IsAttached())
    return;

  if (!current_window->GetFrame())
    return;

  Document* entered_document = entered_window->document();
  if (!entered_document)
    return;

  KURL completed_url = entered_document->CompleteURL(url);
  if (completed_url.IsNull())
    return;

  if (!current_window->GetFrame()->CanNavigate(*dom_window_->GetFrame(),
                                               completed_url)) {
    if (exception_state) {
      exception_state->ThrowSecurityError(
          "The current window does not have permission to navigate the target "
          "frame to '" +
          url + "'.");
    }
    return;
  }
  if (exception_state && !completed_url.IsValid()) {
    exception_state->ThrowDOMException(kSyntaxError,
                                       "'" + url + "' is not a valid URL.");
    return;
  }

  if (dom_window_->IsInsecureScriptAccess(*current_window, completed_url))
    return;

  V8DOMActivityLogger* activity_logger =
      V8DOMActivityLogger::CurrentActivityLoggerIfIsolatedWorld();
  if (activity_logger) {
    Vector<String> argv;
    argv.push_back("LocalDOMWindow");
    argv.push_back("url");
    argv.push_back(entered_document->Url());
    argv.push_back(completed_url);
    activity_logger->LogEvent("blinkSetAttribute", argv.size(), argv.data());
  }
  dom_window_->GetFrame()->Navigate(
      *current_window->document(), completed_url,
      set_location_policy == SetLocationPolicy::kReplaceThisFrame,
      UserGestureStatus::kNone);
}

Document* Location::GetDocument() const {
  return ToLocalDOMWindow(dom_window_)->document();
}

bool Location::IsAttached() const {
  return dom_window_->GetFrame();
}

}  // namespace blink
