// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
  MockWebFrame();
  virtual ~MockWebFrame();

  MOCK_METHOD2(setReferrerForRequest, void(WebURLRequest&, const WebURL&));
  MOCK_METHOD1(dispatchWillSendRequest, void(WebURLRequest&));

  // Methods from WebFrame that we don't care to mock.
  WEBKIT_API static int instanceCount() { return 0; }
  WEBKIT_API static WebFrame* frameForEnteredContext() { return NULL; }
  WEBKIT_API static WebFrame* frameForCurrentContext() { return NULL; }
  WEBKIT_API static WebFrame* fromFrameOwnerElement(const WebElement&) {
    return NULL;
  }

  virtual WebString name() const;
  virtual void setName(const WebString&);
  virtual long long identifier() const;
  virtual WebURL url() const;
  virtual WebURL favIconURL() const;
  virtual WebURL openSearchDescriptionURL() const;
  virtual WebString encoding() const;
  virtual void setCanHaveScrollbars(bool);
  virtual WebSize scrollOffset() const;
  virtual void setScrollOffset(const WebSize&);
  virtual WebSize contentsSize() const;
  virtual int contentsPreferredWidth() const;
  virtual int documentElementScrollHeight() const;
  virtual bool hasVisibleContent() const;
  virtual WebView* view() const;
  virtual WebFrame* opener() const;
  virtual void clearOpener();
  virtual WebFrame* parent() const;
  virtual WebFrame* top() const;
  virtual WebFrame* firstChild() const;
  virtual WebFrame* lastChild() const;
  virtual WebFrame* nextSibling() const;
  virtual WebFrame* previousSibling() const;
  virtual WebFrame* traverseNext(bool wrap) const;
  virtual WebFrame* traversePrevious(bool wrap) const;
  virtual WebFrame* findChildByName(const WebString& name) const;
  virtual WebFrame* findChildByExpression(const WebString& xpath) const;
  virtual WebDocument document() const;
  virtual void forms(WebVector<WebFormElement>&) const;
  virtual WebAnimationController* animationController();
  virtual WebPerformance performance() const;
  virtual WebSecurityOrigin securityOrigin() const;
  virtual void grantUniversalAccess();
  virtual NPObject* windowObject() const;
  virtual void bindToWindowObject(const WebString& name, NPObject*);
  virtual void executeScript(const WebScriptSource&);
  virtual void executeScriptInIsolatedWorld(
      int worldId, const WebScriptSource* sources, unsigned numSources,
      int extensionGroup);
  virtual void addMessageToConsole(const WebConsoleMessage&);
  virtual void collectGarbage();
#if WEBKIT_USING_V8
  virtual v8::Handle<v8::Value> executeScriptAndReturnValue(
      const WebScriptSource&);
  virtual v8::Local<v8::Context> mainWorldScriptContext() const;
#endif
  virtual bool insertStyleText(const WebString& styleText,
                               const WebString& elementId);
  virtual void reload(bool ignoreCache = false);
  virtual void loadRequest(const WebURLRequest&);
  virtual void loadHistoryItem(const WebHistoryItem&);
  virtual void loadData(const WebData& data,
                        const WebString& mimeType,
                        const WebString& textEncoding,
                        const WebURL& baseURL,
                        const WebURL& unreachableURL = WebURL(),
                        bool replace = false);
  virtual void loadHTMLString(const WebData& html,
                              const WebURL& baseURL,
                              const WebURL& unreachableURL = WebURL(),
                              bool replace = false);
  virtual bool isLoading() const;
  virtual void stopLoading();
  virtual WebKit::WebDataSource* provisionalDataSource() const;
  virtual WebKit::WebDataSource* dataSource() const;
  virtual WebHistoryItem previousHistoryItem() const;
  virtual WebHistoryItem currentHistoryItem() const;
  virtual void enableViewSourceMode(bool);
  virtual bool isViewSourceModeEnabled() const;
  // The next two methods were mocked above.
  // virtual void setReferrerForRequest(WebURLRequest&, const WebURL&) {}
  // virtual void dispatchWillSendRequest(WebURLRequest&) {}
  virtual WebURLLoader* createAssociatedURLLoader();
  virtual void commitDocumentData(const char* data, size_t length);
  virtual unsigned unloadListenerCount() const;
  virtual bool isProcessingUserGesture() const;
  virtual bool willSuppressOpenerInNewFrame() const;
  virtual void replaceSelection(const WebString& text);
  virtual void insertText(const WebString& text);
  virtual void setMarkedText(const WebString& text,
                             unsigned location,
                             unsigned length);
  virtual void unmarkText();
  virtual bool hasMarkedText() const;
  virtual WebRange markedRange() const;
  virtual bool firstRectForCharacterRange(unsigned location,
                                          unsigned length,
                                          WebRect&) const;
  virtual bool executeCommand(const WebString&);
  virtual bool executeCommand(const WebString&, const WebString& value);
  virtual bool isCommandEnabled(const WebString&) const;
  virtual void enableContinuousSpellChecking(bool);
  virtual bool isContinuousSpellCheckingEnabled() const;
  virtual bool hasSelection() const;
  virtual WebRange selectionRange() const;
  virtual WebString selectionAsText() const;
  virtual WebString selectionAsMarkup() const;
  virtual bool selectWordAroundCaret();
  virtual int printBegin(const WebSize& pageSize,
                         const WebNode& constrainToNode,
                         int printerDPI = 72,
                         bool* useBrowserOverlays = 0);
  virtual float getPrintPageShrink(int page);
  virtual float printPage(int pageToPrint, WebCanvas*);
  virtual void printEnd();
  virtual bool isPageBoxVisible(int pageIndex);
  virtual void pageSizeAndMarginsInPixels(int pageIndex,
                                          WebSize& pageSize,
                                          int& marginTop,
                                          int& marginRight,
                                          int& marginBottom,
                                          int& marginLeft);
  virtual bool find(int identifier,
                    const WebString& searchText,
                    const WebFindOptions& options,
                    bool wrapWithinFrame,
                    WebRect* selectionRect);
  virtual void stopFinding(bool clearSelection);
  virtual void scopeStringMatches(int identifier,
                                  const WebString& searchText,
                                  const WebFindOptions& options,
                                  bool reset);
  virtual void cancelPendingScopingEffort();
  virtual void increaseMatchCount(int count, int identifier);
  virtual void resetMatchCount();
  virtual bool registerPasswordListener(
      WebInputElement,
      WebPasswordAutocompleteListener*);
  virtual void notifiyPasswordListenerOfAutocomplete(
      const WebInputElement&);
  virtual WebString contentAsText(size_t maxChars) const;
  virtual WebString contentAsMarkup() const;
  virtual WebString renderTreeAsText() const;
  virtual WebString counterValueForElementById(const WebString& id) const;
  virtual WebString markerTextForListItem(const WebElement&) const;
  virtual int pageNumberForElementById(const WebString& id,
                                       float pageWidthInPixels,
                                       float pageHeightInPixels) const;
  virtual WebRect selectionBoundsRect() const;
  virtual bool selectionStartHasSpellingMarkerFor(int from, int length) const;
  virtual bool pauseSVGAnimation(const WebString& animationId,
                                 double time,
                                 const WebString& elementId);
  virtual WebString layerTreeAsText() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWebFrame);
};

}  // namespace webkit_glue

#endif  // WEBKIT_MOCKS_MOCK_WEBFRAME_H_
