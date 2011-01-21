// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_MOCKS_MOCK_WEBFRAME_H_
#define WEBKIT_MOCKS_MOCK_WEBFRAME_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebHistoryItem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPerformance.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRange.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "v8/include/v8.h"

using WebKit::WebAnimationController;
using WebKit::WebCanvas;
using WebKit::WebConsoleMessage;
using WebKit::WebData;
using WebKit::WebDocument;
using WebKit::WebElement;
using WebKit::WebFindOptions;
using WebKit::WebFormElement;
using WebKit::WebFrame;
using WebKit::WebHistoryItem;
using WebKit::WebInputElement;
using WebKit::WebNode;
using WebKit::WebPasswordAutocompleteListener;
using WebKit::WebPerformance;
using WebKit::WebRange;
using WebKit::WebRect;
using WebKit::WebURLRequest;
using WebKit::WebSecurityOrigin;
using WebKit::WebScriptSource;
using WebKit::WebSize;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebURLLoader;
using WebKit::WebVector;
using WebKit::WebView;

namespace webkit_glue {

class MockWebFrame : public WebKit::WebFrame {
 public:
  MockWebFrame() {
  }

  virtual ~MockWebFrame() {
  }

  MOCK_METHOD2(setReferrerForRequest, void(WebURLRequest&, const WebURL&));
  MOCK_METHOD1(dispatchWillSendRequest, void(WebURLRequest&));

  // Methods from WebFrame that we don't care to mock.
  WEBKIT_API static int instanceCount() { return 0; }
  WEBKIT_API static WebFrame* frameForEnteredContext() { return NULL; }
  WEBKIT_API static WebFrame* frameForCurrentContext() { return NULL; }
  WEBKIT_API static WebFrame* fromFrameOwnerElement(const WebElement&) {
    return NULL;
  }

  virtual WebString name() const {
    return WebString();
  }
  virtual void setName(const WebString&) {}
  virtual long long identifier() const {
    return 0;
  }
  virtual WebURL url() const {
    return WebURL();
  }
  virtual WebURL favIconURL() const {
    return WebURL();
  }
  virtual WebURL openSearchDescriptionURL() const {
    return WebURL();
  }
  virtual WebString encoding() const {
    return WebString();
  }
  virtual void setCanHaveScrollbars(bool) {}
  virtual WebSize scrollOffset() const {
    return WebSize(0,0);
  }
  virtual WebSize contentsSize() const {
     return WebSize();
  }
  virtual int contentsPreferredWidth() const {
    return 0;
  }
  virtual int documentElementScrollHeight() const {
    return 0;
  }
  virtual bool hasVisibleContent() const {
    return false;
  }
  virtual WebView* view() const {
    return NULL;
  }
  virtual WebFrame* opener() const {
    return NULL;
  }
  virtual WebFrame* parent() const {
    return NULL;
  }
  virtual WebFrame* top() const {
    return NULL;
  }
  virtual WebFrame* firstChild() const {
    return NULL;
  }
  virtual WebFrame* lastChild() const {
    return NULL;
  }
  virtual WebFrame* nextSibling() const {
    return NULL;
  }
  virtual WebFrame* previousSibling() const {
    return NULL;
  }
  virtual WebFrame* traverseNext(bool wrap) const {
    return NULL;
  }
  virtual WebFrame* traversePrevious(bool wrap) const {
    return NULL;
  }
  virtual WebFrame* findChildByName(const WebString& name) const {
    return NULL;
  }
  virtual WebFrame* findChildByExpression(const WebString& xpath) const {
    return NULL;
  }
  virtual WebDocument document() const {
    return WebDocument();
  }
  virtual void forms(WebVector<WebFormElement>&) const {}
  virtual WebAnimationController* animationController() {
    return NULL;
  }
  virtual WebPerformance performance() const {
    return WebPerformance();
  }
  virtual WebSecurityOrigin securityOrigin() const {
    return WebSecurityOrigin();
  }
  virtual void grantUniversalAccess() {}
  virtual NPObject* windowObject() const {
    return NULL;
  }
  virtual void bindToWindowObject(const WebString& name, NPObject*) {}
  virtual void executeScript(const WebScriptSource&) {}
  virtual void executeScriptInIsolatedWorld(
      int worldId, const WebScriptSource* sources, unsigned numSources,
      int extensionGroup) {}
  virtual void addMessageToConsole(const WebConsoleMessage&) {}
  virtual void collectGarbage() {}
#if WEBKIT_USING_V8
  virtual v8::Handle<v8::Value> executeScriptAndReturnValue(
      const WebScriptSource&) {
    return v8::Handle<v8::Value>();
  }
  virtual v8::Local<v8::Context> mainWorldScriptContext() const {
    return v8::Local<v8::Context>();
  }
#endif
  virtual bool insertStyleText(const WebString& styleText,
                               const WebString& elementId) {
    return false;
  }
  virtual void reload(bool ignoreCache = false) {}
  virtual void loadRequest(const WebURLRequest&) {}
  virtual void loadHistoryItem(const WebHistoryItem&) {}
  virtual void loadData(const WebData& data,
                        const WebString& mimeType,
                        const WebString& textEncoding,
                        const WebURL& baseURL,
                        const WebURL& unreachableURL = WebURL(),
                        bool replace = false) {}
  virtual void loadHTMLString(const WebData& html,
                              const WebURL& baseURL,
                              const WebURL& unreachableURL = WebURL(),
                              bool replace = false) {}
  virtual bool isLoading() const {
    return false;
  }
  virtual void stopLoading() {}
  virtual WebKit::WebDataSource* provisionalDataSource() const {
    return NULL;
  }
  virtual WebKit::WebDataSource* dataSource() const {
    return NULL;
  }
  virtual WebHistoryItem previousHistoryItem() const {
    return WebHistoryItem();
  }
  virtual WebHistoryItem currentHistoryItem() const {
    return WebHistoryItem();
  }
  virtual void enableViewSourceMode(bool) {}
  virtual bool isViewSourceModeEnabled() const {
    return false;
  }
  // The next two methods were mocked above.
  // virtual void setReferrerForRequest(WebURLRequest&, const WebURL&) {}
  // virtual void dispatchWillSendRequest(WebURLRequest&) {}
  virtual WebURLLoader* createAssociatedURLLoader() {
    return NULL;
  }
  virtual void commitDocumentData(const char* data, size_t length) {}
  virtual unsigned unloadListenerCount() const {
    return 0;
  }
  virtual bool isProcessingUserGesture() const {
    return false;
  }
  virtual bool willSuppressOpenerInNewFrame() const {
    return false;
  }
  virtual void replaceSelection(const WebString& text) {}
  virtual void insertText(const WebString& text) {}
  virtual void setMarkedText(const WebString& text,
                             unsigned location,
                             unsigned length) {}
  virtual void unmarkText() {}
  virtual bool hasMarkedText() const {
    return false;
  }
  virtual WebRange markedRange() const {
    return WebRange();
  }
  virtual bool firstRectForCharacterRange(unsigned location,
                                          unsigned length,
                                          WebRect&) const {
    return false;
  }
  virtual bool executeCommand(const WebString&) {
    return false;
  }
  virtual bool executeCommand(const WebString&, const WebString& value) {
    return false;
  }
  virtual bool isCommandEnabled(const WebString&) const {
    return false;
  }
  virtual void enableContinuousSpellChecking(bool) {}
  virtual bool isContinuousSpellCheckingEnabled() const {
    return false;
  }
  virtual bool hasSelection() const {
    return false;
  }
  virtual WebRange selectionRange() const {
    return WebRange();
  }
  virtual WebString selectionAsText() const {
    return WebString();
  }
  virtual WebString selectionAsMarkup() const {
    return WebString();
  }
  virtual bool selectWordAroundCaret() {
    return false;
  }
  virtual int printBegin(const WebSize& pageSize,
                         const WebNode& constrainToNode,
                         int printerDPI = 72,
                         bool* useBrowserOverlays = 0) {
    return 0;
  }
  virtual float getPrintPageShrink(int page) {
    return 0;
  }
  virtual float printPage(int pageToPrint, WebCanvas*) {
    return 0;
  }
  virtual void printEnd() {}
  virtual bool isPageBoxVisible(int pageIndex) {
    return false;
  }
  virtual void pageSizeAndMarginsInPixels(int pageIndex,
                                          WebSize& pageSize,
                                          int& marginTop,
                                          int& marginRight,
                                          int& marginBottom,
                                          int& marginLeft) {}
  virtual bool find(int identifier,
                    const WebString& searchText,
                    const WebFindOptions& options,
                    bool wrapWithinFrame,
                    WebRect* selectionRect) {
    return false;
  }
  virtual void stopFinding(bool clearSelection) {}
  virtual void scopeStringMatches(int identifier,
                                  const WebString& searchText,
                                  const WebFindOptions& options,
                                  bool reset) {}
  virtual void cancelPendingScopingEffort() {}
  virtual void increaseMatchCount(int count, int identifier) {}
  virtual void resetMatchCount() {}
  virtual bool registerPasswordListener(
      WebInputElement,
      WebPasswordAutocompleteListener*) {
    return false;
  }
  virtual void notifiyPasswordListenerOfAutocomplete(
      const WebInputElement&) {}
  virtual WebString contentAsText(size_t maxChars) const {
    return WebString();
  }
  virtual WebString contentAsMarkup() const {
    return WebString();
  }
  virtual WebString renderTreeAsText() const {
    return WebString();
  }
  virtual WebString counterValueForElementById(const WebString& id) const {
    return WebString();
  }
  virtual WebString markerTextForListItem(const WebElement&) const {
    return WebString();
  }
  virtual int pageNumberForElementById(const WebString& id,
                                       float pageWidthInPixels,
                                       float pageHeightInPixels) const {
    return 0;
  }
  virtual WebRect selectionBoundsRect() const {
    return WebRect();
  }
  virtual bool selectionStartHasSpellingMarkerFor(int from, int length) const {
    return false;
  }
  virtual bool pauseSVGAnimation(const WebString& animationId,
                                 double time,
                                 const WebString& elementId) {
    return false;
  }
  virtual WebString layerTreeAsText() const {
    return WebString();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWebFrame);
};

}  // namespace webkit_glue

#endif  // WEBKIT_MOCKS_MOCK_WEBFRAME_H_
