// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "Chrome.h"
#include "CString.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "HTMLAppletElement.h"
#include "HTMLFormElement.h"  // needed by FormState.h
#include "HTMLNames.h"
#include "FormState.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "HitTestResult.h"
#include "MIMETypeRegistry.h"
#include "MouseEvent.h"
#include "Page.h"
#include "PlatformString.h"
#include "PluginData.h"
#include "StringExtras.h"
#include "WindowFeatures.h"
#undef LOG

#include "net/base/mime_util.h"
#include "webkit/api/public/WebForm.h"
#include "webkit/api/public/WebFrameClient.h"
#include "webkit/api/public/WebNode.h"
#include "webkit/api/public/WebPlugin.h"
#include "webkit/api/public/WebPluginParams.h"
#include "webkit/api/public/WebSecurityOrigin.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/api/public/WebURLError.h"
#include "webkit/api/public/WebVector.h"
#include "webkit/api/public/WebViewClient.h"
#include "webkit/api/src/WebDataSourceImpl.h"
#include "webkit/api/src/WebPluginContainerImpl.h"
#include "webkit/api/src/WebPluginLoadObserver.h"
#include "webkit/api/src/WrappedResourceRequest.h"
#include "webkit/api/src/WrappedResourceResponse.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webdevtoolsagent_impl.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webframeloaderclient_impl.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webview_impl.h"

using namespace WebCore;

using WebKit::WebData;
using WebKit::WebDataSourceImpl;
using WebKit::WebNavigationType;
using WebKit::WebNavigationPolicy;
using WebKit::WebNode;
using WebKit::WebPlugin;
using WebKit::WebPluginContainerImpl;
using WebKit::WebPluginLoadObserver;
using WebKit::WebPluginParams;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebURLError;
using WebKit::WebURLRequest;
using WebKit::WebVector;
using WebKit::WrappedResourceRequest;
using WebKit::WrappedResourceResponse;

// Domain for internal error codes.
static const char kInternalErrorDomain[] = "WebKit";

// An internal error code.  Used to note a policy change error resulting from
// dispatchDecidePolicyForMIMEType not passing the PolicyUse option.
enum {
  ERR_POLICY_CHANGE = -10000,
};

static void CopyStringVector(
    const Vector<String>& input, WebVector<WebString>* output) {
  WebVector<WebString> result(input.size());
  for (size_t i = 0; i < input.size(); ++i)
    result[i] = webkit_glue::StringToWebString(input[i]);
  output->swap(result);
}

WebFrameLoaderClient::WebFrameLoaderClient(WebFrameImpl* frame)
    : webframe_(frame),
      has_representation_(false),
      sent_initial_response_to_plugin_(false),
      next_navigation_policy_(WebKit::WebNavigationPolicyIgnore) {
}

WebFrameLoaderClient::~WebFrameLoaderClient() {
}

void WebFrameLoaderClient::frameLoaderDestroyed() {
  // When the WebFrame was created, it had an extra reference given to it on
  // behalf of the Frame.  Since the WebFrame owns us, this extra ref also
  // serves to keep us alive until the FrameLoader is done with us.  The
  // FrameLoader calls this method when it's going away.  Therefore, we balance
  // out that extra reference, which may cause 'this' to be deleted.
  webframe_->Closing();
  webframe_->deref();
}

void WebFrameLoaderClient::windowObjectCleared() {
  if (webframe_->client())
    webframe_->client()->didClearWindowObject(webframe_);

  WebViewImpl* webview = webframe_->GetWebViewImpl();
  if (webview) {
    WebDevToolsAgentImpl* tools_agent = webview->GetWebDevToolsAgentImpl();
    if (tools_agent)
      tools_agent->WindowObjectCleared(webframe_);
  }
}

void WebFrameLoaderClient::documentElementAvailable() {
  if (webframe_->client())
    webframe_->client()->didCreateDocumentElement(webframe_);
}

void WebFrameLoaderClient::didCreateScriptContextForFrame() {
  if (webframe_->client())
    webframe_->client()->didCreateScriptContext(webframe_);
}

void WebFrameLoaderClient::didDestroyScriptContextForFrame() {
  if (webframe_->client())
    webframe_->client()->didDestroyScriptContext(webframe_);
}

void WebFrameLoaderClient::didCreateIsolatedScriptContext() {
  if (webframe_->client())
    webframe_->client()->didCreateIsolatedScriptContext(webframe_);
}

void WebFrameLoaderClient::didPerformFirstNavigation() const {
}

void WebFrameLoaderClient::registerForIconNotification(bool listen){
}

bool WebFrameLoaderClient::hasWebView() const {
  return webframe_->GetWebViewImpl() != NULL;
}

bool WebFrameLoaderClient::hasFrameView() const {
  // The Mac port has this notion of a WebFrameView, which seems to be
  // some wrapper around an NSView.  Since our equivalent is HWND, I guess
  // we have a "frameview" whenever we have the toplevel HWND.
  return webframe_->GetWebViewImpl() != NULL;
}

void WebFrameLoaderClient::makeDocumentView() {
  webframe_->CreateFrameView();
}

void WebFrameLoaderClient::makeRepresentation(DocumentLoader*) {
  has_representation_ = true;
}

void WebFrameLoaderClient::forceLayout() {
  // FIXME
}
void WebFrameLoaderClient::forceLayoutForNonHTML() {
  // FIXME
}

void WebFrameLoaderClient::setCopiesOnScroll() {
  // FIXME
}

void WebFrameLoaderClient::detachedFromParent2() {
  // Nothing to do here.
}

void WebFrameLoaderClient::detachedFromParent3() {
  // Close down the proxy.  The purpose of this change is to make the
  // call to ScriptController::clearWindowShell a no-op when called from
  // Frame::pageDestroyed.  Without this change, this call to clearWindowShell
  // will cause a crash.  If you remove/modify this, just ensure that you can
  // go to a page and then navigate to a new page without getting any asserts
  // or crashes.
  webframe_->frame()->script()->proxy()->clearForClose();
}

// This function is responsible for associating the |identifier| with a given
// subresource load.  The following functions that accept an |identifier| are
// called for each subresource, so they should not be dispatched to the
// WebFrame.
void WebFrameLoaderClient::assignIdentifierToInitialRequest(
    unsigned long identifier, DocumentLoader* loader,
    const ResourceRequest& request) {
  if (webframe_->client()) {
    WrappedResourceRequest webreq(request);
    webframe_->client()->assignIdentifierToRequest(
        webframe_, identifier, webreq);
  }
}

// Determines whether the request being loaded by |loader| is a frame or a
// subresource. A subresource in this context is anything other than a frame --
// this includes images and xmlhttp requests.  It is important to note that a
// subresource is NOT limited to stuff loaded through the frame's subresource
// loader. Synchronous xmlhttp requests for example, do not go through the
// subresource loader, but we still label them as TargetIsSubResource.
//
// The important edge cases to consider when modifying this function are
// how synchronous resource loads are treated during load/unload threshold.
static ResourceRequest::TargetType DetermineTargetTypeFromLoader(
    DocumentLoader* loader) {
  if (loader == loader->frameLoader()->provisionalDocumentLoader()) {
    if (loader->frameLoader()->isLoadingMainFrame()) {
      return ResourceRequest::TargetIsMainFrame;
    } else {
      return ResourceRequest::TargetIsSubFrame;
    }
  }
  return ResourceRequest::TargetIsSubResource;
}

void WebFrameLoaderClient::dispatchWillSendRequest(
    DocumentLoader* loader, unsigned long identifier, ResourceRequest& request,
    const ResourceResponse& redirect_response) {

  if (loader) {
    // We want to distinguish between a request for a document to be loaded into
    // the main frame, a sub-frame, or the sub-objects in that document.
    request.setTargetType(DetermineTargetTypeFromLoader(loader));
  }

  // FrameLoader::loadEmptyDocumentSynchronously() creates an empty document
  // with no URL.  We don't like that, so we'll rename it to about:blank.
  if (request.url().isEmpty())
    request.setURL(KURL(ParsedURLString, "about:blank"));
  if (request.firstPartyForCookies().isEmpty())
    request.setFirstPartyForCookies(KURL(ParsedURLString, "about:blank"));

  // Give the WebFrameClient a crack at the request.
  if (webframe_->client()) {
    WrappedResourceRequest webreq(request);
    WrappedResourceResponse webresp(redirect_response);
    webframe_->client()->willSendRequest(
        webframe_, identifier, webreq, webresp);
  }
}

bool WebFrameLoaderClient::shouldUseCredentialStorage(DocumentLoader*,
    unsigned long identifier) {
  // FIXME
  // Intended to pass through to a method on the resource load delegate.
  // If implemented, that method controls whether the browser should ask the
  // networking layer for a stored default credential for the page (say from
  // the Mac OS keychain). If the method returns false, the user should be
  // presented with an authentication challenge whether or not the networking
  // layer has a credential stored.
  // This returns true for backward compatibility: the ability to override the
  // system credential store is new. (Actually, not yet fully implemented in
  // WebKit, as of this writing.)
  return true;
}

void WebFrameLoaderClient::dispatchDidReceiveAuthenticationChallenge(
    DocumentLoader*, unsigned long identifier, const AuthenticationChallenge&) {
  // FIXME
}

void WebFrameLoaderClient::dispatchDidCancelAuthenticationChallenge(
    DocumentLoader*, unsigned long identifier, const AuthenticationChallenge&) {
  // FIXME
}

void WebFrameLoaderClient::dispatchDidReceiveResponse(DocumentLoader* loader,
                                                      unsigned long identifier,
                                                      const ResourceResponse& response) {
  if (webframe_->client()) {
    WrappedResourceResponse webresp(response);
    webframe_->client()->didReceiveResponse(webframe_, identifier, webresp);
  }
}

void WebFrameLoaderClient::dispatchDidReceiveContentLength(
    DocumentLoader* loader,
    unsigned long identifier,
    int length_received) {
}

// Called when a particular resource load completes
void WebFrameLoaderClient::dispatchDidFinishLoading(DocumentLoader* loader,
                                                    unsigned long identifier) {
  if (webframe_->client())
    webframe_->client()->didFinishResourceLoad(webframe_, identifier);
}

void WebFrameLoaderClient::dispatchDidFailLoading(DocumentLoader* loader,
                                                  unsigned long identifier,
                                                  const ResourceError& error) {
  if (webframe_->client()) {
    webframe_->client()->didFailResourceLoad(
        webframe_, identifier, webkit_glue::ResourceErrorToWebURLError(error));
  }
}

void WebFrameLoaderClient::dispatchDidFinishDocumentLoad() {
  // A frame may be reused.  This call ensures we don't hold on to our password
  // listeners and their associated HTMLInputElements.
  webframe_->ClearPasswordListeners();

  if (webframe_->client())
    webframe_->client()->didFinishDocumentLoad(webframe_);
}

bool WebFrameLoaderClient::dispatchDidLoadResourceFromMemoryCache(
    DocumentLoader* loader,
    const ResourceRequest& request,
    const ResourceResponse& response,
    int length) {
  if (webframe_->client()) {
    WrappedResourceRequest webreq(request);
    WrappedResourceResponse webresp(response);
    webframe_->client()->didLoadResourceFromMemoryCache(
        webframe_, webreq, webresp);
  }
  return false;  // Do not suppress remaining notifications
}

void WebFrameLoaderClient::dispatchDidLoadResourceByXMLHttpRequest(
    unsigned long identifier,
    const ScriptString& source) {
}

void WebFrameLoaderClient::dispatchDidHandleOnloadEvents() {
  if (webframe_->client())
    webframe_->client()->didHandleOnloadEvents(webframe_);
}

// Redirect Tracking
// =================
// We want to keep track of the chain of redirects that occur during page
// loading. There are two types of redirects, server redirects which are HTTP
// response codes, and client redirects which are document.location= and meta
// refreshes.
//
// This outlines the callbacks that we get in different redirect situations,
// and how each call modifies the redirect chain.
//
// Normal page load
// ----------------
//   dispatchDidStartProvisionalLoad() -> adds URL to the redirect list
//   dispatchDidCommitLoad()           -> DISPATCHES & clears list
//
// Server redirect (success)
// -------------------------
//   dispatchDidStartProvisionalLoad()                    -> adds source URL
//   dispatchDidReceiveServerRedirectForProvisionalLoad() -> adds dest URL
//   dispatchDidCommitLoad()                              -> DISPATCHES
//
// Client redirect (success)
// -------------------------
//   (on page)
//   dispatchWillPerformClientRedirect() -> saves expected redirect
//   dispatchDidStartProvisionalLoad()   -> appends redirect source (since
//                                          it matches the expected redirect)
//                                          and the current page as the dest)
//   dispatchDidCancelClientRedirect()   -> clears expected redirect
//   dispatchDidCommitLoad()             -> DISPATCHES
//
// Client redirect (cancelled)
// (e.g meta-refresh trumped by manual doc.location change, or just cancelled
// because a link was clicked that requires the meta refresh to be rescheduled
// (the SOURCE URL may have changed).
// ---------------------------
//   dispatchDidCancelClientRedirect()                 -> clears expected redirect
//   dispatchDidStartProvisionalLoad()                 -> adds only URL to redirect list
//   dispatchDidCommitLoad()                           -> DISPATCHES & clears list
//   rescheduled ? dispatchWillPerformClientRedirect() -> saves expected redirect
//               : nothing

// Client redirect (failure)
// -------------------------
//   (on page)
//   dispatchWillPerformClientRedirect() -> saves expected redirect
//   dispatchDidStartProvisionalLoad()   -> appends redirect source (since
//                                          it matches the expected redirect)
//                                          and the current page as the dest)
//   dispatchDidCancelClientRedirect()
//   dispatchDidFailProvisionalLoad()
//
// Load 1 -> Server redirect to 2 -> client redirect to 3 -> server redirect to 4
// ------------------------------------------------------------------------------
//   dispatchDidStartProvisionalLoad()                    -> adds source URL 1
//   dispatchDidReceiveServerRedirectForProvisionalLoad() -> adds dest URL 2
//   dispatchDidCommitLoad()                              -> DISPATCHES 1+2
//    -- begin client redirect and NEW DATA SOURCE
//   dispatchWillPerformClientRedirect()                  -> saves expected redirect
//   dispatchDidStartProvisionalLoad()                    -> appends URL 2 and URL 3
//   dispatchDidReceiveServerRedirectForProvisionalLoad() -> appends destination URL 4
//   dispatchDidCancelClientRedirect()                    -> clears expected redirect
//   dispatchDidCommitLoad()                              -> DISPATCHES
//
// Interesting case with multiple location changes involving anchors.
// Load page 1 containing future client-redirect (back to 1, e.g meta refresh) > Click
// on a link back to the same page (i.e an anchor href) >
// client-redirect finally fires (with new source, set to 1#anchor)
// -----------------------------------------------------------------------------
//   dispatchWillPerformClientRedirect(non-zero 'interval' param) -> saves expected redirect
//   -- click on anchor href
//   dispatchDidCancelClientRedirect()                            -> clears expected redirect
//   dispatchDidStartProvisionalLoad()                            -> adds 1#anchor source
//   dispatchDidCommitLoad()                                      -> DISPATCHES 1#anchor
//   dispatchWillPerformClientRedirect()                          -> saves exp. source (1#anchor)
//   -- redirect timer fires
//   dispatchDidStartProvisionalLoad()                            -> appends 1#anchor (src) and 1 (dest)
//   dispatchDidCancelClientRedirect()                            -> clears expected redirect
//   dispatchDidCommitLoad()                                      -> DISPATCHES 1#anchor + 1
//
void WebFrameLoaderClient::dispatchDidReceiveServerRedirectForProvisionalLoad() {
  WebDataSourceImpl* ds = webframe_->GetProvisionalDataSourceImpl();
  if (!ds) {
    // Got a server redirect when there is no provisional DS!
    ASSERT_NOT_REACHED();
    return;
  }

  // The server redirect may have been blocked.
  if (ds->request().isNull())
    return;

  // A provisional load should have started already, which should have put an
  // entry in our redirect chain.
  ASSERT(ds->hasRedirectChain());

  // The URL of the destination is on the provisional data source. We also need
  // to update the redirect chain to account for this addition (we do this
  // before the callback so the callback can look at the redirect chain to see
  // what happened).
  ds->appendRedirect(webkit_glue::WebURLToKURL(ds->request().url()));

  if (webframe_->client())
    webframe_->client()->didReceiveServerRedirectForProvisionalLoad(webframe_);
}

// Called on both success and failure of a client redirect.
void WebFrameLoaderClient::dispatchDidCancelClientRedirect() {
  // No longer expecting a client redirect.
  if (webframe_->client()) {
    expected_client_redirect_src_ = KURL();
    expected_client_redirect_dest_ = KURL();
    webframe_->client()->didCancelClientRedirect(webframe_);
  }

  // No need to clear the redirect chain, since that data source has already
  // been deleted by the time this function is called.
}

void WebFrameLoaderClient::dispatchWillPerformClientRedirect(
    const KURL& url,
    double interval,
    double fire_date) {
  // Tells dispatchDidStartProvisionalLoad that if it sees this item it is a
  // redirect and the source item should be added as the start of the chain.
  expected_client_redirect_src_ = webkit_glue::WebURLToKURL(webframe_->url());
  expected_client_redirect_dest_ = url;

  // TODO(timsteele): bug 1135512. Webkit does not properly notify us of
  // cancelling http > file client redirects. Since the FrameLoader's policy
  // is to never carry out such a navigation anyway, the best thing we can do
  // for now to not get confused is ignore this notification.
  if (expected_client_redirect_dest_.isLocalFile() &&
      expected_client_redirect_src_.protocolInHTTPFamily()) {
    expected_client_redirect_src_ = KURL();
    expected_client_redirect_dest_ = KURL();
    return;
  }

  if (webframe_->client()) {
    webframe_->client()->willPerformClientRedirect(
        webframe_,
        webkit_glue::KURLToWebURL(expected_client_redirect_src_),
        webkit_glue::KURLToWebURL(expected_client_redirect_dest_),
        static_cast<unsigned int>(interval),
        static_cast<unsigned int>(fire_date));
  }
}

void WebFrameLoaderClient::dispatchDidChangeLocationWithinPage() {
  // Anchor fragment navigations are not normal loads, so we need to synthesize
  // some events for our delegate.
  WebViewImpl* webview = webframe_->GetWebViewImpl();
  if (webview->client())
    webview->client()->didStartLoading();

  WebDataSourceImpl* ds = webframe_->GetDataSourceImpl();
  ASSERT(ds);  // Should not be null when navigating to a reference fragment!
  if (ds) {
    KURL url = webkit_glue::WebURLToKURL(ds->request().url());
    KURL chain_end;
    if (ds->hasRedirectChain()) {
      chain_end = ds->endOfRedirectChain();
      ds->clearRedirectChain();
    }

    // Figure out if this location change is because of a JS-initiated client
    // redirect (e.g onload/setTimeout document.location.href=).
    // TODO(timsteele): (bugs 1085325, 1046841) We don't get proper redirect
    // performed/cancelled notifications across anchor navigations, so the
    // other redirect-tracking code in this class (see dispatch*ClientRedirect()
    // and dispatchDidStartProvisionalLoad) is insufficient to catch and
    // properly flag these transitions. Once a proper fix for this bug is
    // identified and applied the following block may no longer be required.
    bool was_client_redirect =
        (url == expected_client_redirect_dest_ &&
         chain_end == expected_client_redirect_src_) ||
        !webframe_->isProcessingUserGesture();

    if (was_client_redirect) {
      if (webframe_->client()) {
        webframe_->client()->didCompleteClientRedirect(
            webframe_, webkit_glue::KURLToWebURL(chain_end));
      }
      ds->appendRedirect(chain_end);
      // Make sure we clear the expected redirect since we just effectively
      // completed it.
      expected_client_redirect_src_ = KURL();
      expected_client_redirect_dest_ = KURL();
     }

    // Regardless of how we got here, we are navigating to a URL so we need to
    // add it to the redirect chain.
    ds->appendRedirect(url);
  }

  bool is_new_navigation;
  webview->DidCommitLoad(&is_new_navigation);
  if (webframe_->client()) {
    webframe_->client()->didChangeLocationWithinPage(
        webframe_, is_new_navigation);
  }

  if (webview->client())
    webview->client()->didStopLoading();
}

void WebFrameLoaderClient::dispatchWillClose() {
  if (webframe_->client())
    webframe_->client()->willClose(webframe_);
}

void WebFrameLoaderClient::dispatchDidReceiveIcon() {
  // The icon database is disabled, so this should never be called.
  ASSERT_NOT_REACHED();
}

void WebFrameLoaderClient::dispatchDidStartProvisionalLoad() {
  // In case a redirect occurs, we need this to be set so that the redirect
  // handling code can tell where the redirect came from. Server redirects
  // will occur on the provisional load, so we need to keep track of the most
  // recent provisional load URL.
  // See dispatchDidReceiveServerRedirectForProvisionalLoad.
  WebDataSourceImpl* ds = webframe_->GetProvisionalDataSourceImpl();
  if (!ds) {
    ASSERT_NOT_REACHED();
    return;
  }
  KURL url = webkit_glue::WebURLToKURL(ds->request().url());

  // Since the provisional load just started, we should have not gotten
  // any redirects yet.
  ASSERT(!ds->hasRedirectChain());

  // If this load is what we expected from a client redirect, treat it as a
  // redirect from that original page. The expected redirect urls will be
  // cleared by DidCancelClientRedirect.
  bool completing_client_redirect = false;
  if (expected_client_redirect_src_.isValid()) {
    // expected_client_redirect_dest_ could be something like
    // "javascript:history.go(-1)" thus we need to exclude url starts with
    // "javascript:". See bug: 1080873
    ASSERT(expected_client_redirect_dest_.protocolIs("javascript") ||
           expected_client_redirect_dest_ == url);
    ds->appendRedirect(expected_client_redirect_src_);
    completing_client_redirect = true;
  }
  ds->appendRedirect(url);

  if (webframe_->client()) {
    // As the comment for DidCompleteClientRedirect in webview_delegate.h
    // points out, whatever information its invocation contains should only
    // be considered relevant until the next provisional load has started.
    // So we first tell the delegate that the load started, and then tell it
    // about the client redirect the load is responsible for completing.
    webframe_->client()->didStartProvisionalLoad(webframe_);
    if (completing_client_redirect)
      webframe_->client()->didCompleteClientRedirect(
          webframe_, webkit_glue::KURLToWebURL(expected_client_redirect_src_));
  }
}

void WebFrameLoaderClient::dispatchDidReceiveTitle(const String& title) {
  if (webframe_->client()) {
    webframe_->client()->didReceiveTitle(
        webframe_, webkit_glue::StringToWebString(title));
  }
}

void WebFrameLoaderClient::dispatchDidCommitLoad() {
  WebViewImpl* webview = webframe_->GetWebViewImpl();
  bool is_new_navigation;
  webview->DidCommitLoad(&is_new_navigation);

  if (webframe_->client()) {
    webframe_->client()->didCommitProvisionalLoad(
        webframe_, is_new_navigation);
  }

  WebDevToolsAgentImpl* tools_agent = webview->GetWebDevToolsAgentImpl();
  if (tools_agent) {
    tools_agent->DidCommitLoadForFrame(webview, webframe_, is_new_navigation);
  }
}

void WebFrameLoaderClient::dispatchDidFailProvisionalLoad(
    const ResourceError& error) {
  // If a policy change occured, then we do not want to inform the plugin
  // delegate.  See bug 907789 for details.
  // TODO(darin): This means the plugin won't receive NPP_URLNotify, which
  // seems like it could result in a memory leak in the plugin!!
  if (error.domain() == kInternalErrorDomain &&
      error.errorCode() == ERR_POLICY_CHANGE) {
    webframe_->DidFail(cancelledError(error.failingURL()), true);
    return;
  }

  OwnPtr<WebPluginLoadObserver> plugin_load_observer = GetPluginLoadObserver();
  webframe_->DidFail(error, true);
  if (plugin_load_observer)
    plugin_load_observer->didFailLoading(error);
}

void WebFrameLoaderClient::dispatchDidFailLoad(const ResourceError& error) {
  OwnPtr<WebPluginLoadObserver> plugin_load_observer = GetPluginLoadObserver();
  webframe_->DidFail(error, false);
  if (plugin_load_observer)
    plugin_load_observer->didFailLoading(error);

  // Don't clear the redirect chain, this will happen in the middle of client
  // redirects, and we need the context. The chain will be cleared when the
  // provisional load succeeds or fails, not the "real" one.
}

void WebFrameLoaderClient::dispatchDidFinishLoad() {
  OwnPtr<WebPluginLoadObserver> plugin_load_observer = GetPluginLoadObserver();

  if (webframe_->client())
    webframe_->client()->didFinishLoad(webframe_);

  if (plugin_load_observer)
    plugin_load_observer->didFinishLoading();

  // Don't clear the redirect chain, this will happen in the middle of client
  // redirects, and we need the context. The chain will be cleared when the
  // provisional load succeeds or fails, not the "real" one.
}

void WebFrameLoaderClient::dispatchDidFirstLayout() {
}

void WebFrameLoaderClient::dispatchDidFirstVisuallyNonEmptyLayout() {
  // FIXME: called when webkit finished layout of a page that was visually
  // non-empty.
  // All resources have not necessarily finished loading.
}

Frame* WebFrameLoaderClient::dispatchCreatePage() {
  struct WebCore::WindowFeatures features;
  Page* new_page = webframe_->frame()->page()->chrome()->createWindow(
      webframe_->frame(), FrameLoadRequest(), features);

  // Make sure that we have a valid disposition.  This should have been set in
  // the preceeding call to dispatchDecidePolicyForNewWindowAction.
  ASSERT(next_navigation_policy_ != WebKit::WebNavigationPolicyIgnore);
  WebNavigationPolicy policy = next_navigation_policy_;
  next_navigation_policy_ = WebKit::WebNavigationPolicyIgnore;

  // createWindow can return NULL (e.g., popup blocker denies the window).
  if (!new_page)
    return NULL;

  WebViewImpl::FromPage(new_page)->set_initial_navigation_policy(policy);
  return new_page->mainFrame();
}

void WebFrameLoaderClient::dispatchShow() {
  WebViewImpl* webview = webframe_->GetWebViewImpl();
  if (webview && webview->client())
    webview->client()->show(webview->initial_navigation_policy());
}

static bool TreatAsAttachment(const ResourceResponse& response) {
  const String& content_disposition =
      response.httpHeaderField("Content-Disposition");
  if (content_disposition.isEmpty())
    return false;

  // Some broken sites just send
  // Content-Disposition: ; filename="file"
  // screen those out here.
  if (content_disposition.startsWith(";"))
    return false;

  if (content_disposition.startsWith("inline", false))
    return false;

  // Some broken sites just send
  // Content-Disposition: filename="file"
  // without a disposition token... screen those out.
  if (content_disposition.startsWith("filename", false))
    return false;

  // Also in use is Content-Disposition: name="file"
  if (content_disposition.startsWith("name", false))
    return false;

  // We have a content-disposition of "attachment" or unknown.
  // RFC 2183, section 2.8 says that an unknown disposition
  // value should be treated as "attachment"
  return true;
}

void WebFrameLoaderClient::dispatchDecidePolicyForMIMEType(
     FramePolicyFunction function,
     const String& mime_type,
     const ResourceRequest&) {
  const ResourceResponse& response =
      webframe_->frame()->loader()->activeDocumentLoader()->response();

  PolicyAction action;

  int status_code = response.httpStatusCode();
  if (status_code == 204 || status_code == 205) {
    // The server does not want us to replace the page contents.
    action = PolicyIgnore;
  } else if (TreatAsAttachment(response)) {
    // The server wants us to download instead of replacing the page contents.
    // Downloading is handled by the embedder, but we still get the initial
    // response so that we can ignore it and clean up properly.
    action = PolicyIgnore;
  } else if (!canShowMIMEType(mime_type)) {
    // Make sure that we can actually handle this type internally.
    action = PolicyIgnore;
  } else {
    // OK, we will render this page.
    action = PolicyUse;
  }

  // NOTE: ERR_POLICY_CHANGE will be generated when action is not PolicyUse.
  (webframe_->frame()->loader()->policyChecker()->*function)(action);
}

void WebFrameLoaderClient::dispatchDecidePolicyForNewWindowAction(
    WebCore::FramePolicyFunction function,
    const WebCore::NavigationAction& action,
    const WebCore::ResourceRequest& request,
    PassRefPtr<WebCore::FormState> form_state,
    const WebCore::String& frame_name) {
  WebNavigationPolicy navigation_policy;
  if (!ActionSpecifiesNavigationPolicy(action, &navigation_policy))
    navigation_policy = WebKit::WebNavigationPolicyNewForegroundTab;

  PolicyAction policy_action;
  if (navigation_policy == WebKit::WebNavigationPolicyDownload) {
    policy_action = PolicyDownload;
  } else {
    policy_action = PolicyUse;

    // Remember the disposition for when dispatchCreatePage is called.  It is
    // unfortunate that WebCore does not provide us with any context when
    // creating or showing the new window that would allow us to avoid having
    // to keep this state.
    next_navigation_policy_ = navigation_policy;
  }
  (webframe_->frame()->loader()->policyChecker()->*function)(policy_action);
}

void WebFrameLoaderClient::dispatchDecidePolicyForNavigationAction(
    WebCore::FramePolicyFunction function,
    const WebCore::NavigationAction& action,
    const WebCore::ResourceRequest& request,
    PassRefPtr<WebCore::FormState> form_state) {
  PolicyAction policy_action = PolicyIgnore;

  // It is valid for this function to be invoked in code paths where the
  // the webview is closed.
  // The NULL check here is to fix a crash that seems strange
  // (see - https://bugs.webkit.org/show_bug.cgi?id=23554).
  if (webframe_->client() && !request.url().isNull()) {
    WebNavigationPolicy navigation_policy =
        WebKit::WebNavigationPolicyCurrentTab;
    ActionSpecifiesNavigationPolicy(action, &navigation_policy);

    // Give the delegate a chance to change the navigation policy.
    const WebDataSourceImpl* ds = webframe_->GetProvisionalDataSourceImpl();
    if (ds) {
      KURL url = webkit_glue::WebURLToKURL(ds->request().url());
      if (url.protocolIs(WebKit::backForwardNavigationScheme)) {
        HandleBackForwardNavigation(url);
        navigation_policy = WebKit::WebNavigationPolicyIgnore;
      } else {
        bool is_redirect = ds->hasRedirectChain();

        WebNavigationType webnav_type =
            WebDataSourceImpl::toWebNavigationType(action.type());

        RefPtr<WebCore::Node> node;
        for (const Event* event = action.event(); event;
            event = event->underlyingEvent()) {
          if (event->isMouseEvent()) {
            const MouseEvent* mouse_event =
                static_cast<const MouseEvent*>(event);
            node = webframe_->frame()->eventHandler()->hitTestResultAtPoint(
                mouse_event->absoluteLocation(), false).innerNonSharedNode();
            break;
          }
        }
        WebNode originating_node = webkit_glue::NodeToWebNode(node);

        navigation_policy = webframe_->client()->decidePolicyForNavigation(
            webframe_, ds->request(), webnav_type, originating_node,
            navigation_policy, is_redirect);
      }
    }

    if (navigation_policy == WebKit::WebNavigationPolicyCurrentTab) {
      policy_action = PolicyUse;
    } else if (navigation_policy == WebKit::WebNavigationPolicyDownload) {
      policy_action = PolicyDownload;
    } else {
      if (navigation_policy != WebKit::WebNavigationPolicyIgnore) {
        WrappedResourceRequest webreq(request);
        webframe_->client()->loadURLExternally(
            webframe_, webreq, navigation_policy);
      }
      policy_action = PolicyIgnore;
    }
  }

  (webframe_->frame()->loader()->policyChecker()->*function)(policy_action);
}

void WebFrameLoaderClient::cancelPolicyCheck() {
  // FIXME
}

void WebFrameLoaderClient::dispatchUnableToImplementPolicy(
    const ResourceError& error) {
  WebKit::WebURLError url_error =
      webkit_glue::ResourceErrorToWebURLError(error);
  webframe_->client()->unableToImplementPolicyWithError(webframe_, url_error);
}

void WebFrameLoaderClient::dispatchWillSubmitForm(FramePolicyFunction function,
    PassRefPtr<FormState> form_ref) {
  if (webframe_->client()) {
    webframe_->client()->willSubmitForm(
        webframe_, webkit_glue::HTMLFormElementToWebForm(form_ref->form()));
  }
  (webframe_->frame()->loader()->policyChecker()->*function)(PolicyUse);
}

void WebFrameLoaderClient::dispatchDidLoadMainResource(DocumentLoader*) {
  // FIXME
}

void WebFrameLoaderClient::revertToProvisionalState(DocumentLoader*) {
  has_representation_ = true;
}

void WebFrameLoaderClient::setMainDocumentError(DocumentLoader*,
                                                const ResourceError& error) {
  if (plugin_widget_.get()) {
    if (sent_initial_response_to_plugin_) {
      plugin_widget_->didFailLoading(error);
      sent_initial_response_to_plugin_ = false;
    }
    plugin_widget_ = NULL;
  }
}

void WebFrameLoaderClient::postProgressStartedNotification() {
  WebViewImpl* webview = webframe_->GetWebViewImpl();
  if (webview && webview->client())
    webview->client()->didStartLoading();
}

void WebFrameLoaderClient::postProgressEstimateChangedNotification() {
  // FIXME
}

void WebFrameLoaderClient::postProgressFinishedNotification() {
  // TODO(ericroman): why might the webview be null?
  // http://b/1234461
  WebViewImpl* webview = webframe_->GetWebViewImpl();
  if (webview && webview->client())
    webview->client()->didStopLoading();
}

void WebFrameLoaderClient::setMainFrameDocumentReady(bool ready) {
  // FIXME
}

// Creates a new connection and begins downloading from that (contrast this
// with |download|).
void WebFrameLoaderClient::startDownload(const ResourceRequest& request) {
  if (webframe_->client()) {
    WrappedResourceRequest webreq(request);
    webframe_->client()->loadURLExternally(
        webframe_, webreq, WebKit::WebNavigationPolicyDownload);
  }
}

void WebFrameLoaderClient::willChangeTitle(DocumentLoader*) {
  // FIXME
}

void WebFrameLoaderClient::didChangeTitle(DocumentLoader*) {
  // FIXME
}

// Called whenever data is received.
void WebFrameLoaderClient::committedLoad(DocumentLoader* loader, const char* data, int length) {
  if (!plugin_widget_.get()) {
    if (webframe_->client()) {
      bool preventDefault = false;
      webframe_->client()->didReceiveDocumentData(webframe_, data, length, preventDefault);
      if (!preventDefault)
        webframe_->commitDocumentData(data, length);
    }
  }

  // If we are sending data to WebCore::MediaDocument, we should stop here
  // and cancel the request.
  if (webframe_->frame()->document() &&
      webframe_->frame()->document()->isMediaDocument()) {
    loader->cancelMainResourceLoad(
        pluginWillHandleLoadError(loader->response()));
  }

  // The plugin widget could have been created in the webframe_->DidReceiveData
  // function.
  if (plugin_widget_.get()) {
    if (!sent_initial_response_to_plugin_) {
      sent_initial_response_to_plugin_ = true;
      plugin_widget_->didReceiveResponse(
          webframe_->frame()->loader()->activeDocumentLoader()->response());
    }
    plugin_widget_->didReceiveData(data, length);
  }
}

void WebFrameLoaderClient::finishedLoading(DocumentLoader* dl) {
  if (plugin_widget_.get()) {
    plugin_widget_->didFinishLoading();
    plugin_widget_ = NULL;
    sent_initial_response_to_plugin_ = false;
  } else {
    // This is necessary to create an empty document. See bug 634004.
    // However, we only want to do this if makeRepresentation has been called, to
    // match the behavior on the Mac.
    if (has_representation_)
      dl->frameLoader()->setEncoding("", false);
  }
}

void WebFrameLoaderClient::updateGlobalHistory() {
}

void WebFrameLoaderClient::updateGlobalHistoryRedirectLinks() {
}

bool WebFrameLoaderClient::shouldGoToHistoryItem(HistoryItem*) const {
  // FIXME
  return true;
}

void WebFrameLoaderClient::didDisplayInsecureContent() {
  if (webframe_->client())
    webframe_->client()->didDisplayInsecureContent(webframe_);
}

void WebFrameLoaderClient::didRunInsecureContent(SecurityOrigin* origin) {
  if (webframe_->client()) {
    webframe_->client()->didRunInsecureContent(webframe_,
        webkit_glue::SecurityOriginToWebSecurityOrigin(origin));
  }
}

ResourceError WebFrameLoaderClient::blockedError(const WebCore::ResourceRequest&) {
  // FIXME
  return ResourceError();
}

ResourceError WebFrameLoaderClient::cancelledError(
    const ResourceRequest& request) {
  if (!webframe_->client())
    return ResourceError();

  return webkit_glue::WebURLErrorToResourceError(
      webframe_->client()->cancelledError(
          webframe_, WrappedResourceRequest(request)));
}

ResourceError WebFrameLoaderClient::cannotShowURLError(
    const ResourceRequest& request) {
  if (!webframe_->client())
    return ResourceError();

  return webkit_glue::WebURLErrorToResourceError(
      webframe_->client()->cannotHandleRequestError(
          webframe_, WrappedResourceRequest(request)));
}

ResourceError WebFrameLoaderClient::interruptForPolicyChangeError(
    const ResourceRequest& request) {
  return ResourceError(kInternalErrorDomain, ERR_POLICY_CHANGE,
                       request.url().string(), String());
}

ResourceError WebFrameLoaderClient::cannotShowMIMETypeError(const ResourceResponse&) {
  // FIXME
  return ResourceError();
}

ResourceError WebFrameLoaderClient::fileDoesNotExistError(const ResourceResponse&) {
  // FIXME
  return ResourceError();
}

ResourceError WebFrameLoaderClient::pluginWillHandleLoadError(const WebCore::ResourceResponse&) {
  // FIXME
  return ResourceError();
}

bool WebFrameLoaderClient::shouldFallBack(const ResourceError& error) {
  // This method is called when we fail to load the URL for an <object> tag
  // that has fallback content (child elements) and is being loaded as a frame.
  // The error parameter indicates the reason for the load failure.
  // We should let the fallback content load only if this wasn't a cancelled
  // request.
  // Note: The mac version also has a case for "WebKitErrorPluginWillHandleLoad"
  ResourceError cancelled_error = cancelledError(ResourceRequest());
  return error.errorCode() != cancelled_error.errorCode() ||
         error.domain() != cancelled_error.domain();
}

bool WebFrameLoaderClient::canHandleRequest(
    const ResourceRequest& request) const {
  return webframe_->client()->canHandleRequest(
      webframe_, WrappedResourceRequest(request));
}

bool WebFrameLoaderClient::canShowMIMEType(const String& mime_type) const {
  // This method is called to determine if the media type can be shown
  // "internally" (i.e. inside the browser) regardless of whether or not the
  // browser or a plugin is doing the rendering.

  // mime_type strings are supposed to be ASCII, but if they are not for some
  // reason, then it just means that the mime type will fail all of these "is
  // supported" checks and go down the path of an unhandled mime type.
  if (net::IsSupportedMimeType(
          webkit_glue::CStringToStdString(mime_type.latin1())))
    return true;

  // If Chrome is started with the --disable-plugins switch, pluginData is null.
  WebCore::PluginData* plugin_data = webframe_->frame()->page()->pluginData();

  // See if the type is handled by an installed plugin, if so, we can show it.
  // TODO(beng): (http://b/1085524) This is the place to stick a preference to
  //             disable full page plugins (optionally for certain types!)
  return !mime_type.isEmpty() && plugin_data && plugin_data->supportsMimeType(mime_type);
}

bool WebFrameLoaderClient::representationExistsForURLScheme(const String& URLScheme) const {
  // FIXME
  return false;
}

String WebFrameLoaderClient::generatedMIMETypeForURLScheme(const String& URLScheme) const {
  // This appears to generate MIME types for protocol handlers that are handled
  // internally. The only place I can find in the WebKit code that uses this
  // function is WebView::registerViewClass, where it is used as part of the
  // process by which custom view classes for certain document representations
  // are registered.
  String mimetype("x-apple-web-kit/");
  mimetype.append(URLScheme.lower());
  return mimetype;
}

void WebFrameLoaderClient::frameLoadCompleted() {
  // FIXME: the mac port also conditionally calls setDrawsBackground:YES on
  // it's ScrollView here.

  // This comment from the Mac port:
  // Note: Can be called multiple times.
  // Even if already complete, we might have set a previous item on a frame that
  // didn't do any data loading on the past transaction. Make sure to clear these out.

  // FIXME: setPreviousHistoryItem() no longer exists. http://crbug.com/8566
  // webframe_->frame()->loader()->setPreviousHistoryItem(0);
}

void WebFrameLoaderClient::saveViewStateToItem(HistoryItem*) {
  // FIXME
}


void WebFrameLoaderClient::restoreViewState() {
  // FIXME: probably scrolls to last position when you go back or forward
}

void WebFrameLoaderClient::provisionalLoadStarted() {
  // FIXME: On mac, this does various caching stuff
}

void WebFrameLoaderClient::didFinishLoad() {
  OwnPtr<WebPluginLoadObserver> plugin_load_observer = GetPluginLoadObserver();
  if (plugin_load_observer)
    plugin_load_observer->didFinishLoading();
}

void WebFrameLoaderClient::prepareForDataSourceReplacement() {
  // FIXME
}

PassRefPtr<DocumentLoader> WebFrameLoaderClient::createDocumentLoader(
    const ResourceRequest& request,
    const SubstituteData& data) {
  RefPtr<WebDataSourceImpl> ds = WebDataSourceImpl::create(request, data);
  if (webframe_->client())
    webframe_->client()->didCreateDataSource(webframe_, ds.get());
  return ds.release();
}

void WebFrameLoaderClient::setTitle(const String& title, const KURL& url) {
  // FIXME: monitor for changes in WebFrameLoaderClient.mm
  // FIXME: Set the title of the current history item. HistoryItemImpl's setter
  //        will notify its clients (e.g. the history database) that the title
  //        has changed.
  //
  // e.g.:
  // WebHistoryItem* item =
  // webframe_->GetWebViewImpl()->GetBackForwardList()->GetCurrentItem();
  // WebHistoryItemImpl* item_impl = static_cast<WebHistoryItemImpl*>(item);
  //
  // item_impl->SetTitle(webkit_glue::StringToStdWString(title));
}

String WebFrameLoaderClient::userAgent(const KURL& url) {
  return webkit_glue::StdStringToString(
      webkit_glue::GetUserAgent(webkit_glue::KURLToGURL(url)));
}

void WebFrameLoaderClient::savePlatformDataToCachedFrame(WebCore::CachedFrame*) {
  // The page cache should be disabled.
  ASSERT_NOT_REACHED();
}

void WebFrameLoaderClient::transitionToCommittedFromCachedFrame(WebCore::CachedFrame*) {
  ASSERT_NOT_REACHED();
}

// Called when the FrameLoader goes into a state in which a new page load
// will occur.
void WebFrameLoaderClient::transitionToCommittedForNewPage() {
  makeDocumentView();
}

bool WebFrameLoaderClient::canCachePage() const {
  // Since we manage the cache, always report this page as non-cacheable to
  // FrameLoader.
  return false;
}

// Downloading is handled in the browser process, not WebKit. If we get to this
// point, our download detection code in the ResourceDispatcherHost is broken!
void WebFrameLoaderClient::download(ResourceHandle* handle,
                                    const ResourceRequest& request,
                                    const ResourceRequest& initialRequest,
                                    const ResourceResponse& response) {
  ASSERT_NOT_REACHED();
}

PassRefPtr<Frame> WebFrameLoaderClient::createFrame(
    const KURL& url,
    const String& name,
    HTMLFrameOwnerElement* owner_element,
    const String& referrer,
    bool allows_scrolling,
    int margin_width,
    int margin_height) {
  FrameLoadRequest frame_request(ResourceRequest(url, referrer), name);
  return webframe_->CreateChildFrame(frame_request, owner_element);
}

PassRefPtr<Widget> WebFrameLoaderClient::createPlugin(
    const IntSize& size, // TODO(erikkay): how do we use this?
    HTMLPlugInElement* element,
    const KURL& url,
    const Vector<String>& param_names,
    const Vector<String>& param_values,
    const String& mime_type,
    bool load_manually) {
#if !PLATFORM(WIN_OS)
  // WebCore asks us to make a plugin even if we don't have a
  // registered handler, with a comment saying it's so we can display
  // the broken plugin icon.  In Chromium, we normally register a
  // fallback plugin handler that allows you to install a missing
  // plugin.  Since we don't yet have a default plugin handler, we
  // need to return NULL here rather than going through all the
  // plugin-creation IPCs only to discover we don't have a plugin
  // registered, which causes a crash.
  // TODO(evanm): remove me once we have a default plugin.
  if (objectContentType(url, mime_type) != ObjectContentNetscapePlugin)
    return NULL;
#endif

  if (!webframe_->client())
    return NULL;

  WebPluginParams params;
  params.url = webkit_glue::KURLToWebURL(url);
  params.mimeType = webkit_glue::StringToWebString(mime_type);
  CopyStringVector(param_names, &params.attributeNames);
  CopyStringVector(param_values, &params.attributeValues);
  params.loadManually = load_manually;

  WebPlugin* webplugin = webframe_->client()->createPlugin(webframe_, params);
  if (!webplugin)
    return NULL;

  // The container takes ownership of the WebPlugin.
  RefPtr<WebPluginContainerImpl> container =
      WebPluginContainerImpl::create(element, webplugin);

  if (!webplugin->initialize(container.get()))
    return NULL;

  // The element might have been removed during plugin initialization!
  if (!element->renderer())
    return NULL;

  return container;
}

// This method gets called when a plugin is put in place of html content
// (e.g., acrobat reader).
void WebFrameLoaderClient::redirectDataToPlugin(Widget* pluginWidget) {
  plugin_widget_ = static_cast<WebPluginContainerImpl*>(pluginWidget);
  ASSERT(plugin_widget_.get());
}

PassRefPtr<Widget> WebFrameLoaderClient::createJavaAppletWidget(
                                           const IntSize& size,
                                           HTMLAppletElement* element,
                                           const KURL& /* base_url */,
                                           const Vector<String>& param_names,
                                           const Vector<String>& param_values) {
  return createPlugin(size, element, KURL(), param_names, param_values,
      "application/x-java-applet", false);
}

ObjectContentType WebFrameLoaderClient::objectContentType(
    const KURL& url,
    const String& explicit_mime_type) {
  // This code is based on Apple's implementation from
  // WebCoreSupport/WebFrameBridge.mm.

  String mime_type = explicit_mime_type;
  if (mime_type.isEmpty()) {
    // Try to guess the MIME type based off the extension.
    String filename = url.lastPathComponent();
    int extension_pos = filename.reverseFind('.');
    if (extension_pos >= 0)
      mime_type = MIMETypeRegistry::getMIMETypeForPath(url.path());

    if (mime_type.isEmpty())
      return ObjectContentFrame;
  }

  if (MIMETypeRegistry::isSupportedImageMIMEType(mime_type))
    return ObjectContentImage;

  // If Chrome is started with the --disable-plugins switch, pluginData is null.
  PluginData* plugin_data = webframe_->frame()->page()->pluginData();
  if (plugin_data && plugin_data->supportsMimeType(mime_type))
    return ObjectContentNetscapePlugin;

  if (MIMETypeRegistry::isSupportedNonImageMIMEType(mime_type))
    return ObjectContentFrame;

  return ObjectContentNone;
}

String WebFrameLoaderClient::overrideMediaType() const {
  // FIXME
  return String();
}

bool WebFrameLoaderClient::ActionSpecifiesNavigationPolicy(
    const WebCore::NavigationAction& action,
    WebNavigationPolicy* policy) {
  if ((action.type() != NavigationTypeLinkClicked) ||
      !action.event()->isMouseEvent())
    return false;

  const MouseEvent* event = static_cast<const MouseEvent*>(action.event());
  return WebViewImpl::NavigationPolicyFromMouseEvent(event->button(),
      event->ctrlKey(), event->shiftKey(), event->altKey(), event->metaKey(),
      policy);
}

void WebFrameLoaderClient::HandleBackForwardNavigation(const KURL& url) {
  ASSERT(url.protocolIs(WebKit::backForwardNavigationScheme));

  bool ok;
  int offset = url.lastPathComponent().toIntStrict(&ok);
  if (!ok)
    return;

  WebViewImpl* webview = webframe_->GetWebViewImpl();
  if (webview->client())
    webview->client()->navigateBackForwardSoon(offset);
}

PassOwnPtr<WebPluginLoadObserver> WebFrameLoaderClient::GetPluginLoadObserver() {
  WebDataSourceImpl* ds = WebDataSourceImpl::fromDocumentLoader(
      webframe_->frame()->loader()->activeDocumentLoader());
  return ds->releasePluginLoadObserver();
}
