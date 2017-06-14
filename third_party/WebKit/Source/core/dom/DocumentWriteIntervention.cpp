// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DocumentWriteIntervention.h"

#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/Settings.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/DocumentLoader.h"
#include "core/probe/CoreProbes.h"
#include "platform/network/NetworkStateNotifier.h"
#include "platform/network/NetworkUtils.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebEffectiveConnectionType.h"

namespace blink {

namespace {

void EmitWarningForDocWriteScripts(const String& url, Document& document) {
  String message =
      "A Parser-blocking, cross site (i.e. different eTLD+1) script, " + url +
      ", is invoked via document.write. The network request for this script "
      "MAY be blocked by the browser in this or a future page load due to poor "
      "network connectivity. If blocked in this page load, it will be "
      "confirmed in a subsequent console message."
      "See https://www.chromestatus.com/feature/5718547946799104 "
      "for more details.";
  document.AddConsoleMessage(
      ConsoleMessage::Create(kJSMessageSource, kWarningMessageLevel, message));
  DVLOG(1) << message.Utf8().data();
}

bool IsConnectionEffectively2G(WebEffectiveConnectionType effective_type) {
  switch (effective_type) {
    case WebEffectiveConnectionType::kTypeSlow2G:
    case WebEffectiveConnectionType::kType2G:
      return true;
    case WebEffectiveConnectionType::kType3G:
    case WebEffectiveConnectionType::kType4G:
    case WebEffectiveConnectionType::kTypeUnknown:
    case WebEffectiveConnectionType::kTypeOffline:
      return false;
  }
  NOTREACHED();
  return false;
}

bool ShouldDisallowFetch(Settings* settings,
                         WebConnectionType connection_type,
                         WebEffectiveConnectionType effective_connection) {
  if (settings->GetDisallowFetchForDocWrittenScriptsInMainFrame())
    return true;
  if (settings
          ->GetDisallowFetchForDocWrittenScriptsInMainFrameOnSlowConnections() &&
      connection_type == kWebConnectionTypeCellular2G)
    return true;
  if (settings
          ->GetDisallowFetchForDocWrittenScriptsInMainFrameIfEffectively2G() &&
      IsConnectionEffectively2G(effective_connection))
    return true;
  return false;
}

}  // namespace

bool MaybeDisallowFetchForDocWrittenScript(ResourceRequest& request,
                                           FetchParameters::DeferOption defer,
                                           Document& document) {
  // Only scripts inserted via document.write are candidates for having their
  // fetch disallowed.
  if (!document.IsInDocumentWrite())
    return false;

  Settings* settings = document.GetSettings();
  if (!settings)
    return false;

  if (!document.GetFrame() || !document.GetFrame()->IsMainFrame())
    return false;

  // Only block synchronously loaded (parser blocking) scripts.
  if (defer != FetchParameters::kNoDefer)
    return false;

  probe::documentWriteFetchScript(&document);

  if (!request.Url().ProtocolIsInHTTPFamily())
    return false;

  // Avoid blocking same origin scripts, as they may be used to render main
  // page content, whereas cross-origin scripts inserted via document.write
  // are likely to be third party content.
  String request_host = request.Url().Host();
  String document_host = document.GetSecurityOrigin()->Domain();

  bool same_site = false;
  if (request_host == document_host)
    same_site = true;

  // If the hosts didn't match, then see if the domains match. For example, if
  // a script is served from static.example.com for a document served from
  // www.example.com, we consider that a first party script and allow it.
  String request_domain = NetworkUtils::GetDomainAndRegistry(
      request_host, NetworkUtils::kIncludePrivateRegistries);
  String document_domain = NetworkUtils::GetDomainAndRegistry(
      document_host, NetworkUtils::kIncludePrivateRegistries);
  // getDomainAndRegistry will return the empty string for domains that are
  // already top-level, such as localhost. Thus we only compare domains if we
  // get non-empty results back from getDomainAndRegistry.
  if (!request_domain.IsEmpty() && !document_domain.IsEmpty() &&
      request_domain == document_domain)
    same_site = true;

  if (same_site) {
    // This histogram is introduced to help decide whether we should also check
    // same scheme while deciding whether or not to block the script as is done
    // in other cases of "same site" usage. On the other hand we do not want to
    // block more scripts than necessary.
    if (request.Url().Protocol() != document.GetSecurityOrigin()->Protocol()) {
      document.Loader()->DidObserveLoadingBehavior(
          WebLoadingBehaviorFlag::
              kWebLoadingBehaviorDocumentWriteBlockDifferentScheme);
    }
    return false;
  }

  EmitWarningForDocWriteScripts(request.Url().GetString(), document);
  request.AddHTTPHeaderField("Intervention",
                             "<https://www.chromestatus.com/feature/"
                             "5718547946799104>; level=\"warning\"");

  // Do not block scripts if it is a page reload. This is to enable pages to
  // recover if blocking of a script is leading to a page break and the user
  // reloads the page.
  const FrameLoadType load_type = document.Loader()->LoadType();
  if (IsReloadLoadType(load_type)) {
    // Recording this metric since an increase in number of reloads for pages
    // where a script was blocked could be indicative of a page break.
    document.Loader()->DidObserveLoadingBehavior(
        WebLoadingBehaviorFlag::kWebLoadingBehaviorDocumentWriteBlockReload);
    return false;
  }

  // Add the metadata that this page has scripts inserted via document.write
  // that are eligible for blocking. Note that if there are multiple scripts
  // the flag will be conveyed to the browser process only once.
  document.Loader()->DidObserveLoadingBehavior(
      WebLoadingBehaviorFlag::kWebLoadingBehaviorDocumentWriteBlock);

  if (!ShouldDisallowFetch(
          settings, GetNetworkStateNotifier().ConnectionType(),
          document.GetFrame()->Client()->GetEffectiveConnectionType())) {
    return false;
  }

  request.SetCachePolicy(WebCachePolicy::kReturnCacheDataDontLoad);
  return true;
}

}  // namespace blink
