// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_EMPTY_WEBFRAMECLIENT_H_
#define WEBKIT_GLUE_EMPTY_WEBFRAMECLIENT_H_

#include "webkit/api/public/WebFrameClient.h"
#include "webkit/api/public/WebURLError.h"

namespace webkit_glue {

// Extend from this if you only need to override a few WebFrameClient methods.
class EmptyWebFrameClient : public WebKit::WebFrameClient {
 public:
  virtual WebKit::WebPlugin* createPlugin(
      WebKit::WebFrame* frame, const WebKit::WebPluginParams& params) {
    return NULL; }
  virtual WebKit::WebWorker* createWorker(
      WebKit::WebFrame* frame, WebKit::WebWorkerClient* client) {
    return NULL; }
  virtual WebKit::WebMediaPlayer* createMediaPlayer(
      WebKit::WebFrame* frame, WebKit::WebMediaPlayerClient* client) {
    return NULL; }
  virtual void willClose(WebKit::WebFrame* frame) {}
  virtual void loadURLExternally(
      WebKit::WebFrame* frame, const WebKit::WebURLRequest& request,
      WebKit::WebNavigationPolicy policy) {}
  virtual WebKit::WebNavigationPolicy decidePolicyForNavigation(
      WebKit::WebFrame* frame, const WebKit::WebURLRequest& request,
      WebKit::WebNavigationType type, const WebKit::WebNode& originating_node,
      WebKit::WebNavigationPolicy default_policy, bool is_redirect) {
    return default_policy; }
  virtual bool canHandleRequest(
      WebKit::WebFrame*, const WebKit::WebURLRequest&) { return true; }
  virtual WebKit::WebURLError cannotHandleRequestError(
      WebKit::WebFrame*, const WebKit::WebURLRequest& request) {
    return WebKit::WebURLError();
  }
  virtual WebKit::WebURLError cancelledError(
      WebKit::WebFrame*, const WebKit::WebURLRequest& request) {
    return WebKit::WebURLError();
  }
  virtual void unableToImplementPolicyWithError(
      WebKit::WebFrame*, const WebKit::WebURLError&) {}
  virtual void willSubmitForm(WebKit::WebFrame* frame,
      const WebKit::WebForm& form) {}
  virtual void willPerformClientRedirect(
      WebKit::WebFrame* frame, const WebKit::WebURL& from,
      const WebKit::WebURL& to, double interval, double fire_time) {}
  virtual void didCancelClientRedirect(WebKit::WebFrame* frame) {}
  virtual void didCompleteClientRedirect(
      WebKit::WebFrame* frame, const WebKit::WebURL& from) {}
  virtual void didCreateDataSource(
      WebKit::WebFrame* frame, WebKit::WebDataSource* datasource) {}
  virtual void didStartProvisionalLoad(WebKit::WebFrame* frame) {}
  virtual void didReceiveServerRedirectForProvisionalLoad(
      WebKit::WebFrame* frame) {}
  virtual void didFailProvisionalLoad(
      WebKit::WebFrame* frame, const WebKit::WebURLError& error) {}
  virtual void didReceiveDocumentData(
      WebKit::WebFrame* frame, const char* data, size_t length,
      bool& prevent_default) {}
  virtual void didCommitProvisionalLoad(
      WebKit::WebFrame* frame, bool is_new_navigation) {}
  virtual void didClearWindowObject(WebKit::WebFrame* frame) {}
  virtual void didCreateDocumentElement(WebKit::WebFrame* frame) {}
  virtual void didReceiveTitle(
      WebKit::WebFrame* frame, const WebKit::WebString& title) {}
  virtual void didFinishDocumentLoad(WebKit::WebFrame* frame) {}
  virtual void didHandleOnloadEvents(WebKit::WebFrame* frame) {}
  virtual void didFailLoad(
      WebKit::WebFrame* frame, const WebKit::WebURLError& error) {}
  virtual void didFinishLoad(WebKit::WebFrame* frame) {}
  virtual void didChangeLocationWithinPage(
      WebKit::WebFrame* frame, bool is_new_navigation) {}
  virtual void didUpdateCurrentHistoryItem(WebKit::WebFrame* frame) {}
  virtual void assignIdentifierToRequest(
      WebKit::WebFrame* frame, unsigned identifier,
      const WebKit::WebURLRequest& request) {}
  virtual void willSendRequest(
      WebKit::WebFrame* frame, unsigned identifier,
      WebKit::WebURLRequest& request,
      const WebKit::WebURLResponse& redirect_response) {}
  virtual void didReceiveResponse(
      WebKit::WebFrame* frame, unsigned identifier,
      const WebKit::WebURLResponse& response) {}
  virtual void didFinishResourceLoad(
      WebKit::WebFrame* frame, unsigned identifier) {}
  virtual void didFailResourceLoad(
      WebKit::WebFrame* frame, unsigned identifier,
      const WebKit::WebURLError& error) {}
  virtual void didLoadResourceFromMemoryCache(
      WebKit::WebFrame* frame, const WebKit::WebURLRequest& request,
      const WebKit::WebURLResponse&) {}
  virtual void didDisplayInsecureContent(WebKit::WebFrame* frame) {}
  virtual void didRunInsecureContent(
      WebKit::WebFrame* frame, const WebKit::WebSecurityOrigin& origin) {}
  virtual void didExhaustMemoryAvailableForScript(WebKit::WebFrame* frame) {}
  virtual void didCreateScriptContext(WebKit::WebFrame* frame) {}
  virtual void didDestroyScriptContext(WebKit::WebFrame* frame) {}
  virtual void didCreateIsolatedScriptContext(WebKit::WebFrame* frame) {}
  virtual void didChangeContentsSize(
      WebKit::WebFrame* frame, const WebKit::WebSize& size) {}
  virtual void reportFindInPageMatchCount(
      int identifier, int count, bool final_update) {}
  virtual void reportFindInPageSelection(
      int identifier, int ordinal, const WebKit::WebRect& selection) {}
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_EMPTY_WEBFRAMECLIENT_H_
